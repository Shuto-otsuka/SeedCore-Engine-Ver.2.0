#include "../../Shader/Constants.hlsli"
#include "../../Shader/Structured.hlsli"
#include "../../Shader/Light.hlsli"
#include "../../Shader/Normal.hlsli"
#include "../../Shader/Noise.hlsli"
#include "../../Sky/SkyMath.hlsli"
#include "../../Light/ImageBasedLighting.hlsli"
#include "../VolumetricCloudScapes/VolumetricCloudScapes.hlsli"
#include "Reflection.hlsli"

/**
* [EN]
* Ray-traced glossy reflection (RTPSO / DispatchRays, raygen + miss +
* closesthit). Reflects the view ray off the G-Buffer surface and traces one
* GGX-importance-sampled ray per pixel per frame (SkyMath.hlsli's
* ImportanceSampleGgx, the same half-vector distribution SpecularPrefilterCS.hlsl
* uses to prefilter the IBL cube, driven here by a per-pixel random sample
* instead of a fixed Hammersley set). At roughness 0 the half-vector always
* equals the normal, so this degenerates to the old exact mirror ray with no
* noise; roughness > 0 spreads the ray direction and needs
* ReflectionDenoiseCS.hlsl's spatio-temporal accumulation to converge. miss
* samples the environment cube; closesthit re-fetches the hit triangle's
* vertices through the per-instance table (InstanceID() -> ReflectionInstanceData)
* and relights the hit point with the directional/punctual lights + diffuse IBL.
*
* [JP]
* レイトレ光沢反射(RTPSO / DispatchRays、raygen + miss + closesthit)。
* G-Buffer の面で視線を反射し、1ピクセル1フレームにつき GGX 重点サンプリング
* したレイを1本撃つ(SkyMath.hlsli の ImportanceSampleGgx — IBL キューブの
* プリフィルタに SpecularPrefilterCS.hlsl が使うのと同じハーフベクトル分布を、
* ここでは固定 Hammersley 集合ではなくピクセルごとの乱数サンプルで駆動する)。
* roughness 0 ではハーフベクトルが常に法線と一致するため、ノイズ無しの旧・
* 厳密ミラーレイへ縮退する。roughness > 0 ではレイ方向が広がるため、
* ReflectionDenoiseCS.hlsl の空間+時間蓄積で収束させる必要がある。miss は
* 環境キューブをサンプル。closesthit はインスタンステーブル(InstanceID() →
* ReflectionInstanceData)経由でヒット三角形の頂点を引き直し、
* ディレクショナル/ポイント/スポット/矩形ライト+拡散IBLでヒット点を
* 再ライティングする。
*/

// radiance(12) + hit_distance(4) = 16 bytes, fits the default
// maxPayloadSizeInBytes (16).
struct ReflectionPayload
{
	float3 radiance_;
	float hit_distance_;
};

