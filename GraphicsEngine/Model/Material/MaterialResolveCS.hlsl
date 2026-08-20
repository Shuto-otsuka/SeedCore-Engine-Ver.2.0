#include "../Model.hlsli"
#include "../../Shader/Structured.hlsli"
#include "../../Shader/Sampler.hlsli"
#include "../../Shader/Normal.hlsli"
#include "../../Shader/Constants.hlsli"

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

	Texture2D<uint4> visibility_texture = ResourceDescriptorHeap[structured_indices.gbuffer_.index_4_];
	uint4 visibility_id = visibility_texture.Load(int3(pixel, 0));

	uint instance_index;
	uint meshlet_index;
	uint triangle_in_meshlet_index;
	UnpackVisibilityID(visibility_id, instance_index, meshlet_index, triangle_in_meshlet_index);

	StructuredBuffer<ModelInstance> instances = ResourceDescriptorHeap[structured_indices.model_.instance_index_];
	ModelInstance instance = instances[instance_index];

	/// [JP] 三角形の再取得・スキニング・透視正しい重心補間・表裏判定は
	///      Model.hlsli の ResolveModelSurface に集約してある
	///      (Model/Transparent/ModelTransparentPS.hlsl の OIT パスと共有 — 透明面と
	///      不透明面が必ず同一のジオメトリから陰影付けされるようにするため)。
	float2 pixel_ndc = (float2(pixel) + 0.5) * scene.inverse_screen_size_;
	pixel_ndc = float2(pixel_ndc.x * 2.0 - 1.0, 1.0 - pixel_ndc.y * 2.0);

	ModelSurface surface = ResolveModelSurface(instance, meshlet_index, triangle_in_meshlet_index, pixel_ndc, scene.current_view_projection_, scene.camera_position_.xyz, structured_indices.model_.bone_matrix_index_, structured_indices.model_.morph_weight_index_);

	float3 world_position = surface.world_position_;
	float3 N = surface.normal_;
	float3 world_tangent_xyz = surface.tangent_;
	float tangent_sign = surface.tangent_sign_;
	float2 texcoord = surface.texcoord_;

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

	float4 previous_clip = mul(float4(surface.previous_world_position_, 1.0), scene.previous_view_projection_);

	float4 current_clip = mul(float4(world_position, 1.0), scene.current_view_projection_);
	float2 current_ndc = current_clip.xy / current_clip.w;
	float2 previous_ndc = previous_clip.xy / previous_clip.w;
	float2 velocity = (current_ndc - previous_ndc) * 0.5;

	RWTexture2D<float4> rt0 = ResourceDescriptorHeap[structured_indices.gbuffer_.index_0_uav_];
	RWTexture2D<float4> rt1 = ResourceDescriptorHeap[structured_indices.gbuffer_.index_1_uav_];
	RWTexture2D<float2> rt2 = ResourceDescriptorHeap[structured_indices.gbuffer_.velocity_uav_index_];
	RWTexture2D<float4> rt3 = ResourceDescriptorHeap[structured_indices.gbuffer_.index_3_uav_];

	rt0[pixel] = float4(base_color.rgb, metallic);
	/// [JP] RT1.a はタンジェント(N まわりの角度15bit + 利き手符号1bit)。
	///      KHR拡張のスカラー値は per-instance 定数なので GBuffer に焼き込まず
	///      VisID 経由で DeferredLightingPS.hlsl が ModelInstance から直接読むが、
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
