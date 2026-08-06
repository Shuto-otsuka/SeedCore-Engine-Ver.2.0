#include "Model.hlsli"
#include "../Shader/Constants.hlsli"
#include "../Shader/Structured.hlsli"
#include "../Shader/Normal.hlsli"
#include "../Shader/Light.hlsli"
#include "../Light/Cluster.hlsli"
#include "../Light/BidirectionalReflectanceDistributionFunction.hlsli"
#include "../Raytracing/Shadow/Shadow.hlsli"
#include "../Raytracing/AmbientOcclusion/AmbientOcclusion.hlsli"
#include "../Raytracing/SubsurfaceScattering/SubsurfaceScattering.hlsli"
#include "../Raytracing/Reflection/Reflection.hlsli"
#include "../Raytracing/VolumetricCloudScapes/VolumetricCloudScapes.hlsli"
#include "../Raytracing/Froxel/Froxel.hlsli"
#include "../Raytracing/VolumetricLight/VolumetricLight.hlsli"
#include "../Shader/Noise.hlsli"
#include "../Shader/Precipitation.hlsli"

/// [JP] Froxel フォグ/体積光の積分ボリューム(rgb=累積散乱、a=透過率)を
///      uv+ビュー空間Zでサンプルしてシーン色へ合成する。無効時はレンダラーが
///      (0,0,0,1)にクリアするので常に呼んでよい。
float3 ApplyVolumetricFog(float3 color, float2 uv, float view_z)
{
	SceneConstantBuffer fog_scene = GetSceneConstantBuffer();
	Texture3D<float4> integration_volume = ResourceDescriptorHeap[structured_indices.volumetric_light_.integration_srv_index_];

	float slice_t = saturate(ViewZToFroxelSlice(max(view_z, fog_scene.near_plane_), fog_scene.near_plane_, fog_scene.far_plane_));
	float4 fog = integration_volume.SampleLevel(sampler_linear_clamp, float3(uv, slice_t), 0);

	return color * fog.a + fog.rgb;
}

struct CompositeOutput
{
	float4 position : SV_Position;
	float2 texcoord : TEXCOORD0;
};

static const float AMBIENT = 0.03;

float3 ReconstructWorldPosition(float2 uv, float depth)
{
	SceneConstantBuffer scene = GetSceneConstantBuffer();
	float4 ndc = float4(uv * 2.0 - 1.0, depth, 1.0);
	ndc.y = -ndc.y;
	float4 world_position = mul(ndc, scene.inverse_view_projection_);
	return world_position.xyz / world_position.w;
}

/// [JP] KHR_materials_sheen 用の Charlie NDF(布特有のグレージング角ハイライト)。
///      sin(theta_h) は sqrt(1 - cos^2) で求める。
float CharlieDistribution(float roughness, float normal_dot_half)
{
	const float PI = 3.14159265358979;
	float alpha = max(roughness, 0.001);
	float inv_alpha = 1.0 / alpha;
	float cos2h = normal_dot_half * normal_dot_half;
	float sin2h = max(1.0 - cos2h, 0.0078125);
	return (2.0 + inv_alpha) * pow(sin2h, inv_alpha * 0.5) / (2.0 * PI);
}

/// [JP] KHR_materials_sheen 用の Ashikhmin 可視性項。
float AshikhminVisibility(float normal_dot_light, float normal_dot_view)
{
	return 1.0 / max(4.0 * (normal_dot_light + normal_dot_view - normal_dot_light * normal_dot_view), 1e-4);
}

