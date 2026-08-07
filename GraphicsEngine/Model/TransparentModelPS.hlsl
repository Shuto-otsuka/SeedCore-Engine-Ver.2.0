#include "Model.hlsli"
#include "../Shader/Structured.hlsli"
#include "../Shader/Sampler.hlsli"
#include "../Shader/Constants.hlsli"
#include "../Shader/Normal.hlsli"
#include "../Shader/Light.hlsli"
#include "../Light/Cluster.hlsli"
#include "../Light/BidirectionalReflectanceDistributionFunction.hlsli"
#include "../Raytracing/Reflection/Reflection.hlsli"

/**
* [EN]
* Pixel Shader for transparent model PPLL accumulation.
* Instead of writing to G-Buffer render targets, this PS allocates
* a fragment in the Per-Pixel Linked List and writes color + depth.
* Used by both static and skeletal transparent models.
*
* The surface is fully lit here rather than written out flat: transparent
* geometry never reaches the deferred G-Buffer (it has no VisibilityBuffer
* entry, since the OIT pass writes no render targets), so
* Model/DeferredLightingPS.hlsl never gets a chance to shade it. Shading
* therefore happens inline, reconstructing the surface through the same
* Model.hlsli ResolveModelSurface the deferred resolve uses so transparent and
* opaque geometry are shaded from identical attributes. The lighting model is
* the same base PBR as the deferred path (Lambert + GGX, clustered punctual
* lights, diffuse/specular IBL); the extras that only exist per-instance in the
* deferred path (clearcoat/sheen/iridescence, ray-traced shadows/AO/GI) are
* deliberately left out - they need G-Buffer-resident screen-space inputs that
* transparent fragments do not have.
*
* The head pointer texture, fragment buffer, and counter are accessed
* as UAVs via the bindless descriptor heap.
*
* ---------------------------------------------------------------------
*
* [JP]
* 透明モデル PPLL 蓄積用のピクセルシェーダー。
* G-Buffer RT に書き込む代わりに、Per-Pixel Linked List にフラグメントを
* アトミックに割り当て、色 + 深度を書き込む。
* 静的・スケルタル両方の透明モデルで使用する。
*
* ここで面を完全にライティングしてから書き込む(以前のようにフラットな
* ベースカラーをそのまま書かない): 透明ジオメトリは deferred の G-Buffer に
* 一切載らない(OITパスはRTを書かないので VisibilityBuffer のエントリが無い)
* ため、Model/DeferredLightingPS.hlsl が陰影を付ける機会が無い。そこで
* この場でシェーディングし、面の再構築には deferred の解決パスと同じ
* Model.hlsli の ResolveModelSurface を使う — 透明面と不透明面が必ず同一の
* 属性から陰影付けされるようにするため。ライティングモデルは deferred と
* 同じ基本PBR(Lambert + GGX、クラスタライト、拡散/鏡面IBL)。deferred 側
* だけにある追加要素(クリアコート/シーン/虹彩、レイトレの影/AO/GI)は
* あえて省いている — それらは透明フラグメントが持ち得ない
* G-Buffer 常駐のスクリーン空間入力を必要とするため。
*
* ヘッドポインタテクスチャ、フラグメントバッファ、カウンターは
* バインドレスディスクリプタヒープ経由で UAV としてアクセスする。
*/
void main(ModelMSOutput input, ModelMSPrimitiveOutput primitive)
{
	StructuredBuffer<ModelInstance> instances = ResourceDescriptorHeap[structured_indices.model_.instance_index_];
	ModelInstance instance = instances[input.instance_index];

	/// [JP] まずアルファだけ先に評価してクリップする — 完全に透明なフラグメント
	///      に対して面の再構築とライティングを走らせても無駄なので、
	///      ベースカラーのサンプルは補間済み texcoord で先に済ませる。
	float4 base_color = instance.base_color_;
	if (instance.base_color_texture_index_ != OIT_INVALID_INDEX)
	{
		Texture2D base_color_texture = ResourceDescriptorHeap[instance.base_color_texture_index_];
		base_color *= base_color_texture.Sample(sampler_aniso_wrap, input.texcoord);
	}

	clip(base_color.a - 0.01);

	SceneConstantBuffer scene = GetSceneConstantBuffer();

	/// [JP] SV_Position.xy は既にピクセル中心(x+0.5)なのでそのまま NDC へ。
	float2 uv = input.position.xy * scene.inverse_screen_size_;
	float2 pixel_ndc = float2(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0);

	ModelSurface surface = ResolveModelSurface(instance, input.meshlet_index, primitive.triangle_in_meshlet_index, pixel_ndc, scene.current_view_projection_, scene.camera_position_.xyz, structured_indices.model_.bone_matrix_index_);

	float3 N = surface.normal_;

	if (instance.normal_texture_index_ != OIT_INVALID_INDEX)
	{
		float3 T = surface.tangent_;
		float3 B = cross(N, T) * surface.tangent_sign_;
		float3x3 TBN = float3x3(T, B, N);

		Texture2D normal_texture = ResourceDescriptorHeap[instance.normal_texture_index_];
		float3 normal_sample = normal_texture.Sample(sampler_linear_wrap, surface.texcoord_).xyz * 2.0 - 1.0;
		N = normalize(mul(normal_sample, TBN));
	}

	float metallic = instance.metallic_;
	float roughness = instance.roughness_;
	if (instance.metallic_roughness_texture_index_ != OIT_INVALID_INDEX)
	{
		Texture2D metallic_roughness_texture = ResourceDescriptorHeap[instance.metallic_roughness_texture_index_];
		float4 metallic_roughness = metallic_roughness_texture.Sample(sampler_linear_wrap, surface.texcoord_);
		metallic *= metallic_roughness.b;
		roughness *= metallic_roughness.g;
	}
	roughness = clamp(roughness, 0.03, 1.0);

	float3 emissive = instance.emissive_;
	if (instance.emissive_texture_index_ != OIT_INVALID_INDEX)
	{
		Texture2D emissive_texture = ResourceDescriptorHeap[instance.emissive_texture_index_];
		emissive *= emissive_texture.Sample(sampler_linear_wrap, surface.texcoord_).rgb;
	}
	emissive *= instance.emissive_strength_;

	/// [JP] KHR_materials_unlit: ライティングを丸ごと飛ばして
	///      base_color + emissive をそのまま使う(DeferredLightingPS.hlsl と同じ)。
	float3 lighting;
	if (instance.unlit_ > 0.5)
	{
		lighting = base_color.rgb + emissive;
	}
	else
	{
		float3 view = normalize(scene.camera_position_.xyz - surface.world_position_);

		/// [JP] DeferredLightingPS.hlsl と同じ誘電体F0の組み立て
		///      (KHR_materials_ior + KHR_materials_specular)。
		float dielectric = (instance.ior_ - 1.0) / (instance.ior_ + 1.0);
		dielectric = dielectric * dielectric;
		float3 dielectric_f0 = saturate(dielectric * instance.specular_color_ * instance.specular_factor_);

		float3 diffuse_color = base_color.rgb * (1.0 - metallic);
		float3 f0 = lerp(dielectric_f0, base_color.rgb, metallic);

		lighting = float3(0, 0, 0);

		ConstantBuffer<LightConstantData> light_constant_buffer = ResourceDescriptorHeap[constant_indices.light_index_];

		/// [JP] ディレクショナルライト。影レイは撃たない — 透明フラグメントは
		///      スクリーン空間の影バッファ(不透明面の深度で解決済み)を引けないため。
		if (light_constant_buffer.directional_intensity_ > 0.0)
		{
			float3 light_direction = normalize(-light_constant_buffer.directional_direction_);
			float normal_dot_light = max(dot(N, light_direction), 0.0);
			if (normal_dot_light > 0.0)
			{
				float normal_dot_view = max(dot(N, view), 0.001);
				float3 half_vector = normalize(view + light_direction);
				float normal_dot_half = max(dot(N, half_vector), 0.0);
				float view_dot_half = max(dot(view, half_vector), 0.0);

				float alpha_roughness = roughness * roughness;
				float3 f90 = float3(1.0, 1.0, 1.0);

				float3 diffuse = BrdfLambertian(f0, f90, diffuse_color, view_dot_half);
				float3 specular = BrdfSpecularGgx(f0, f90, alpha_roughness, view_dot_half, normal_dot_light, normal_dot_view, normal_dot_half);

				lighting += (diffuse + specular) * normal_dot_light * light_constant_buffer.directional_color_.rgb * light_constant_buffer.directional_intensity_;
			}
		}

		/// [JP] Point/Spot/Rect: ReflectionRT.hlsl / GlobalIlluminationRT.hlsl と
		///      共有のクラスタ済みランバート項(Reflection.hlsli)。1/PI 込み。
		lighting += ComputeClusteredPunctualLighting(surface.world_position_, N, scene, light_constant_buffer) * diffuse_color;

		/// [JP] IBL。スカイマップが無い場合はフラットな環境光へフォールバック
		///      (DeferredLightingPS.hlsl の AMBIENT と同じ 0.03)。
		if (structured_indices.sky_.diffuse_irradiance_index_ != 0)
		{
			lighting += ImageBasedLightingRadianceLambertian(N, view, roughness, diffuse_color, f0) * structured_indices.sky_.intensity_;
			lighting += ImageBasedLightingRadianceGgx(N, view, roughness, f0) * structured_indices.sky_.intensity_;
		}
		else
		{
			lighting += diffuse_color * 0.03;
		}

		lighting += emissive;
	}

	float4 final_color = float4(lighting, base_color.a);

	RWTexture2D<uint> head_pointer = ResourceDescriptorHeap[structured_indices.oit_.head_pointer_index_];
	RWStructuredBuffer<OITFragment> fragment_buffer = ResourceDescriptorHeap[structured_indices.oit_.fragment_buffer_index_];
	RWByteAddressBuffer oit_counter = ResourceDescriptorHeap[structured_indices.oit_.counter_index_];

	uint max_fragments = uint(scene.screen_size_.x) * uint(scene.screen_size_.y) * OIT_MAX_LAYERS;

	uint new_index;
	oit_counter.InterlockedAdd(0, 1, new_index);
	if (new_index >= max_fragments)
	{
		return;
	}

	uint2 pixel = uint2(input.position.xy);
	uint old_head;
	InterlockedExchange(head_pointer[pixel], new_index, old_head);

	OITFragment frag;
	frag.packed_color_ = PackColorRGBA8(final_color);
	frag.depth_ = input.position.z;
	frag.next_ = old_head;
	fragment_buffer[new_index] = frag;
}
