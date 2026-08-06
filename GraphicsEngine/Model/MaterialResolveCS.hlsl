#include "Model.hlsli"
#include "../Shader/Structured.hlsli"
#include "../Shader/Sampler.hlsli"
#include "../Shader/Normal.hlsli"
#include "../Shader/Constants.hlsli"

/**
* [EN]
* VisibilityBuffer material resolve pass, final step of the material sort
* (Model/MaterialClassifyCS.hlsl / MaterialPrefixSumCS.hlsl / MaterialScatterCS.hlsl).
* Dispatched 1D over the material-sorted pixel list instead of raw screen
* order, so neighbouring threads in a wave tend to share the same/nearby
* instance and its texture indices. For each entry, re-derives the covering
* triangle's 3 vertices from the same bindless buffers StaticModelMS/
* SkeletalModelMS read, reconstructs perspective-correct screen-space
* barycentric weights, interpolates attributes, samples materials, and
* rewrites RT0/1/2/3 (base_color+metallic / octNormal+roughness / velocity /
* emissive). Background pixels are handled by MaterialClassifyCS.hlsl (the
* only one of the four passes that walks the full screen), not here - the
* sorted list only ever contains foreground pixels. KHR extension scalars
* (ior/specular/clearcoat/transmission/volume/sheen/iridescence/anisotropy/
* unlit) are NOT baked in here - Model/DeferredLightingPS.hlsl reads them
* straight from ModelInstance via this same VisID at lighting time instead.
*
* Known limitation: texture sampling uses SampleLevel at mip 0 (no analytic
* UV derivatives yet), so mipmapped/anisotropic filtering does not match the
* raster pass exactly.
*
* ---------------------------------------------------------------------
*
* [JP]
* VisibilityBuffer のマテリアル解決パス。マテリアルソート
* (Model/MaterialClassifyCS.hlsl / MaterialPrefixSumCS.hlsl / MaterialScatterCS.hlsl)
* の最終段。生のスクリーン順ではなく、マテリアルでソート済みのピクセル
* リストを1Dディスパッチで辿るので、同じウェーブの隣接スレッドが同じ/近い
* インスタンスとそのテクスチャインデックスを共有しやすい。各エントリで、
* StaticModelMS/SkeletalModelMS と同じ bindless バッファから該当三角形の
* 3 頂点を再取得し、パースペクティブ正しいスクリーン空間重心座標を復元して
* 属性を補間、マテリアルをサンプルして RT0/1/2/3(base_color+metallic /
* octNormal+roughness / velocity / emissive)を書き直す。背景ピクセルは
* MaterialClassifyCS.hlsl(4パス中、全画面を走査する唯一のパス)が処理
* 済みでここでは扱わない - ソート済みリストには前景ピクセルしか入らない。
* KHR拡張のスカラー値(ior/specular/clearcoat/transmission/volume/sheen/
* iridescence/anisotropy/unlit)はここでは焼き込まない -
* Model/DeferredLightingPS.hlsl が同じVisID経由でライティング時に
* ModelInstanceから直接読む。
*
* 既知の制限: テクスチャサンプルは mip 0 固定の SampleLevel を使う
* (解析的な UV 偏微分は未実装)。ミップマップ/異方性フィルタはラスタパスと
* 完全には一致しない。
*/
[numthreads(64, 1, 1)]
void main(uint3 dtid : SV_DispatchThreadID)
{
	SceneConstantBuffer scene = GetSceneConstantBuffer();

	uint screen_width = (uint)scene.screen_size_.x;
	uint screen_height = (uint)scene.screen_size_.y;
	uint total_pixels = screen_width * screen_height;

	if (dtid.x >= total_pixels)
	{
		return;
	}

	RWByteAddressBuffer sorted_pixel_list = ResourceDescriptorHeap[structured_indices.material_sort_.sorted_pixel_list_index_];
	uint linear_pixel = sorted_pixel_list.Load(dtid.x * 4);
	if (linear_pixel == MATERIAL_SORT_INVALID_PIXEL)
	{
		return;
	}

	uint2 pixel = uint2(linear_pixel % screen_width, linear_pixel / screen_width);

	Texture2D<uint2> visibility_texture = ResourceDescriptorHeap[structured_indices.gbuffer_.index_4_];
	uint2 visibility_id = visibility_texture.Load(int3(pixel, 0));

	uint instance_index;
	uint meshlet_index;
	uint triangle_in_meshlet_index;
	UnpackVisibilityID(visibility_id, instance_index, meshlet_index, triangle_in_meshlet_index);

	StructuredBuffer<ModelInstance> instances = ResourceDescriptorHeap[structured_indices.model_.instance_index_];
	ModelInstance instance = instances[instance_index];

	StructuredBuffer<CompressedModelVertex> vertices = ResourceDescriptorHeap[instance.vertex_buffer_index_];
	StructuredBuffer<ModelMeshlet> meshlets = ResourceDescriptorHeap[instance.meshlet_buffer_index_];
	StructuredBuffer<uint> vertex_indices = ResourceDescriptorHeap[instance.vertex_indices_buffer_index_];
	ByteAddressBuffer primitive_indices = ResourceDescriptorHeap[instance.primitive_indices_buffer_index_];

	ModelMeshlet meshlet = meshlets[meshlet_index];

	/// [JP] StaticModelMS.hlsl / SkeletalModelMS.hlsl と同じ 3 バイト/三角形の
	///      パック解除(primitive_indices はメシュレットローカルの頂点番号 0..63)。
	uint byte_offset = meshlet.triangle_offset_ + triangle_in_meshlet_index * 3;
	uint aligned_offset = byte_offset & ~3;
	uint shift = (byte_offset & 3) * 8;

	uint dword0 = primitive_indices.Load(aligned_offset);
	uint packed = dword0 >> shift;
	if (shift > 8)
	{
		uint dword1 = primitive_indices.Load(aligned_offset + 4);
		packed |= dword1 << (32 - shift);
	}

	uint local_index0 = packed & 0xFF;
	uint local_index1 = (packed >> 8) & 0xFF;
	uint local_index2 = (packed >> 16) & 0xFF;

	uint global_index0 = vertex_indices[meshlet.vertex_offset_ + local_index0];
	uint global_index1 = vertex_indices[meshlet.vertex_offset_ + local_index1];
	uint global_index2 = vertex_indices[meshlet.vertex_offset_ + local_index2];

	ModelVertex vertex0 = DecodeModelVertex(vertices[global_index0], instance);
	ModelVertex vertex1 = DecodeModelVertex(vertices[global_index1], instance);
	ModelVertex vertex2 = DecodeModelVertex(vertices[global_index2], instance);

	float3 local_position0 = vertex0.position_;
	float3 local_position1 = vertex1.position_;
	float3 local_position2 = vertex2.position_;
	float3 local_normal0 = vertex0.normal_;
	float3 local_normal1 = vertex1.normal_;
	float3 local_normal2 = vertex2.normal_;
	float3 local_tangent0 = vertex0.tangent_.xyz;
	float3 local_tangent1 = vertex1.tangent_.xyz;
	float3 local_tangent2 = vertex2.tangent_.xyz;

	/// [JP] SkeletalModelMS.hlsl と同じリニアブレンドスキニング。
	///      instance.skin_index_ == 0xFFFFFFFF なら静的(未スキン)。
	if (instance.skin_index_ != 0xFFFFFFFF)
	{
		StructuredBuffer<ModelBoneMatrix> bone_matrices = ResourceDescriptorHeap[structured_indices.model_.bone_matrix_index_];
		StructuredBuffer<ModelSkinVertex> skin_vertices = ResourceDescriptorHeap[instance.skin_vertex_buffer_index_];

		uint4 joints0, joints1, joints2;
		float4 weights0, weights1, weights2;
		DecodeModelSkinVertex(skin_vertices[global_index0], joints0, weights0);
		DecodeModelSkinVertex(skin_vertices[global_index1], joints1, weights1);
		DecodeModelSkinVertex(skin_vertices[global_index2], joints2, weights2);

		float4x4 skin_matrix0 =
			LoadBoneMatrix(bone_matrices[instance.bone_offset_ + joints0.x]) * weights0.x +
			LoadBoneMatrix(bone_matrices[instance.bone_offset_ + joints0.y]) * weights0.y +
			LoadBoneMatrix(bone_matrices[instance.bone_offset_ + joints0.z]) * weights0.z +
			LoadBoneMatrix(bone_matrices[instance.bone_offset_ + joints0.w]) * weights0.w;
		float4x4 skin_matrix1 =
			LoadBoneMatrix(bone_matrices[instance.bone_offset_ + joints1.x]) * weights1.x +
			LoadBoneMatrix(bone_matrices[instance.bone_offset_ + joints1.y]) * weights1.y +
			LoadBoneMatrix(bone_matrices[instance.bone_offset_ + joints1.z]) * weights1.z +
			LoadBoneMatrix(bone_matrices[instance.bone_offset_ + joints1.w]) * weights1.w;
		float4x4 skin_matrix2 =
			LoadBoneMatrix(bone_matrices[instance.bone_offset_ + joints2.x]) * weights2.x +
			LoadBoneMatrix(bone_matrices[instance.bone_offset_ + joints2.y]) * weights2.y +
			LoadBoneMatrix(bone_matrices[instance.bone_offset_ + joints2.z]) * weights2.z +
			LoadBoneMatrix(bone_matrices[instance.bone_offset_ + joints2.w]) * weights2.w;

		local_position0 = mul(float4(local_position0, 1.0), skin_matrix0).xyz;
		local_position1 = mul(float4(local_position1, 1.0), skin_matrix1).xyz;
		local_position2 = mul(float4(local_position2, 1.0), skin_matrix2).xyz;
		local_normal0 = normalize(mul(float4(local_normal0, 0.0), skin_matrix0).xyz);
		local_normal1 = normalize(mul(float4(local_normal1, 0.0), skin_matrix1).xyz);
		local_normal2 = normalize(mul(float4(local_normal2, 0.0), skin_matrix2).xyz);
		local_tangent0 = normalize(mul(float4(local_tangent0, 0.0), skin_matrix0).xyz);
		local_tangent1 = normalize(mul(float4(local_tangent1, 0.0), skin_matrix1).xyz);
		local_tangent2 = normalize(mul(float4(local_tangent2, 0.0), skin_matrix2).xyz);
	}

	float3 world_position0 = mul(float4(local_position0, 1.0), instance.world_).xyz;
	float3 world_position1 = mul(float4(local_position1, 1.0), instance.world_).xyz;
	float3 world_position2 = mul(float4(local_position2, 1.0), instance.world_).xyz;

	float4 clip0 = mul(float4(world_position0, 1.0), scene.current_view_projection_);
	float4 clip1 = mul(float4(world_position1, 1.0), scene.current_view_projection_);
	float4 clip2 = mul(float4(world_position2, 1.0), scene.current_view_projection_);

	/// [JP] Culling.hlsli の IsBackFace と同じ NDC 空間の符号付き面積。
	float2 ndc0 = clip0.xy / clip0.w;
	float2 ndc1 = clip1.xy / clip1.w;
	float2 ndc2 = clip2.xy / clip2.w;

	float2 pixel_ndc = (float2(pixel) + 0.5) * scene.inverse_screen_size_;
	pixel_ndc = float2(pixel_ndc.x * 2.0 - 1.0, 1.0 - pixel_ndc.y * 2.0);

	/// [JP] エッジ関数によるスクリーン空間重心座標(w0,w1,w2)。
	float area = (ndc1.x - ndc0.x) * (ndc2.y - ndc0.y) - (ndc1.y - ndc0.y) * (ndc2.x - ndc0.x);

	float edge0 = (ndc2.x - ndc1.x) * (pixel_ndc.y - ndc1.y) - (ndc2.y - ndc1.y) * (pixel_ndc.x - ndc1.x);
	float edge1 = (ndc0.x - ndc2.x) * (pixel_ndc.y - ndc2.y) - (ndc0.y - ndc2.y) * (pixel_ndc.x - ndc2.x);
	float edge2 = (ndc1.x - ndc0.x) * (pixel_ndc.y - ndc0.y) - (ndc1.y - ndc0.y) * (pixel_ndc.x - ndc0.x);
	float screen_w0 = edge0 / area;
	float screen_w1 = edge1 / area;
	float screen_w2 = edge2 / area;

	/// [JP] パースペクティブ補正: screen_wN(NDC 上で線形)を invW で重み付けし
	///      正規化すると、属性を直接線形結合するだけで透視正しい補間になる。
	float inverse_w0 = 1.0 / clip0.w;
	float inverse_w1 = 1.0 / clip1.w;
	float inverse_w2 = 1.0 / clip2.w;
	float perspective_w0 = screen_w0 * inverse_w0;
	float perspective_w1 = screen_w1 * inverse_w1;
	float perspective_w2 = screen_w2 * inverse_w2;
	float perspective_sum = perspective_w0 + perspective_w1 + perspective_w2;
	perspective_w0 /= perspective_sum;
	perspective_w1 /= perspective_sum;
	perspective_w2 /= perspective_sum;

	float3 world_position = perspective_w0 * world_position0 + perspective_w1 * world_position1 + perspective_w2 * world_position2;
	float3 N = normalize(perspective_w0 * local_normal0 + perspective_w1 * local_normal1 + perspective_w2 * local_normal2);
	float3 world_tangent_xyz = normalize(perspective_w0 * local_tangent0 + perspective_w1 * local_tangent1 + perspective_w2 * local_tangent2);
	float tangent_sign = vertex0.tangent_.w;
	float2 texcoord = perspective_w0 * vertex0.texcoord_ + perspective_w1 * vertex1.texcoord_ + perspective_w2 * vertex2.texcoord_;

	/// [JP] StaticModelMS.hlsl 同様、法線/タンジェントはワールド変換の逆転置/通常行列を通す。
	N = normalize(mul(float4(N, 0.0), instance.inverse_transpose_world_).xyz);
	world_tangent_xyz = normalize(mul(float4(world_tangent_xyz, 0.0), instance.world_).xyz);

	/// [JP] 表裏判定はNDC空間の巻き順符号(D3Dのラスタライザ規約)に頼らず、
	///      ワールド空間の幾何学的な面法線とビュー方向の内積で行う - こちらは
	///      座標系の巻き方向に依存せず一意に決まる。面法線は頂点順序依存で
	///      符号があいまいなので、まず補間済みシェーディング法線Nと同じ向きに
	///      揃えてから、カメラ方向との内積で表裏を判定する。
	float3 geometric_face_normal = cross(world_position1 - world_position0, world_position2 - world_position0);
	if (dot(geometric_face_normal, N) < 0.0)
	{
		geometric_face_normal = -geometric_face_normal;
	}
	float3 view_direction_for_facing = normalize(scene.camera_position_.xyz - world_position);
	bool is_front_face = dot(geometric_face_normal, view_direction_for_facing) > 0.0;

	if (!is_front_face)
	{
		N = -N;
	}

	/// [JP] StaticModelPS.hlsl と同じマテリアル評価。
	float4 base_color = instance.base_color_;
	if (instance.base_color_texture_index_ != 0xFFFFFFFF)
	{
		Texture2D base_color_texture = ResourceDescriptorHeap[instance.base_color_texture_index_];
		base_color *= base_color_texture.SampleLevel(sampler_aniso_wrap, texcoord, 0);
	}

	if (instance.normal_texture_index_ != 0xFFFFFFFF)
	{
		float3 T = world_tangent_xyz;
		float3 B = cross(N, T) * tangent_sign;
		float3x3 TBN = float3x3(T, B, N);

		Texture2D normal_texture = ResourceDescriptorHeap[instance.normal_texture_index_];
		float3 normal_sample = normal_texture.SampleLevel(sampler_linear_wrap, texcoord, 0).xyz * 2.0 - 1.0;
		N = normalize(mul(normal_sample, TBN));
	}

	float metallic = instance.metallic_;
	float roughness = instance.roughness_;
	if (instance.metallic_roughness_texture_index_ != 0xFFFFFFFF)
	{
		Texture2D metallic_roughness_texture = ResourceDescriptorHeap[instance.metallic_roughness_texture_index_];
		float4 metallic_roughness = metallic_roughness_texture.SampleLevel(sampler_linear_wrap, texcoord, 0);
		metallic *= metallic_roughness.b;
		roughness *= metallic_roughness.g;
	}

	float3 emissive = instance.emissive_;
	if (instance.emissive_texture_index_ != 0xFFFFFFFF)
	{
		Texture2D emissive_texture = ResourceDescriptorHeap[instance.emissive_texture_index_];
		emissive *= emissive_texture.SampleLevel(sampler_linear_wrap, texcoord, 0).rgb;
	}

	float3 previous_world_position0 = mul(float4(local_position0, 1.0), instance.previous_world_).xyz;
	float3 previous_world_position1 = mul(float4(local_position1, 1.0), instance.previous_world_).xyz;
	float3 previous_world_position2 = mul(float4(local_position2, 1.0), instance.previous_world_).xyz;
	float3 previous_world_position = perspective_w0 * previous_world_position0 + perspective_w1 * previous_world_position1 + perspective_w2 * previous_world_position2;
	float4 previous_clip = mul(float4(previous_world_position, 1.0), scene.previous_view_projection_);

	float4 current_clip = mul(float4(world_position, 1.0), scene.current_view_projection_);
	float2 current_ndc = current_clip.xy / current_clip.w;
	float2 previous_ndc = previous_clip.xy / previous_clip.w;
	float2 velocity = (current_ndc - previous_ndc) * 0.5;

	RWTexture2D<float4> rt0 = ResourceDescriptorHeap[structured_indices.gbuffer_.index_0_uav_];
	RWTexture2D<float4> rt1 = ResourceDescriptorHeap[structured_indices.gbuffer_.index_1_uav_];
	RWTexture2D<float2> rt2 = ResourceDescriptorHeap[structured_indices.gbuffer_.velocity_uav_index_];
	RWTexture2D<float4> rt3 = ResourceDescriptorHeap[structured_indices.gbuffer_.index_3_uav_];

	rt0[pixel] = float4(base_color.rgb, metallic);
	/// [JP] RT1.a は未使用(0固定) - KHR拡張のスカラー値(ior/specular/clearcoat/
	///      transmission/volume/sheen/iridescence/anisotropy/unlit)は per-instance
	///      定数でピクセルごとに変わらないため GBuffer に焼き込まず、VisID経由で
	///      DeferredLightingPS.hlsl がその場で ModelInstance から直接読む。
	rt1[pixel] = float4(OctNormalEncode(N), roughness, 0.0);
	rt2[pixel] = velocity;
	/// [JP] emissive は生値(テクスチャ*factor)のみ - emissive_strength_ は
	///      per-instance 定数なので DeferredLightingPS.hlsl 側で掛ける。
	rt3[pixel] = float4(emissive, 0.0);
}