float3 EvalDirectLight(float3 normal, float3 view, float3 light_direction, float3 diffuse_color, float3 f0, float roughness, float clearcoat, float clearcoat_roughness, float3 sheen_color, float sheen_roughness)
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

	float3 base = diffuse + specular;

	/// [EN] Clearcoat: a second GGX lobe with a fixed dielectric F0 (IOR ~1.5 -> 0.04)
	///      layered on top; the base layer is attenuated by the coat Fresnel.
	if (clearcoat > 0.0)
	{
		const float3 coat_f0 = float3(0.04, 0.04, 0.04);
		float coat_alpha = clearcoat_roughness * clearcoat_roughness;
		float3 coat_fresnel = FresnelSchlick(coat_f0, f90, view_dot_half) * clearcoat;
		float3 coat_specular = BrdfSpecularGgx(coat_f0, f90, coat_alpha, view_dot_half, normal_dot_light, normal_dot_view, normal_dot_half) * clearcoat;
		base = base * (1.0 - coat_fresnel) + coat_specular;
	}

	/// [EN] Sheen: additive Charlie lobe (fabric grazing-angle highlight). Not
	///      energy-conserving against the base layer (the full KHR spec scales
	///      the base by a sheen directional-albedo LUT) - simple additive
	///      approximation.
	/// [JP] Sheen: 加算のCharlieローブ(布のグレージング角ハイライト)。ベース層への
	///      エネルギー保存はしていない(KHR仕様はsheenディレクショナルアルベドLUTで
	///      ベースを減衰させる) - 単純な加算近似。
	if (dot(sheen_color, sheen_color) > 0.0)
	{
		float sheen_ndf = CharlieDistribution(sheen_roughness, normal_dot_half);
		float sheen_visibility = AshikhminVisibility(normal_dot_light, normal_dot_view);
		base += sheen_color * sheen_ndf * sheen_visibility;
	}

	return base * normal_dot_light;
}

