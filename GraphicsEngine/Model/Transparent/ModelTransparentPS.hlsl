#include "../Model.hlsli"
#include "../../Shader/Structured.hlsli"
#include "../../Shader/Sampler.hlsli"
#include "../../Shader/Constants.hlsli"
#include "../../Shader/Normal.hlsli"
#include "../../Shader/Light.hlsli"
#include "../../Light/Cluster.hlsli"
#include "../../Light/BidirectionalReflectanceDistributionFunction.hlsli"
#include "../../Raytracing/Reflection/Reflection.hlsli"

float3 EvalTransparentDirectLight(float3 normal, float3 view, float3 light_direction, float3 diffuse_color, float3 f0, float roughness)
{
	float normal_dot_light = max(dot(normal, light_direction), 0.0);
	if (normal_dot_light <= 0.0)
	{
		return float3(0, 0, 0);
	}

	float normal_dot_view = max(dot(normal, view), 0.001);
	float3 half_vector = normalize(view + light_direction);
	float normal_dot_half = max(dot(normal, half_vector), 0.0);
	float view_dot_half = max(dot(view, half_vector), 0.0);

	float alpha_roughness = roughness * roughness;
	float3 f90 = float3(1.0, 1.0, 1.0);

	float3 diffuse = BrdfLambertian(f0, f90, diffuse_color, view_dot_half);
	float3 specular = BrdfSpecularGgx(f0, f90, alpha_roughness, view_dot_half, normal_dot_light, normal_dot_view, normal_dot_half);

	return (diffuse + specular) * normal_dot_light;
}

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
* the same base PBR as the deferred path (Lambert + GGX for the directional
* light AND for every clustered punctual light, diffuse/specular IBL); the
* extras that only exist per-instance in the deferred path (clearcoat/sheen/
* iridescence, ray-traced shadows/AO/GI) are deliberately left out - they need
* G-Buffer-resident screen-space inputs that transparent fragments do not have.
*
* The head pointer texture, fragment buffer, and counter are accessed
* as UAVs via the bindless descriptor heap.
*
* [earlydepthstencil] is mandatory, not an optimization: a pixel shader that
* writes UAVs and calls clip() runs with late depth-stencil by default, so the
* fragment would be appended to the linked list before the depth test, and the
* PSO has no render target and no depth write for a failed test to suppress.
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
* 同じ基本PBR(ディレクショナルライトにもクラスタ済みパンクチュアルライト
* にも Lambert + GGX、拡散/鏡面IBL)。deferred 側だけにある追加要素
* (クリアコート/シーン/虹彩、レイトレの影/AO/GI)はあえて省いている —
* それらは透明フラグメントが持ち得ない
* G-Buffer 常駐のスクリーン空間入力を必要とするため。
*
* ヘッドポインタテクスチャ、フラグメントバッファ、カウンターは
* バインドレスディスクリプタヒープ経由で UAV としてアクセスする。
*
* [earlydepthstencil] は最適化ではなく必須。UAV に書き込み clip() を持つ
* ピクセルシェーダは既定で後段深度ステンシルになるため、深度テストより先に
* リンクリストへ追加されてしまい、この PSO には落ちたテストが抑止できる
* レンダーターゲットも深度書き込みも無い。
*/
[earlydepthstencil]
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

	clip(base_color.a - instance.alpha_cutoff_);

	SceneConstantBuffer scene = GetSceneConstantBuffer();

	/// [JP] SV_Position.xy は既にピクセル中心(x+0.5)なのでそのまま NDC へ。
	float2 uv = input.position.xy * scene.inverse_screen_size_;
	float2 pixel_ndc = float2(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0);

	ModelSurface surface = ResolveModelSurface(instance, input.meshlet_index, primitive.triangle_in_meshlet_index, pixel_ndc, scene.current_view_projection_, scene.camera_position_.xyz, structured_indices.model_.bone_matrix_index_, structured_indices.model_.morph_weight_index_);

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
	roughness = clamp(roughness, 0.045, 1.0);

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
			lighting += EvalTransparentDirectLight(N, view, light_direction, diffuse_color, f0, roughness) * light_constant_buffer.directional_color_.rgb * light_constant_buffer.directional_intensity_;
		}

		/// [JP] Point/Spot/Rect。DeferredLightingPS.hlsl と同じクラスタ走査 +
		///      同じ BRDF。Reflection.hlsli の ComputeClusteredPunctualLighting は
		///      BRDF を通さない放射照度を返す仕様なので使わない。
		{
			float4 view_position = mul(float4(surface.world_position_, 1.0), scene.view_);
			float linear_depth = view_position.z;

			uint2 cluster_pixel = uint2(input.position.xy);
			uint3 cluster_count = uint3(light_constant_buffer.cluster_count_x_, light_constant_buffer.cluster_count_y_, CLUSTER_DEPTH_SLICES);
			uint tile_x = cluster_pixel.x / CLUSTER_TILE_SIZE;
			uint tile_y = cluster_pixel.y / CLUSTER_TILE_SIZE;
			uint slice = ComputeDepthSlice(linear_depth, scene.near_plane_, scene.far_plane_);
			uint cluster_index = ClusterIndex(uint3(tile_x, tile_y, slice), cluster_count);

			StructuredBuffer<ClusterData> cluster_data = ResourceDescriptorHeap[light_constant_buffer.cluster_data_shader_resource_view_index_];
			ByteAddressBuffer light_list = ResourceDescriptorHeap[light_constant_buffer.cluster_light_list_shader_resource_view_index_];

			ClusterData cluster = cluster_data[cluster_index];
			uint base = cluster_index * CLUSTER_STRIDE;

			if (cluster.point_count_ > 0)
			{
				StructuredBuffer<PointLightData> point_lights = ResourceDescriptorHeap[light_constant_buffer.point_light_index_];
				uint count = min(cluster.point_count_, CLUSTER_MAX_POINT_LIGHTS);

				for (uint index = 0; index < count; index++)
				{
					uint light_index = light_list.Load((base + index) * 4);
					PointLightData point_light = point_lights[light_index];

					float3 to_light = point_light.position - surface.world_position_;
					float distance_to_light = length(to_light);
					float3 light_direction = to_light / max(distance_to_light, 0.0001);
					float attenuation = AttenuateDistance(distance_to_light, point_light.range);

					float3 light_color = point_light.color.rgb * point_light.intensity * attenuation;
					lighting += EvalTransparentDirectLight(N, view, light_direction, diffuse_color, f0, roughness) * light_color;
				}
			}

			if (cluster.spot_count_ > 0)
			{
				StructuredBuffer<SpotLightData> spot_lights = ResourceDescriptorHeap[light_constant_buffer.spot_light_index_];
				uint count = min(cluster.spot_count_, CLUSTER_MAX_SPOT_LIGHTS);

				for (uint index = 0; index < count; index++)
				{
					uint light_index = light_list.Load((base + CLUSTER_MAX_POINT_LIGHTS + index) * 4);
					SpotLightData spot_light = spot_lights[light_index];

					float3 to_light = spot_light.position - surface.world_position_;
					float distance_to_light = length(to_light);
					float3 light_direction = to_light / max(distance_to_light, 0.0001);

					float cos_angle = dot(-light_direction, spot_light.direction);
					float spot_fade = saturate((cos_angle - spot_light.cos_half_angle) / max(spot_light.softness * (1.0 - spot_light.cos_half_angle), 0.0001));

					float attenuation = AttenuateDistance(distance_to_light, spot_light.range) * spot_fade;

					float3 light_color = spot_light.color.rgb * spot_light.intensity * attenuation;
					lighting += EvalTransparentDirectLight(N, view, light_direction, diffuse_color, f0, roughness) * light_color;
				}
			}

			if (cluster.rect_count_ > 0)
			{
				StructuredBuffer<RectLightData> rect_lights = ResourceDescriptorHeap[light_constant_buffer.rect_light_index_];
				uint count = min(cluster.rect_count_, CLUSTER_MAX_RECT_LIGHTS);

				for (uint index = 0; index < count; index++)
				{
					uint light_index = light_list.Load((base + CLUSTER_MAX_POINT_LIGHTS + CLUSTER_MAX_SPOT_LIGHTS + index) * 4);
					RectLightData rect_light = rect_lights[light_index];

					if (dot(surface.world_position_ - rect_light.position, rect_light.normal) <= 0.0)
					{
						continue;
					}

					float3 representative_point = ClosestPointOnRect(surface.world_position_, rect_light.position, rect_light.right, rect_light.up, rect_light.half_width, rect_light.half_height);
					float3 to_light = representative_point - surface.world_position_;
					float distance_to_light = length(to_light);
					float3 light_direction = to_light / max(distance_to_light, 0.0001);

					float attenuation = AttenuateDistance(distance_to_light, rect_light.range);
					float3 light_color = rect_light.color.rgb * rect_light.intensity * attenuation;
					lighting += EvalTransparentDirectLight(N, view, light_direction, diffuse_color, f0, roughness) * light_color;
				}
			}
		}

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

	uint max_fragments = structured_indices.oit_.fragment_capacity_;

	uint new_index;
	oit_counter.InterlockedAdd(0, 1, new_index);
	if (new_index >= max_fragments)
	{
		return;
	}

	uint2 pixel = uint2(input.position.xy);
	uint old_head;
	InterlockedExchange(head_pointer[pixel], new_index, old_head);

	uint2 packed_color = PackColorHalf4(final_color);

	OITFragment frag;
	frag.packed_color_rg_ = packed_color.x;
	frag.packed_color_ba_ = packed_color.y;
	frag.depth_ = input.position.z;
	frag.next_ = old_head;
	fragment_buffer[new_index] = frag;
}