[shader("raygeneration")]
void ReflectionRayGeneration()
{
	uint2 pixel = DispatchRaysIndex().xy;
	RWTexture2D<float4> output = ResourceDescriptorHeap[structured_indices.reflection_.output_uav_index_];

	// 背景(reverse-Z 遠平面=0)は反射なし。a=0 で「無効」を示す。
	Texture2D<float> depth_texture = ResourceDescriptorHeap[structured_indices.gbuffer_.depth_index_];
	float depth = depth_texture.Load(int3(pixel, 0));

	if (depth == 0.0)
	{
		output[pixel] = float4(0, 0, 0, 0);
		return;
	}

	SceneConstantBuffer scene = GetSceneConstantBuffer();
	ConstantBuffer<ReflectionRayConstantBuffer> tuning = ResourceDescriptorHeap[structured_indices.reflection_.ray_constant_index_];

	// ワールド座標と法線を復元(ShadowRT.hlsl と同じ手順、mul は行ベクトル左)。
	float2 uv = (float2(pixel) + 0.5) * scene.inverse_screen_size_;
	float2 ndc = float2(uv.x * 2 - 1, 1 - uv.y * 2);
	float4 clip = float4(ndc, depth, 1.0);
	float4 world = mul(clip, scene.inverse_view_projection_);
	float3 world_position = world.xyz / world.w;

	// gbuffer1: rg = octエンコード法線、b = ラフネス(DeferredLightingPS.hlsl
	// と同じ詰め方)。
	Texture2D<float4> normal_texture = ResourceDescriptorHeap[structured_indices.gbuffer_.index_1_];
	float4 gbuffer1 = normal_texture.Load(int3(pixel, 0));
	float3 normal = OctNormalDecode(gbuffer1.rg);
	float roughness = gbuffer1.b;

	float3 view_direction = normalize(world_position - scene.camera_position_.xyz);
	float3 mirror_direction = normalize(reflect(view_direction, normal));

	// GGX 重点サンプリングでハーフベクトルをサンプルし、視線をそのまわりへ
	// 反射させる(ImageBasedLighting の畳み込みと同じ式: L = 2*(V.H)*H - V、
	// V はここでは surface→camera 方向)。roughness 0 では
	// ImportanceSampleGgx が常に H=normal を返すので mirror_direction と厳密に
	// 一致する(ノイズ無し)。
	uint rng_state = SeedFromPixel(pixel, tuning.frame_index_ + 2654435761u);
	float3 view = -view_direction;
	float3 half_vector = ImportanceSampleGgx(Rand2(rng_state), normal, roughness);
	float3 reflect_direction = normalize(2.0 * dot(view, half_vector) * half_vector - view);

	// [JP] GGX半球サンプルは稀に法線の下(dot(N,L)<=0)を向く(特に高ラフネス+
	// 視線がすれすれの時)。有効なレイが要るので、その場合は決定論的な
	// ミラー方向へフォールバックする(エネルギーを捨てて真っ黒にするより、
	// 偏りは小さい)。
	if (dot(normal, reflect_direction) <= 0.0)
	{
		reflect_direction = mirror_direction;
	}

	RaytracingAccelerationStructure tlas = ResourceDescriptorHeap[structured_indices.raytracing_.tlas_index_];

	RayDesc ray_desc;
	ray_desc.Origin = world_position + normal * tuning.normal_bias_;
	ray_desc.Direction = reflect_direction;
	ray_desc.TMin = 0.001;
	ray_desc.TMax = tuning.ray_t_max_;

	ReflectionPayload payload;
	payload.radiance_ = float3(0, 0, 0);
	payload.hit_distance_ = 0.0;

	// 最近接ヒットが要るので first-hit 打ち切りフラグは付けない。
	TraceRay(tlas, RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES, 0xFF, 0, 0, 0, ray_desc, payload);

	output[pixel] = float4(payload.radiance_, 1.0);
}

[shader("miss")]
void ReflectionMiss(inout ReflectionPayload payload)
{
	// レイが空へ抜けた。スカイマップがあれば環境キューブを、無ければ
	// プロシージャル空(有効時)を、それも無ければ黒をサンプルする。
	if (structured_indices.sky_.environment_cube_index_ != 0)
	{
		payload.radiance_ = SampleSkyboxEnvironment(WorldRayDirection()).rgb;
	}
	else
	{
		ConstantBuffer<VolumetricCloudScapesRayConstantBuffer> cloud_tuning = ResourceDescriptorHeap[structured_indices.cloud_.ray_constant_index_];
		if (cloud_tuning.procedural_sky_enabled_ != 0 && structured_indices.sky_.specular_prefiltered_index_ != 0)
		{
			// プロシージャル空モード: 雲まで焼き込み済みの prefilter キューブ
			// (ProceduralSkyToCubeCS でベイク→畳み込み)の mip0 をサンプルする。
			// 128px 相当なので鏡面でもわずかに柔らかいが、雲が映る方が重要。
			TextureCube<float4> prefiltered = ResourceDescriptorHeap[structured_indices.sky_.specular_prefiltered_index_];
			payload.radiance_ = prefiltered.SampleLevel(sampler_linear_clamp, WorldRayDirection(), 0).rgb;
		}
		else if (cloud_tuning.procedural_sky_enabled_ != 0)
		{
			// ベイクがまだ済んでいない最初の数フレームは解析的な空+太陽で代用。
			ConstantBuffer<LightConstantData> light = ResourceDescriptorHeap[constant_indices.light_index_];
			float3 sun_direction = normalize(-light.directional_direction_);
			float3 sun_radiance = light.directional_color_.rgb * light.directional_intensity_;
			payload.radiance_ = ProceduralSkyColor(WorldRayDirection(), sun_direction, sun_radiance, cloud_tuning);
		}
		else
		{
			payload.radiance_ = float3(0, 0, 0);
		}
	}
	payload.hit_distance_ = 100000.0;
}

