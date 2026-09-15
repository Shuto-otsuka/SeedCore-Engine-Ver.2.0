#include "../Model.hlsli"
#include "../../Shader/ShaderResources.hlsli"
#include "../../Shader/UnorderedAccesses.hlsli"
#include "../../Shader/Sampler.hlsli"
#include "../../Shader/Normal.hlsli"
#include "../../Shader/Scene.hlsli"

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
* straight from ModelStructuredBuffer via this same VisID at lighting time instead.
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

	RWByteAddressBuffer sorted_pixel_list = ResourceDescriptorHeap[unordered_access_indices.material_sort_.sorted_pixel_list_index_];
	uint linear_pixel = sorted_pixel_list.Load(dtid.x * 4);
	if (linear_pixel == MATERIAL_SORT_INVALID_PIXEL)
	{
		return;
	}

	uint2 pixel = uint2(linear_pixel % screen_width, linear_pixel / screen_width);

	Texture2D<uint4> visibility_texture = ResourceDescriptorHeap[shader_resource_indices.geometry_buffer_.index_4_];
	uint4 visibility_id = visibility_texture.Load(int3(pixel, 0));

	uint instance_index;
	uint meshlet_index;
	uint triangle_in_meshlet_index;
	UnpackVisibilityID(visibility_id, instance_index, meshlet_index, triangle_in_meshlet_index);

	StructuredBuffer<ModelStructuredBuffer> instances = GetModelStructuredBuffer(shader_resource_indices.model_.instance_index_);
	ModelStructuredBuffer instance = instances[instance_index];

	/// [JP] 三角形の再取得・スキニング・透視正しい重心補間・表裏判定は
	///      Model/Transparent/ModelTransparentPS.hlsl の OIT パスと同一の
	///      ロジックをここにも展開している — 透明面と不透明面が必ず
	///      同一のジオメトリから陰影付けされるようにするため。
	float2 pixel_ndc = (float2(pixel) + 0.5) * scene.inverse_screen_size_;
	pixel_ndc = float2(pixel_ndc.x * 2.0 - 1.0, 1.0 - pixel_ndc.y * 2.0);

	StructuredBuffer<CompressedModelVertex> vertices = ResourceDescriptorHeap[instance.geometry_.vertex_buffer_index_];
	StructuredBuffer<ModelMeshlet> meshlets = ResourceDescriptorHeap[instance.geometry_.meshlet_buffer_index_];
	StructuredBuffer<uint> resolve_vertex_indices = ResourceDescriptorHeap[instance.geometry_.vertex_indices_buffer_index_];
	ByteAddressBuffer resolve_primitive_indices = ResourceDescriptorHeap[instance.geometry_.primitive_indices_buffer_index_];

	ModelMeshlet resolve_meshlet = meshlets[meshlet_index];

	// Same 3-bytes-per-triangle unpack as StaticModelMS.hlsl / SkeletalModelMS.hlsl
	// (primitive_indices holds meshlet-local vertex numbers 0..63).
	uint resolve_byte_offset = resolve_meshlet.triangle_offset_ + triangle_in_meshlet_index * 3;
	uint resolve_aligned_offset = resolve_byte_offset & ~3;
	uint resolve_shift = (resolve_byte_offset & 3) * 8;

	uint resolve_dword0 = resolve_primitive_indices.Load(resolve_aligned_offset);
	uint resolve_packed = resolve_dword0 >> resolve_shift;
	if (resolve_shift > 8)
	{
		uint resolve_dword1 = resolve_primitive_indices.Load(resolve_aligned_offset + 4);
		resolve_packed |= resolve_dword1 << (32 - resolve_shift);
	}

	uint local_index0 = resolve_packed & 0xFF;
	uint local_index1 = (resolve_packed >> 8) & 0xFF;
	uint local_index2 = (resolve_packed >> 16) & 0xFF;

	uint global_index0 = resolve_vertex_indices[resolve_meshlet.vertex_offset_ + local_index0];
	uint global_index1 = resolve_vertex_indices[resolve_meshlet.vertex_offset_ + local_index1];
	uint global_index2 = resolve_vertex_indices[resolve_meshlet.vertex_offset_ + local_index2];

	ExpandedModelVertex vertex0 = DecodeModelVertex(vertices[global_index0], instance);
	ExpandedModelVertex vertex1 = DecodeModelVertex(vertices[global_index1], instance);
	ExpandedModelVertex vertex2 = DecodeModelVertex(vertices[global_index2], instance);

	float3 local_position0 = vertex0.position_;
	float3 local_position1 = vertex1.position_;
	float3 local_position2 = vertex2.position_;
	float3 previous_local_position0 = local_position0;
	float3 previous_local_position1 = local_position1;
	float3 previous_local_position2 = local_position2;

	// Morph composes before skin (matches StaticModelMS.hlsl/
	// SkeletalModelMS.hlsl) - without this, the depth/VisID pass below
	// writes morphed positions while this reconstruction used the
	// unmorphed ones, and the mismatch corrupts the screen-space
	// barycentric reprojection entirely (wrong/garbage triangle shape).
	// global_vertex_index must be the SAME index used to fetch the vertex's
	// CompressedModelVertex (i.e. an index into the LOD 0 shared vertex pool
	// - see morph_target_count_'s comment for why this is a no-op for any
	// other cluster).
	if (instance.morph_.morph_target_count_ != 0)
	{
		StructuredBuffer<uint> vertex_morph_source = ResourceDescriptorHeap[instance.morph_.vertex_morph_source_buffer_index_];
		StructuredBuffer<float3> morph_deltas = ResourceDescriptorHeap[instance.morph_.morph_delta_buffer_index_];
		StructuredBuffer<float> morph_weights = ResourceDescriptorHeap[shader_resource_indices.model_.morph_weight_index_];
		StructuredBuffer<float> previous_morph_weights = ResourceDescriptorHeap[shader_resource_indices.model_.previous_morph_weight_index_];

		uint local_vertex_index0 = vertex_morph_source[global_index0] - instance.morph_.morph_vertex_offset_;
		uint local_vertex_index1 = vertex_morph_source[global_index1] - instance.morph_.morph_vertex_offset_;
		uint local_vertex_index2 = vertex_morph_source[global_index2] - instance.morph_.morph_vertex_offset_;

		for (uint target = 0; target < instance.morph_.morph_target_count_; ++target)
		{
			float weight = morph_weights[instance.morph_.morph_weight_offset_ + target];
			float previous_weight = previous_morph_weights[instance.morph_.morph_weight_offset_ + target];
			float3 morph_delta0 = morph_deltas[instance.morph_.morph_delta_offset_ + target * instance.morph_.morph_vertex_count_ + local_vertex_index0];
			float3 morph_delta1 = morph_deltas[instance.morph_.morph_delta_offset_ + target * instance.morph_.morph_vertex_count_ + local_vertex_index1];
			float3 morph_delta2 = morph_deltas[instance.morph_.morph_delta_offset_ + target * instance.morph_.morph_vertex_count_ + local_vertex_index2];
			local_position0 += morph_delta0 * weight;
			local_position1 += morph_delta1 * weight;
			local_position2 += morph_delta2 * weight;
			previous_local_position0 += morph_delta0 * previous_weight;
			previous_local_position1 += morph_delta1 * previous_weight;
			previous_local_position2 += morph_delta2 * previous_weight;
		}
	}

	float3 local_normal0 = vertex0.normal_;
	float3 local_normal1 = vertex1.normal_;
	float3 local_normal2 = vertex2.normal_;
	float3 local_tangent0 = vertex0.tangent_.xyz;
	float3 local_tangent1 = vertex1.tangent_.xyz;
	float3 local_tangent2 = vertex2.tangent_.xyz;

	// Same linear blend skinning as SkeletalModelMS.hlsl.
	// instance.skining_.skin_index_ == 0xFFFFFFFF means static (unskinned).
	if (instance.skining_.skin_index_ != 0xFFFFFFFF)
	{
		StructuredBuffer<ModelBoneMatrix> bone_matrices = ResourceDescriptorHeap[shader_resource_indices.model_.bone_matrix_index_];
		StructuredBuffer<CompressedModelSkin> skin_vertices = ResourceDescriptorHeap[instance.skining_.skin_vertex_buffer_index_];

		ExpandedModelSkin skin0 = DecodeSkinVertex(skin_vertices[global_index0]);
		ExpandedModelSkin skin1 = DecodeSkinVertex(skin_vertices[global_index1]);
		ExpandedModelSkin skin2 = DecodeSkinVertex(skin_vertices[global_index2]);

		float4x4 skin_matrix0 =
			LoadBoneMatrix(bone_matrices[instance.skining_.bone_offset_ + skin0.joints_.x]) * skin0.weights_.x +
			LoadBoneMatrix(bone_matrices[instance.skining_.bone_offset_ + skin0.joints_.y]) * skin0.weights_.y +
			LoadBoneMatrix(bone_matrices[instance.skining_.bone_offset_ + skin0.joints_.z]) * skin0.weights_.z +
			LoadBoneMatrix(bone_matrices[instance.skining_.bone_offset_ + skin0.joints_.w]) * skin0.weights_.w;
		float4x4 skin_matrix1 =
			LoadBoneMatrix(bone_matrices[instance.skining_.bone_offset_ + skin1.joints_.x]) * skin1.weights_.x +
			LoadBoneMatrix(bone_matrices[instance.skining_.bone_offset_ + skin1.joints_.y]) * skin1.weights_.y +
			LoadBoneMatrix(bone_matrices[instance.skining_.bone_offset_ + skin1.joints_.z]) * skin1.weights_.z +
			LoadBoneMatrix(bone_matrices[instance.skining_.bone_offset_ + skin1.joints_.w]) * skin1.weights_.w;
		float4x4 skin_matrix2 =
			LoadBoneMatrix(bone_matrices[instance.skining_.bone_offset_ + skin2.joints_.x]) * skin2.weights_.x +
			LoadBoneMatrix(bone_matrices[instance.skining_.bone_offset_ + skin2.joints_.y]) * skin2.weights_.y +
			LoadBoneMatrix(bone_matrices[instance.skining_.bone_offset_ + skin2.joints_.z]) * skin2.weights_.z +
			LoadBoneMatrix(bone_matrices[instance.skining_.bone_offset_ + skin2.joints_.w]) * skin2.weights_.w;

		StructuredBuffer<ModelBoneMatrix> previous_bone_matrices = ResourceDescriptorHeap[shader_resource_indices.model_.previous_bone_matrix_index_];
		float4x4 previous_skin_matrix0 =
			LoadBoneMatrix(previous_bone_matrices[instance.skining_.bone_offset_ + skin0.joints_.x]) * skin0.weights_.x +
			LoadBoneMatrix(previous_bone_matrices[instance.skining_.bone_offset_ + skin0.joints_.y]) * skin0.weights_.y +
			LoadBoneMatrix(previous_bone_matrices[instance.skining_.bone_offset_ + skin0.joints_.z]) * skin0.weights_.z +
			LoadBoneMatrix(previous_bone_matrices[instance.skining_.bone_offset_ + skin0.joints_.w]) * skin0.weights_.w;
		float4x4 previous_skin_matrix1 =
			LoadBoneMatrix(previous_bone_matrices[instance.skining_.bone_offset_ + skin1.joints_.x]) * skin1.weights_.x +
			LoadBoneMatrix(previous_bone_matrices[instance.skining_.bone_offset_ + skin1.joints_.y]) * skin1.weights_.y +
			LoadBoneMatrix(previous_bone_matrices[instance.skining_.bone_offset_ + skin1.joints_.z]) * skin1.weights_.z +
			LoadBoneMatrix(previous_bone_matrices[instance.skining_.bone_offset_ + skin1.joints_.w]) * skin1.weights_.w;
		float4x4 previous_skin_matrix2 =
			LoadBoneMatrix(previous_bone_matrices[instance.skining_.bone_offset_ + skin2.joints_.x]) * skin2.weights_.x +
			LoadBoneMatrix(previous_bone_matrices[instance.skining_.bone_offset_ + skin2.joints_.y]) * skin2.weights_.y +
			LoadBoneMatrix(previous_bone_matrices[instance.skining_.bone_offset_ + skin2.joints_.z]) * skin2.weights_.z +
			LoadBoneMatrix(previous_bone_matrices[instance.skining_.bone_offset_ + skin2.joints_.w]) * skin2.weights_.w;

		local_position0 = mul(float4(local_position0, 1.0), skin_matrix0).xyz;
		local_position1 = mul(float4(local_position1, 1.0), skin_matrix1).xyz;
		local_position2 = mul(float4(local_position2, 1.0), skin_matrix2).xyz;
		previous_local_position0 = mul(float4(previous_local_position0, 1.0), previous_skin_matrix0).xyz;
		previous_local_position1 = mul(float4(previous_local_position1, 1.0), previous_skin_matrix1).xyz;
		previous_local_position2 = mul(float4(previous_local_position2, 1.0), previous_skin_matrix2).xyz;
		local_normal0 = normalize(mul(float4(local_normal0, 0.0), skin_matrix0).xyz);
		local_normal1 = normalize(mul(float4(local_normal1, 0.0), skin_matrix1).xyz);
		local_normal2 = normalize(mul(float4(local_normal2, 0.0), skin_matrix2).xyz);
		local_tangent0 = normalize(mul(float4(local_tangent0, 0.0), skin_matrix0).xyz);
		local_tangent1 = normalize(mul(float4(local_tangent1, 0.0), skin_matrix1).xyz);
		local_tangent2 = normalize(mul(float4(local_tangent2, 0.0), skin_matrix2).xyz);
	}

	float3 world_position0 = mul(float4(local_position0, 1.0), instance.transform_.world_).xyz;
	float3 world_position1 = mul(float4(local_position1, 1.0), instance.transform_.world_).xyz;
	float3 world_position2 = mul(float4(local_position2, 1.0), instance.transform_.world_).xyz;

	float4 resolve_clip0 = mul(float4(world_position0, 1.0), scene.current_view_projection_);
	float4 resolve_clip1 = mul(float4(world_position1, 1.0), scene.current_view_projection_);
	float4 resolve_clip2 = mul(float4(world_position2, 1.0), scene.current_view_projection_);

	float2 resolve_ndc0 = resolve_clip0.xy / resolve_clip0.w;
	float2 resolve_ndc1 = resolve_clip1.xy / resolve_clip1.w;
	float2 resolve_ndc2 = resolve_clip2.xy / resolve_clip2.w;

	// Screen-space barycentric weights via edge functions.
	float resolve_area = (resolve_ndc1.x - resolve_ndc0.x) * (resolve_ndc2.y - resolve_ndc0.y) - (resolve_ndc1.y - resolve_ndc0.y) * (resolve_ndc2.x - resolve_ndc0.x);

	float resolve_edge0 = (resolve_ndc2.x - resolve_ndc1.x) * (pixel_ndc.y - resolve_ndc1.y) - (resolve_ndc2.y - resolve_ndc1.y) * (pixel_ndc.x - resolve_ndc1.x);
	float resolve_edge1 = (resolve_ndc0.x - resolve_ndc2.x) * (pixel_ndc.y - resolve_ndc2.y) - (resolve_ndc0.y - resolve_ndc2.y) * (pixel_ndc.x - resolve_ndc2.x);
	float resolve_edge2 = (resolve_ndc1.x - resolve_ndc0.x) * (pixel_ndc.y - resolve_ndc0.y) - (resolve_ndc1.y - resolve_ndc0.y) * (pixel_ndc.x - resolve_ndc0.x);
	float screen_w0 = resolve_edge0 / resolve_area;
	float screen_w1 = resolve_edge1 / resolve_area;
	float screen_w2 = resolve_edge2 / resolve_area;

	// Perspective correction: weighting the (NDC-linear) screen weights by invW
	// and renormalising makes a plain linear combination of attributes
	// perspective-correct.
	float inverse_w0 = 1.0 / resolve_clip0.w;
	float inverse_w1 = 1.0 / resolve_clip1.w;
	float inverse_w2 = 1.0 / resolve_clip2.w;
	float perspective_w0 = screen_w0 * inverse_w0;
	float perspective_w1 = screen_w1 * inverse_w1;
	float perspective_w2 = screen_w2 * inverse_w2;
	float perspective_sum = perspective_w0 + perspective_w1 + perspective_w2;
	perspective_w0 /= perspective_sum;
	perspective_w1 /= perspective_sum;
	perspective_w2 /= perspective_sum;

	ModelSurface surface;
	surface.world_position_ = perspective_w0 * world_position0 + perspective_w1 * world_position1 + perspective_w2 * world_position2;

	float3 resolve_previous_world_position0 = mul(float4(previous_local_position0, 1.0), instance.transform_.previous_world_).xyz;
	float3 resolve_previous_world_position1 = mul(float4(previous_local_position1, 1.0), instance.transform_.previous_world_).xyz;
	float3 resolve_previous_world_position2 = mul(float4(previous_local_position2, 1.0), instance.transform_.previous_world_).xyz;
	surface.previous_world_position_ = perspective_w0 * resolve_previous_world_position0 + perspective_w1 * resolve_previous_world_position1 + perspective_w2 * resolve_previous_world_position2;

	float3 resolve_N = normalize(perspective_w0 * local_normal0 + perspective_w1 * local_normal1 + perspective_w2 * local_normal2);
	float3 resolve_world_tangent_xyz = normalize(perspective_w0 * local_tangent0 + perspective_w1 * local_tangent1 + perspective_w2 * local_tangent2);

	surface.tangent_sign_ = vertex0.tangent_.w;
	surface.texcoord_ = perspective_w0 * vertex0.texcoord_ + perspective_w1 * vertex1.texcoord_ + perspective_w2 * vertex2.texcoord_;

	// Normals go through the inverse transpose, tangents through the plain
	// world matrix - same as StaticModelMS.hlsl used to do.
	resolve_N = normalize(mul(float4(resolve_N, 0.0), instance.transform_.inverse_transpose_world_).xyz);
	surface.tangent_ = normalize(mul(float4(resolve_world_tangent_xyz, 0.0), instance.transform_.world_).xyz);

	// Facing from the world-space geometric face normal versus the view
	// direction, NOT the NDC winding sign (which depends on the source model's
	// handedness). The face normal's sign is vertex-order dependent, so align it
	// with the interpolated shading normal first.
	float3 resolve_geometric_face_normal = cross(world_position1 - world_position0, world_position2 - world_position0);
	if (dot(resolve_geometric_face_normal, resolve_N) < 0.0)
	{
		resolve_geometric_face_normal = -resolve_geometric_face_normal;
	}
	if (instance.shading_.shading_model_ == SHADING_MODEL_FLAT)
	{
		resolve_N = normalize(resolve_geometric_face_normal);
	}
	float3 resolve_view_direction_for_facing = normalize(scene.camera_position_.xyz - surface.world_position_);
	bool resolve_is_front_face = dot(resolve_geometric_face_normal, resolve_view_direction_for_facing) > 0.0;

	if (!resolve_is_front_face)
	{
		resolve_N = -resolve_N;
	}
	surface.normal_ = resolve_N;

	float3 world_position = surface.world_position_;
	float3 N = surface.normal_;
	float3 world_tangent_xyz = surface.tangent_;
	float tangent_sign = surface.tangent_sign_;
	float2 texcoord = surface.texcoord_;

	/// [JP] StaticModelPS.hlsl と同じマテリアル評価。
	float4 base_color = instance.texture_.base_color_;
	if (instance.texture_.base_color_texture_index_ != 0xFFFFFFFF)
	{
		Texture2D base_color_texture = ResourceDescriptorHeap[instance.texture_.base_color_texture_index_];
		base_color *= base_color_texture.SampleLevel(sampler_aniso_wrap, texcoord, 0);
	}

	if (instance.texture_.normal_texture_index_ != 0xFFFFFFFF)
	{
		float3 T = world_tangent_xyz;
		float3 B = cross(N, T) * tangent_sign;
		float3x3 TBN = float3x3(T, B, N);

		Texture2D normal_texture = ResourceDescriptorHeap[instance.texture_.normal_texture_index_];
		float3 normal_sample = normal_texture.SampleLevel(sampler_linear_wrap, texcoord, 0).xyz * 2.0 - 1.0;
		N = normalize(mul(normal_sample, TBN));
	}

	float metallic = instance.texture_.metallic_;
	float roughness = instance.texture_.roughness_;
	if (instance.texture_.metallic_roughness_texture_index_ != 0xFFFFFFFF)
	{
		Texture2D metallic_roughness_texture = ResourceDescriptorHeap[instance.texture_.metallic_roughness_texture_index_];
		float4 metallic_roughness = metallic_roughness_texture.SampleLevel(sampler_linear_wrap, texcoord, 0);
		metallic *= metallic_roughness.b;
		roughness *= metallic_roughness.g;
	}

	float3 emissive = instance.texture_.emissive_;
	if (instance.texture_.emissive_texture_index_ != 0xFFFFFFFF)
	{
		Texture2D emissive_texture = ResourceDescriptorHeap[instance.texture_.emissive_texture_index_];
		emissive *= emissive_texture.SampleLevel(sampler_linear_wrap, texcoord, 0).rgb;
	}

	float4 previous_clip = mul(float4(surface.previous_world_position_, 1.0), scene.previous_view_projection_);

	float4 current_clip = mul(float4(world_position, 1.0), scene.current_view_projection_);
	float2 current_ndc = current_clip.xy / current_clip.w;
	float2 previous_ndc = previous_clip.xy / previous_clip.w;
	float2 velocity = (current_ndc - previous_ndc) * 0.5;

	RWTexture2D<float4> rt0 = ResourceDescriptorHeap[unordered_access_indices.geometry_buffer_.index_0_];
	RWTexture2D<float4> rt1 = ResourceDescriptorHeap[unordered_access_indices.geometry_buffer_.index_1_];
	RWTexture2D<float2> rt2 = ResourceDescriptorHeap[unordered_access_indices.geometry_buffer_.index_2_];
	RWTexture2D<float4> rt3 = ResourceDescriptorHeap[unordered_access_indices.geometry_buffer_.index_3_];

	rt0[pixel] = float4(base_color.rgb, metallic);
	/// [JP] RT1.a はタンジェント(N まわりの角度15bit + 利き手符号1bit)。
	///      KHR拡張のスカラー値は per-instance 定数なので GBuffer に焼き込まず
	///      VisID 経由で DeferredLightingPS.hlsl が ModelStructuredBuffer から直接読むが、
	///      KHR_materials_clearcoat の法線マップだけは接空間なので TBN が要る。
	///      deferred パスは補間済みタンジェントを持たないため、ここで渡す。
	///      符号化は【法線マップ適用後の N】に対して行う - 復元側はその N と
	///      直交するタンジェントを得て、そのまま TBN を組める。
	rt1[pixel] = float4(OctNormalEncode(N), roughness, PackTangentAngle(N, world_tangent_xyz, tangent_sign));
	rt2[pixel] = velocity;
	/// [JP] emissive は生値(テクスチャ*factor)のみ - emissive_strength_ は
	///      per-instance 定数なので DeferredLightingPS.hlsl 側で掛ける。
	rt3[pixel] = float4(emissive, 0.0);
}