float4 main(CompositeOutput input) : SV_Target0
{
	Texture2D<float4> gbuffer0 = ResourceDescriptorHeap[structured_indices.gbuffer_.index_0_];
	Texture2D<float4> gbuffer1 = ResourceDescriptorHeap[structured_indices.gbuffer_.index_1_];
	Texture2D<float4> gbuffer3 = ResourceDescriptorHeap[structured_indices.gbuffer_.index_3_];
	Texture2D<float> gbuffer_depth = ResourceDescriptorHeap[structured_indices.gbuffer_.depth_index_];

	uint2 pixel = uint2(input.position.xy);

	float4 rt0 = gbuffer0.Load(int3(pixel, 0));
	float4 rt1 = gbuffer1.Load(int3(pixel, 0));
	float4 rt3 = gbuffer3.Load(int3(pixel, 0));
	float depth = gbuffer_depth.Load(int3(pixel, 0));

	/// [JP] 深度モード(3)は背景も含めて画面全体を深度で塗るため、背景スキップより
	///      先に処理する。非線形 reverse-Z をビュー空間Zにしてリニア化し、
	///      近=白 / 遠=黒 に反転（近い物のシルエットが見えるように）。
	///      DEPTH_VIEW_RANGE で正規化する距離を調整（小さいほど近距離のコントラスト大）。
	if (constant_indices.view_mode_ == 3)
	{
		const float DEPTH_VIEW_RANGE = 50.0;
		SceneConstantBuffer depth_scene = GetSceneConstantBuffer();
		float3 depth_world = ReconstructWorldPosition(input.texcoord, depth);
		float view_z = mul(float4(depth_world, 1.0), depth_scene.view_).z;
		float linear_depth = saturate(view_z / DEPTH_VIEW_RANGE);
		float shade = 1.0 - linear_depth;
		return float4(shade, shade, shade, 1.0);
	}

	if (dot(rt1, rt1) == 0.0)
	{
		/// [EN] Background pixel (no geometry). If a skymap is bound, draw the
		///      environment cube as the skybox by reconstructing the view ray
		///      from the far-plane depth; otherwise leave the pixel untouched.
		/// [JP] 背景ピクセル(ジオメトリなし)。スカイマップがバインドされていれば
		///      遠クリップ深度からビュー方向を復元し environment キューブを
		///      スカイボックスとして描く。なければ何も描かない。
		if (structured_indices.sky_.environment_cube_index_ != 0)
		{
			/// [EN] Skybox background is drawn at the raw environment radiance,
			///      independent of sky_intensity_ (which only scales the IBL
			///      contribution to lit surfaces). The procedural sky+cloud
			///      system is skymap-less only, so no clouds here.
			/// [JP] スカイボックス背景は環境の生の放射輝度で描画し、
			///      sky_intensity_ の影響を受けない（intensity は物体への IBL
			///      寄与のみをスケールする）。プロシージャル空+雲システムは
			///      スカイマップ無効時専用なので、ここでは雲を合成しない。
			SceneConstantBuffer sky_scene = GetSceneConstantBuffer();
			float3 sky_world = ReconstructWorldPosition(input.texcoord, depth);
			float3 sky_direction = normalize(sky_world - sky_scene.camera_position_.xyz);
			float3 skybox_color = SampleSkyboxEnvironment(sky_direction).rgb;

			/// [JP] 雷の閃光はスカイマップ使用時も独立して効く。
			ConstantBuffer<LightConstantData> skybox_light = ResourceDescriptorHeap[constant_indices.light_index_];
			skybox_color += skybox_light.thunder_flash_ * float3(0.85, 0.9, 1.0) * 2.5;
			skybox_color += LightningBoltMask(input.texcoord, skybox_light.thunder_seed_, saturate(skybox_light.thunder_flash_ - 0.5) * 2.0);

			/// [JP] 空は最遠なのでフォグは far スライスで適用(地平線が霞む)。
			skybox_color = ApplyVolumetricFog(skybox_color, input.texcoord, sky_scene.far_plane_);
			return float4(skybox_color, 1.0);
		}

		/// [JP] スカイマップ無し: プロシージャル空システム(グラフィックス→
		///      レイトレーシング→雲)が有効なら、空のグラデーション+太陽
		///      ディスクを描き、その上にボリューメトリック雲を合成する。
		///      雲の rgb は事前乗算済みの内散乱、a はカバレッジ。
		ConstantBuffer<VolumetricCloudScapesRayConstantBuffer> cloud_tuning = ResourceDescriptorHeap[structured_indices.cloud_.ray_constant_index_];
		if (cloud_tuning.procedural_sky_enabled_ != 0)
		{
			SceneConstantBuffer sky_scene = GetSceneConstantBuffer();
			float3 sky_world = ReconstructWorldPosition(input.texcoord, depth);
			float3 view_direction = normalize(sky_world - sky_scene.camera_position_.xyz);

			ConstantBuffer<LightConstantData> sky_light = ResourceDescriptorHeap[constant_indices.light_index_];
			float3 sun_direction = normalize(-sky_light.directional_direction_);
			float3 sun_radiance = sky_light.directional_color_.rgb * sky_light.directional_intensity_;

			float3 sky_color = ProceduralSkyColor(view_direction, sun_direction, sun_radiance, cloud_tuning);

			/// [JP] 星/月/流れ星は雲より奥にあるので、雲を合成する前に加算する
			///      (雲のカバレッジで自然に隠れるように)。
			Texture2D<float4> star_texture = ResourceDescriptorHeap[structured_indices.star_.output_srv_index_];
			float4 star = star_texture.SampleLevel(sampler_linear_clamp, input.texcoord, 0);
			sky_color += star.rgb;

			/// [JP] 雲は縮小解像度で描かれている(VolumetricCloudScapesRenderer の
			///      resolutionDivisor_)ので、Load ではなくバイリニアで拡大する。
			///      値は事前乗算済み(rgb は透過率を掛けた放射輝度、a はカバレッジ)
			///      なので、そのまま線形補間して合成してよい。
			Texture2D<float4> cloud_texture = ResourceDescriptorHeap[structured_indices.cloud_.output_srv_index_];
			float4 cloud = cloud_texture.SampleLevel(sampler_linear_clamp, input.texcoord, 0);
			sky_color = sky_color * (1.0 - cloud.a) + cloud.rgb;

			/// [JP] 雷の閃光は空全体も一瞬明るく染める。ボルト本体も閃光のピーク
			///      付近だけ重ねて描く。
			sky_color += sky_light.thunder_flash_ * float3(0.85, 0.9, 1.0) * 2.5;
			sky_color += LightningBoltMask(input.texcoord, sky_light.thunder_seed_, saturate(sky_light.thunder_flash_ - 0.5) * 2.0);

			/// [JP] プロシージャル空にもフォグを far スライスで適用。
			sky_color = ApplyVolumetricFog(sky_color, input.texcoord, sky_scene.far_plane_);
			return float4(sky_color, 1.0);
		}

		discard;
	}

	float3 base_color = rt0.rgb;
	float metallic = rt0.a;
	float3 normal = OctNormalDecode(rt1.rg);
	float roughness = rt1.b;
	/// [JP] emissive はテクスチャ*factorの生値(RT3)。VisID(RT4)から
	///      instance_indexを引いてModelInstanceを直接読み、per-instance定数の
	///      KHR拡張スカラー(ior/specular/clearcoat/transmission/volume/sheen/
	///      iridescence/anisotropy/unlit/emissive_strength)はGBufferに焼き込まず
	///      ここ(ライティングのその場)で評価する。
	float3 emissive = rt3.rgb;

	Texture2D<uint2> gbuffer4 = ResourceDescriptorHeap[structured_indices.gbuffer_.index_4_];
	uint2 visibility_id = gbuffer4.Load(int3(pixel, 0));
	uint material_instance_index, material_meshlet_index, material_triangle_index;
	UnpackVisibilityID(visibility_id, material_instance_index, material_meshlet_index, material_triangle_index);
	StructuredBuffer<ModelInstance> material_instances = ResourceDescriptorHeap[structured_indices.model_.instance_index_];
	ModelInstance material_instance = material_instances[material_instance_index];
	emissive *= material_instance.emissive_strength_;

	/// [JP] エディタービューの表示モード（ViewMode）。Lit(0) は下の通常ライティングへ流す。
	///      Wireframe(2) / Meshlet(4) は別パスで扱うためここでは Lit と同じ扱い（素通り）。
	switch (constant_indices.view_mode_)
	{
		case 1: // ライティングなし（アルベド）
			return float4(base_color, 1.0);
		case 5: // 法線
			return float4(normal * 0.5 + 0.5, 1.0);
		case 6: // ラフネス
			return float4(roughness, roughness, roughness, 1.0);
		case 7: // メタルネス
			return float4(metallic, metallic, metallic, 1.0);
		case 8: // エミッシブ
			return float4(emissive, 1.0);
		case 9: // モーションベクター
		{
			Texture2D<float2> gbuffer_velocity = ResourceDescriptorHeap[structured_indices.gbuffer_.index_2_];
			float2 velocity = gbuffer_velocity.Load(int3(pixel, 0));
			return float4(velocity * 0.5 + 0.5, 0.0, 1.0);
		}
	}

	/// [JP] KHR_materials_unlit: ライティングを一切せず base_color + emissive のみ。
	if (material_instance.unlit_ > 0.5)
	{
		return float4(base_color + emissive, 1.0);
	}

	float3 world_position = ReconstructWorldPosition(input.texcoord, depth);

	SceneConstantBuffer scene = GetSceneConstantBuffer();
	float3 view = normalize(scene.camera_position_.xyz - world_position);

	/// [JP] 天候の見た目への反映(WeatherSystem::ReadGpuState経由)。上向きの
	///      面ほど強く - 濡れは粗さを落としてテカらせ、雪は白へブレンドする。
	///      diffuse_color/f0 の算出前に base_color/roughness へ適用することで
	///      環境光/反射/直接光/GI すべてに一貫して効く。
	ConstantBuffer<LightConstantData> weather_light = ResourceDescriptorHeap[constant_indices.light_index_];
	float weather_up_facing = saturate(normal.y);

	float wetness = weather_light.wetness_ * weather_up_facing;
	roughness = lerp(roughness, roughness * 0.25, wetness);
	base_color = lerp(base_color, base_color * 0.8, wetness * 0.6);

	/// [JP] 水たまり: ほぼ水平な面(法線がほぼ真上)だけ、濡れ具合に応じて
	///      さらに鏡面寄りへ押す。傾いた面(壁など)は上の薄い水膜の
	///      テカりだけに留め、水が「溜まる」のは平らな地面だけにする。
	float flatness = saturate((normal.y - 0.85) / 0.15);
	float puddle = wetness * flatness;
	roughness = lerp(roughness, 0.03, puddle);
	base_color = lerp(base_color, base_color * 0.5, puddle * 0.5);

	float snow = weather_light.snow_coverage_ * weather_up_facing;
	base_color = lerp(base_color, float3(0.95, 0.96, 1.0), snow);
	roughness = lerp(roughness, 0.9, snow);

	/// [JP] KHR_materials_ior/specular からその場で誘電体 F0 を計算。金属は base_color。
	float dielectric = (material_instance.ior_ - 1.0) / (material_instance.ior_ + 1.0);
	dielectric *= dielectric;
	float3 dielectric_f0 = saturate(dielectric * material_instance.specular_color_ * material_instance.specular_factor_);

	/// [JP] KHR_materials_iridescence: 簡易近似(物理的に正確な薄膜干渉ではなく、
	///      厚み/視野角で位相をずらした疑似虹色シフト)で F0 を色付けする。
	if (material_instance.iridescence_factor_ > 0.0)
	{
		float normal_dot_view_for_iridescence = saturate(dot(normal, view));
		float phase = material_instance.iridescence_thickness_ * 0.01 * normal_dot_view_for_iridescence + material_instance.iridescence_ior_;
		float3 iridescence_shift = sin(float3(phase, phase + 2.094395, phase + 4.18879)) * 0.5 + 0.5;
		dielectric_f0 = lerp(dielectric_f0, iridescence_shift, saturate(material_instance.iridescence_factor_));
	}

	/// [JP] KHR_materials_transmission: 透過する光は拡散反射しない。実際に背後を
	///      透かして見せる屈折表現(スクリーン空間/レイトレの Refraction)は別途
	///      実装予定 - ここでは拡散応答の減衰のみ反映する。
	float3 diffuse_color = base_color * (1.0 - metallic) * (1.0 - material_instance.transmission_factor_);
	float3 f0 = lerp(dielectric_f0, base_color, metallic);
	/// [JP] 濡れた面は薄い水膜でグレージング角の反射率が上がる。水たまりは
	///      水面そのものとしてさらに強く反射させる。
	f0 = lerp(f0, max(f0, 0.02), wetness);
	f0 = lerp(f0, max(f0, 0.05), puddle);

	float clearcoat_factor = material_instance.clearcoat_factor_;
	float clearcoat_roughness = material_instance.clearcoat_roughness_;
	float3 sheen_color = material_instance.sheen_color_;
	float sheen_roughness = max(material_instance.sheen_roughness_, 0.001);

	/// [JP] ShadowRT.hlsl(+ShadowDenoiseCS.hlsl で時間積分済み)が書いた
	///      2チャンネル可視性(r=ディレクショナル, g=Point/Spot/Rect のうち
	///      確率的に選ばれた1灯)。ビューごとの蓄積チェーンなので
	///      constant_indices 側から取る。shadow_strength_ で効きの強さを
	///      0(常に照射扱い)〜1(可視性をそのまま適用)で調整する。
	///      各チャンネルは対応する直接光の項【だけ】に掛ける。環境光(IBL)には
	///      掛けない — 太陽から見えない面(壁の裏・屋内)は可視性0になるため、
	///      環境光まで消すと真っ黒に潰れる。影の暗さの下限は環境光が決める
	///      (物理的にもそれが正しい)。もっと暗い影にしたければ環境光/スカイ
	///      強度を下げるか太陽強度を上げる。
	Texture2D<float2> shadow_visibility = ResourceDescriptorHeap[constant_indices.shadow_.visibility_srv_index_];
	float2 visibility = shadow_visibility.Load(int3(pixel, 0));

	ConstantBuffer<ShadowRayConstantBuffer> shadow_tuning = ResourceDescriptorHeap[structured_indices.shadow_.ray_constant_index_];
	float directional_shadow_factor = lerp(1.0, visibility.x, saturate(shadow_tuning.shadow_strength_));
	float punctual_shadow_factor = lerp(1.0, visibility.y, saturate(shadow_tuning.shadow_strength_));

	/// [EN] Ambient term: image-based lighting when a skymap is bound
	///      (irradiance index != 0), otherwise the flat fallback ambient.
	/// [JP] 環境光項: スカイマップがバインドされていれば（irradiance インデックス
	///      != 0）IBL、なければ従来のフラットな環境光にフォールバック。
	/// [JP] レイトレ反射(ReflectionDenoiseCS.hlsl がビューごとに空間+時間
	///      デノイズした後の放射輝度、a=1 有効)。GGX重点サンプリング
	///      (ReflectionRT.hlsl)は roughness に応じて自然にボケる(roughness 0
	///      は厳密ミラーへ縮退)ので、以前のように (1-roughness) で重みを
	///      落として粗い面だけプリフィルタ済みIBLへ逃がす必要はない —
	///      トレース結果をそのまま信頼してよい。無効時はレンダラー側が a=0
	///      でクリアするので常に読んでよい。ここで読むのは
	///      structured_indices 側の生1sppではなく、GIと同じく
	///      constant_indices 側のデノイズ済み結果。
	Texture2D<float4> reflection_texture = ResourceDescriptorHeap[constant_indices.reflection_.radiance_srv_index_];
	float4 traced_reflection = reflection_texture.Load(int3(pixel, 0));

	ConstantBuffer<ReflectionRayConstantBuffer> reflection_tuning = ResourceDescriptorHeap[structured_indices.reflection_.ray_constant_index_];
	float reflection_weight = traced_reflection.a * saturate(reflection_tuning.strength_);

	/// [JP] レイトレGI(1バウンス拡散)。書かれているのは【入射放射輝度】で、
	///      受け側のアルベドは掛かっていない — コサイン重み付き半球サンプリングの
	///      pdf(cos/PI)が BRDF の albedo/PI と cos を約分するため。a=1 が有効。
	///      無効時はレンダラーが a=0 でクリアするので常に読んでよい。ここで
	///      読むのは structured_indices 側の生1sppではなく、
	///      GlobalIlluminationDenoiseCS.hlsl がビューごとに空間+時間デノイズ
	///      した後の constant_indices 側の結果。
	Texture2D<float4> global_illumination_texture = ResourceDescriptorHeap[constant_indices.global_illumination_.radiance_srv_index_];
	float4 traced_global_illumination = global_illumination_texture.Load(int3(pixel, 0));
	float global_illumination_weight = saturate(traced_global_illumination.a);

	float3 lighting;
	if (structured_indices.sky_.diffuse_irradiance_index_ != 0)
	{
		float3 ibl_diffuse = ImageBasedLightingRadianceLambertian(normal, view, roughness, diffuse_color, f0);
		float3 ibl_specular = ImageBasedLightingRadianceGgx(normal, view, roughness, f0);

		/// [JP] トレース反射で「プリフィルタ環境の代わりにシーンの実radiance」
		///      を使ったスペキュラを作り(single_scattering の式は
		///      ImageBasedLightingRadianceGgx と同一)、reflection_weight で
		///      IBLスペキュラと置き換える。
		if (reflection_weight > 0.0)
		{
			float normal_dot_view = clamp(dot(normal, view), 0.0, 1.0);
			float2 f_ab = SampleGgxLookupTable(clamp(float2(normal_dot_view, roughness), 0.0, 1.0)).rg;
			float3 fresnel_roughness = max(1.0 - roughness, f0) - f0;
			float3 specular_color = f0 + fresnel_roughness * pow(1.0 - normal_dot_view, 5.0);
			float3 single_scattering = specular_color * f_ab.x + f_ab.y;

			float3 traced_specular = traced_reflection.rgb * single_scattering;
			ibl_specular = lerp(ibl_specular, traced_specular, reflection_weight);
		}

		/// [JP] GI が有効なピクセルでは、空由来の拡散環境光を GI で【置き換える】。
		///      ibl_diffuse は「遮蔽を考えずに空の irradiance を与える近似」で、GI は
		///      同じものを実際にレイを飛ばして遮蔽込みで求めたもの。両方足すと空が
		///      二重計上になる(GI の miss は空を返すのだから当然)。
		lighting = (ibl_diffuse * (1.0 - global_illumination_weight) + ibl_specular) * structured_indices.sky_.intensity_;
	}
	else
	{
		lighting = diffuse_color * AMBIENT;

		/// [JP] スカイ(BRDF LUT)が無い場合の簡易フォールバック: F0 で重み付けた
		///      トレース反射をそのまま足す。
		lighting += traced_reflection.rgb * f0 * reflection_weight;
	}

	/// [JP] レイトレAO(AmbientOcclusionRT.hlsl + 時間積分済み)。AO は
	///      「環境光がどれだけ届くか」の近似なので環境光項だけに掛ける
	///      (直接光には掛けない — そちらの遮蔽はレイトレ影が担当)。
	///      power_ でコントラストを調整(1=そのまま)。無効時はレンダラー側が
	///      バッファを 1.0 でクリアするので、ここは常に読んでよい。
	Texture2D<float> ao_openness = ResourceDescriptorHeap[constant_indices.ambient_occlusion_.openness_srv_index_];
	ConstantBuffer<AmbientOcclusionRayConstantBuffer> ao_tuning = ResourceDescriptorHeap[structured_indices.ambient_occlusion_.ray_constant_index_];
	float ao = pow(saturate(ao_openness.Load(int3(pixel, 0))), max(ao_tuning.power_, 0.0001));
	lighting *= ao;

	/// [JP] GI はここで足す。AO を掛けないのは、GI のレイ自体が遮蔽そのものを
	///      計算しているから — AO を重ねると遮蔽の二重適用になって暗くなりすぎる
	///      (AO は「環境光がどれだけ届くか」の近似で、GI はその実測値)。
	///      sky_intensity_ も掛けない。GI シェーダ側で irradiance に既に掛けてある。
	lighting += traced_global_illumination.rgb * global_illumination_weight * diffuse_color;


	ConstantBuffer<LightConstantData> light_constant_buffer = ResourceDescriptorHeap[constant_indices.light_index_];

	if (light_constant_buffer.directional_intensity_ > 0.0)
	{
		float3 light_direction = normalize(-light_constant_buffer.directional_direction_);
		float3 directional_color = light_constant_buffer.directional_color_.rgb * light_constant_buffer.directional_intensity_ * directional_shadow_factor;
		lighting += EvalDirectLight(normal, view, light_direction, diffuse_color, f0, roughness, clearcoat_factor, clearcoat_roughness, sheen_color, sheen_roughness) * directional_color;

		/// [JP] レイトレ表面下散乱(SubsurfaceScatteringRT.hlsl が書いた透過率)。
		///      光に背いた面(N・L<0)で、裏から差し込む光が薄い部分ほど透けて
		///      見える項を足す。back(裏面性)で正面側では自然に0になる。
		///      無効時はレンダラー側がバッファを 0.0 でクリアするので常に
		///      読んでよい。
		Texture2D<float> sss_transmittance_texture = ResourceDescriptorHeap[structured_indices.subsurface_scattering_.transmittance_srv_index_];
		float sss_transmittance = sss_transmittance_texture.Load(int3(pixel, 0));

		if (sss_transmittance > 0.0)
		{
			ConstantBuffer<SubsurfaceScatteringRayConstantBuffer> sss_tuning = ResourceDescriptorHeap[structured_indices.subsurface_scattering_.ray_constant_index_];
			float back = saturate(dot(-normal, light_direction));
			float3 translucency = diffuse_color * sss_tuning.subsurface_color_ * light_constant_buffer.directional_color_.rgb * light_constant_buffer.directional_intensity_;
			lighting += translucency * sss_transmittance * back * sss_tuning.strength_;
		}
	}

	float4 view_position = mul(float4(world_position, 1.0), scene.view_);
	float linear_depth = view_position.z;

	uint3 cluster_count = uint3(light_constant_buffer.cluster_count_x_, light_constant_buffer.cluster_count_y_, CLUSTER_DEPTH_SLICES);
	uint tile_x = pixel.x / CLUSTER_TILE_SIZE;
	uint tile_y = pixel.y / CLUSTER_TILE_SIZE;
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

			float3 to_light = point_light.position - world_position;
			float distance = length(to_light);
			float3 light_direction = to_light / max(distance, 0.0001);
			float attenuation = AttenuateDistance(distance, point_light.range);

			float3 light_color = point_light.color.rgb * point_light.intensity * attenuation * punctual_shadow_factor;
			lighting += EvalDirectLight(normal, view, light_direction, diffuse_color, f0, roughness, clearcoat_factor, clearcoat_roughness, sheen_color, sheen_roughness) * light_color;
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

			float3 to_light = spot_light.position - world_position;
			float distance = length(to_light);
			float3 light_direction = to_light / max(distance, 0.0001);

			float cos_angle = dot(-light_direction, spot_light.direction);
			float spot_fade = saturate((cos_angle - spot_light.cos_half_angle) / max(spot_light.softness * (1.0 - spot_light.cos_half_angle), 0.0001));

			float attenuation = AttenuateDistance(distance, spot_light.range) * spot_fade;

			float3 light_color = spot_light.color.rgb * spot_light.intensity * attenuation * punctual_shadow_factor;
			lighting += EvalDirectLight(normal, view, light_direction, diffuse_color, f0, roughness, clearcoat_factor, clearcoat_roughness, sheen_color, sheen_roughness) * light_color;
		}
	}

	/// [JP] 矩形(エリア)ライト。Point/Spot と同じくクラスタで絞ってループする。
	///      面上の最近点を代表点として拡散/鏡面を評価する近似。物理的に正しくやるなら
	///      LTC(Linearly Transformed Cosines)、ソフトシャドウはレイトレ側で別途対応する。
	if (cluster.rect_count_ > 0)
	{
		StructuredBuffer<RectLightData> rect_lights = ResourceDescriptorHeap[light_constant_buffer.rect_light_index_];
		uint count = min(cluster.rect_count_, CLUSTER_MAX_RECT_LIGHTS);

		for (uint index = 0; index < count; index++)
		{
			uint light_index = light_list.Load((base + CLUSTER_MAX_POINT_LIGHTS + CLUSTER_MAX_SPOT_LIGHTS + index) * 4);
			RectLightData rect_light = rect_lights[light_index];

			/// [JP] 片面発光: パネルの正面(normal 側)にある面だけを照らす。
			if (dot(world_position - rect_light.position, rect_light.normal) <= 0.0)
			{
				continue;
			}

			float3 representative_point = ClosestPointOnRect(world_position, rect_light.position, rect_light.right, rect_light.up, rect_light.half_width, rect_light.half_height);
			float3 to_light = representative_point - world_position;
			float distance = length(to_light);
			float3 light_direction = to_light / max(distance, 0.0001);

			float attenuation = AttenuateDistance(distance, rect_light.range);
			float3 light_color = rect_light.color.rgb * rect_light.intensity * attenuation * punctual_shadow_factor;
			lighting += EvalDirectLight(normal, view, light_direction, diffuse_color, f0, roughness, clearcoat_factor, clearcoat_roughness, sheen_color, sheen_roughness) * light_color;
		}
	}

	/// [JP] 雷の閃光: シーン全体を一瞬明るく染める簡易表現(実際の稲妻ジオメトリは
	///      描かない)。
	lighting += weather_light.thunder_flash_ * float3(0.85, 0.9, 1.0) * 2.5;

	/// [JP] 最後にフォグ/体積光を合成(このピクセルのビュー空間Zのスライスで
	///      サンプル)。ゴッドレイ(fog.rgb)もここで乗る。
	float3 final_color = ApplyVolumetricFog(lighting + emissive, input.texcoord, linear_depth);

	/// [JP] 雷の閃光はこの面にも独立して効く。
	final_color += LightningBoltMask(input.texcoord, weather_light.thunder_seed_, saturate(weather_light.thunder_flash_ - 0.5) * 2.0);

	return float4(final_color, 1.0);
}