[shader("closesthit")]
void ReflectionClosestHit(inout ReflectionPayload payload, in BuiltInTriangleIntersectionAttributes attributes)
{
	// インスタンステーブルからヒットメッシュの頂点/インデックスSRVを引き、
	// barycentrics で法線を補間する。
	StructuredBuffer<ReflectionInstanceData> instances = ResourceDescriptorHeap[structured_indices.raytracing_.instance_data_index_];
	ReflectionInstanceData instance = instances[InstanceID()];

	StructuredBuffer<uint> triangle_indices = ResourceDescriptorHeap[instance.index_buffer_index_];
	StructuredBuffer<ReflectionVertex> vertices = ResourceDescriptorHeap[instance.vertex_buffer_index_];

	uint base_index = PrimitiveIndex() * 3;
	ReflectionVertex vertex0 = vertices[triangle_indices[base_index + 0]];
	ReflectionVertex vertex1 = vertices[triangle_indices[base_index + 1]];
	ReflectionVertex vertex2 = vertices[triangle_indices[base_index + 2]];

	float2 barycentrics = attributes.barycentrics;
	float weight0 = 1.0 - barycentrics.x - barycentrics.y;
	float weight1 = barycentrics.x;
	float weight2 = barycentrics.y;

	float3 object_normal =
		DecodeReflectionVertexNormal(vertex0) * weight0 +
		DecodeReflectionVertexNormal(vertex1) * weight1 +
		DecodeReflectionVertexNormal(vertex2) * weight2;

	float2 texcoord =
		DecodeReflectionVertexTexcoord(vertex0, instance.texcoord_min_, instance.texcoord_extent_) * weight0 +
		DecodeReflectionVertexTexcoord(vertex1, instance.texcoord_min_, instance.texcoord_extent_) * weight1 +
		DecodeReflectionVertexTexcoord(vertex2, instance.texcoord_min_, instance.texcoord_extent_) * weight2;

	// オブジェクト空間→ワールド空間。非一様スケールの厳密な逆転置は省略
	// (v1近似 — 一様スケール/回転/平行移動なら正しい)。
	float3 world_normal = normalize(mul((float3x3)ObjectToWorld3x4(), object_normal));

	float3 hit_position = WorldRayOrigin() + WorldRayDirection() * RayTCurrent();

	// ヒット面の再ライティング(簡易版): ディレクショナルライト+Point/Spot/Rect
	// のランバート項 + 拡散IBL(無ければフラット環境光)。鏡面項・影(Point/Spot/
	// Rect側)・テクスチャは省略のv1。
	SceneConstantBuffer scene = GetSceneConstantBuffer();
	ConstantBuffer<LightConstantData> light = ResourceDescriptorHeap[constant_indices.light_index_];

	float3 lighting = float3(0, 0, 0);

	if (structured_indices.sky_.diffuse_irradiance_index_ != 0)
	{
		// レイトレシェーダでは暗黙LODの Sample が使えない(SampleLevel のみ)
		// ため、ヘルパー(SampleDiffuseIrradianceEnvironmentMap)を経由せず
		// 直接 SampleLevel で読む。irradiance キューブはミップ1枚なので等価。
		//
		// sky_intensity_ はここでは掛けない。DeferredLightingPS.hlsl が
		// トレース反射を ibl_specular へ lerp した後に lighting 全体へ
		// 一度掛けるので、ここで掛けると二重適用になる(既定値 1.0 では
		// 見えないが、空の明るさを変えた瞬間に反射だけ二乗で効く)。
		// irradiance キューブは畳み込み時に 1/PI 込みの規約なので、
		// ImageBasedLightingRadianceLambertian と同じく素の乗算で正しい。
		TextureCube<float4> diffuse_irradiance = ResourceDescriptorHeap[structured_indices.sky_.diffuse_irradiance_index_];
		lighting += diffuse_irradiance.SampleLevel(sampler_linear_clamp, world_normal, 0).rgb;
	}
	else
	{
		lighting += 0.03;
	}

	if (light.directional_intensity_ > 0.0)
	{
		float3 light_direction = normalize(-light.directional_direction_);
		float normal_dot_light = saturate(dot(world_normal, light_direction));

		// 反射に映る面の影。closesthit から TraceRay は撃てない
		// (ReflectionShader.cpp の maxTraceRecursionDepth_ = 1)が、インライン
		// RayQuery は再帰深度の制限対象外なので、RTPSO の設定を変えずに影を
		// 撃てる。これが無いと反射の中だけ全ての面が全照射になり、影のある
		// 直接描画との差が「反射だけライティングが違う」として出る。
		float sun_visibility = 1.0;

		if (normal_dot_light > 0.0)
		{
			ConstantBuffer<ReflectionRayConstantBuffer> tuning = ResourceDescriptorHeap[structured_indices.reflection_.ray_constant_index_];
			RaytracingAccelerationStructure tlas = ResourceDescriptorHeap[structured_indices.raytracing_.tlas_index_];

			RayDesc shadow_ray;
			shadow_ray.Origin = hit_position + world_normal * tuning.normal_bias_;
			shadow_ray.Direction = light_direction;
			shadow_ray.TMin = 0.001;
			shadow_ray.TMax = tuning.ray_t_max_;

			RayQuery<RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES> shadow_query;
			shadow_query.TraceRayInline(tlas, RAY_FLAG_NONE, 0xFF, shadow_ray);
			shadow_query.Proceed();

			if (shadow_query.CommittedStatus() == COMMITTED_TRIANGLE_HIT)
			{
				sun_visibility = 0.0;
			}
		}

		// 1/PI が必要。直接光側のランバート BRDF は
		// BidirectionalReflectanceDistributionFunction.hlsli の BrdfLambertian
		// が albedo/PI を返す規約なので、ここで割らないと【反射に映る面だけが
		// 直接光を PI 倍(約3.14倍)受ける】。太陽強度 1.0・アルベド 1.0 だと
		// 直接見た面は 0.318 で収まるのに反射側は 1.0 に張り付くため、
		// 階調が飛んで「ライティングが効いていない単色」に見える。
		const float lambert_normalization = 1.0 / 3.14159265358979;
		lighting += light.directional_color_.rgb * light.directional_intensity_ * normal_dot_light * sun_visibility * lambert_normalization;
	}

	// Point/Spot/Rect ライト。ShadowRT.hlsl と同じクラスタ(法線での N・L
	// ゲート込み)から拾うが、影レイは撃たない(Reflection.hlsli の
	// ComputeClusteredPunctualLighting 参照 — ライト数ぶんレイが増えるため)。
	lighting += ComputeClusteredPunctualLighting(hit_position, world_normal, scene, light);

	// ヒット三角形が属するサブメッシュのマテリアルを解決する(メッシュ全体で
	// 単一色にしない — 複数マテリアルのメッシュ(例: 壁ごとに色が違う Cornell
	// box を1つの Crister で作った場合)で、実際に当たった壁と無関係な色が
	// 返っていたのを修正)。
	ReflectionMaterialData material = ResolveReflectionMaterial(instance, PrimitiveIndex());

	// ベースカラーテクスチャ。レイトレシェーダには暗黙の微分が無いので
	// SampleLevel 固定(ミップ選択はレイのフットプリントから出すのが本式だが、
	// 鏡面反射のフットプリントは狭いので mip0 で始める)。
	float3 albedo = material.base_color_;

	if (material.base_color_texture_index_ != 0xFFFFFFFF)
	{
		Texture2D<float4> base_color_texture = ResourceDescriptorHeap[material.base_color_texture_index_];
		albedo *= base_color_texture.SampleLevel(sampler_linear_wrap, texcoord, 0).rgb;
	}

	payload.radiance_ = albedo * lighting;
	payload.hit_distance_ = RayTCurrent();
}
