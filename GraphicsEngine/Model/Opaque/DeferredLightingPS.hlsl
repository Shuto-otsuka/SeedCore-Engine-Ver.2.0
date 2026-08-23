#include "../Model.hlsli"
#include "../../Shader/Constants.hlsli"
#include "../../Shader/Structured.hlsli"
#include "../../Shader/Normal.hlsli"
#include "../../Shader/Light.hlsli"
#include "../../Shader/Material.hlsli"
#include "../../Light/Cluster.hlsli"
#include "../../Light/ImageBasedLighting.hlsli"
#include "../../Raytracing/Shadow/Shadow.hlsli"
#include "../../Raytracing/AmbientOcclusion/AmbientOcclusion.hlsli"
#include "../../Raytracing/SubsurfaceScattering/SubsurfaceScattering.hlsli"
#include "../../Raytracing/Reflection/Reflection.hlsli"
#include "../../Raytracing/Refraction/Refraction.hlsli"
#include "../../Raytracing/VolumetricCloudScapes/VolumetricCloudScapes.hlsli"
#include "../../Raytracing/Froxel/Froxel.hlsli"
#include "../../Raytracing/VolumetricLight/VolumetricLight.hlsli"
#include "../../Shader/Noise.hlsli"
#include "../../Shader/Precipitation.hlsli"

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

	Texture2D<uint4> gbuffer4 = ResourceDescriptorHeap[structured_indices.gbuffer_.index_4_];
	uint4 visibility_id = gbuffer4.Load(int3(pixel, 0));
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

		/// [JP] 以下はレイトレ信号のバッファ可視化。生
		///      (structured_indices 側・全ビュー共有の1sppトレース結果)と
		///      デノイズ後(constant_indices 側・ビューごとのチェーン出力)を
		///      それぞれそのまま出す。加工しないのは、どちらの段で信号が
		///      失われたかを判定するのが目的だから — 見やすさのための補正を
		///      入れると、その補正自体が判定を曇らせる。
		case 10: // 反射（生）
		{
			Texture2D<float4> reflection_raw = ResourceDescriptorHeap[structured_indices.reflection_.output_srv_index_];
			return float4(reflection_raw.Load(int3(pixel, 0)).rgb, 1.0);
		}
		case 11: // 反射（デノイズ後）
		{
			Texture2D<float4> reflection_denoised = ResourceDescriptorHeap[constant_indices.reflection_.radiance_srv_index_];
			return float4(reflection_denoised.Load(int3(pixel, 0)).rgb, 1.0);
		}
		case 12: // グローバルイルミネーション（生）
		{
			Texture2D<float4> global_illumination_raw = ResourceDescriptorHeap[structured_indices.global_illumination_.output_srv_index_];
			return float4(global_illumination_raw.Load(int3(pixel, 0)).rgb, 1.0);
		}
		case 13: // グローバルイルミネーション（デノイズ後）
		{
			Texture2D<float4> global_illumination_denoised = ResourceDescriptorHeap[constant_indices.global_illumination_.radiance_srv_index_];
			return float4(global_illumination_denoised.Load(int3(pixel, 0)).rgb, 1.0);
		}

		/// [JP] シャドウの生信号は r=ディレクショナル可視性、gba=パンクチュアル
		///      放射輝度なので、そのまま出す(r=ディレクショナル、
		///      gb=パンクチュアル放射輝度の頭2チャンネル)。デノイズ後は形が
		///      違う2枚(directional_denoised_/punctual_denoised_)に分かれて
		///      いるので、rにディレクショナル可視性、gbにパンクチュアル放射輝度の
		///      頭2チャンネルを合成して同じ見え方にする。
		case 14: // シャドウ（生）
		{
			Texture2D<float4> shadow_raw = ResourceDescriptorHeap[structured_indices.shadow_.raw_visibility_srv_index_];
			float4 raw_value = shadow_raw.Load(int3(pixel, 0));
			return float4(raw_value.x, raw_value.y, raw_value.z, 1.0);
		}
		case 15: // シャドウ（デノイズ後）
		{
			Texture2D<float> directional_denoised = ResourceDescriptorHeap[constant_indices.shadow_.directional_visibility_srv_index_];
			Texture2D<float4> punctual_denoised = ResourceDescriptorHeap[constant_indices.shadow_.punctual_radiance_srv_index_];
			float directional_value = directional_denoised.Load(int3(pixel, 0));
			float3 punctual_value = punctual_denoised.Load(int3(pixel, 0)).rgb;
			return float4(directional_value, punctual_value.r, punctual_value.g, 1.0);
		}
		case 16: // アンビエントオクルージョン（生）
		{
			Texture2D<float> ambient_occlusion_raw = ResourceDescriptorHeap[structured_indices.ambient_occlusion_.raw_srv_index_];
			return float4(ambient_occlusion_raw.Load(int3(pixel, 0)).xxx, 1.0);
		}
		case 17: // アンビエントオクルージョン（デノイズ後）
		{
			Texture2D<float> ambient_occlusion_denoised = ResourceDescriptorHeap[constant_indices.ambient_occlusion_.openness_srv_index_];
			return float4(ambient_occlusion_denoised.Load(int3(pixel, 0)).xxx, 1.0);
		}
	}

	/// [JP] Unlit: ライティングを一切せず base_color + emissive のみ
	///      (KHR_materials_unlit、またはマテリアルで明示的に選択されたUnlit)。
	if (material_instance.shading_model_ == SHADING_MODEL_UNLIT)
	{
		return float4(base_color + emissive, 1.0);
	}

	float3 world_position = ReconstructWorldPosition(input.texcoord, depth);

	SceneConstantBuffer scene = GetSceneConstantBuffer();
	float3 view = normalize(scene.camera_position_.xyz - world_position);

	/// [JP] 天候補正+KHR拡張のマテリアル解決は Shader/Material.hlsli の
	///      ResolveGBufferMaterial に共通化した(Raytracing/Shadow/ShadowRT.hlsl
	///      の ReSTIR DI からも同じ関数を呼び、鏡面ハイライトが主要視点と
	///      一致するようにするため)。モデル UV は VisibilityBuffer(RT4.zw)
	///      から復元する — このパスはフルスクリーン矩形なので input.texcoord
	///      はスクリーン UV であってモデル UV ではない。
	float2 material_texcoord = UnpackVisibilityTexcoord(visibility_id);
	GBufferMaterial gbuffer_material = ResolveGBufferMaterial(base_color, metallic, roughness, normal, view, material_instance, material_texcoord, rt1.a);
	float3 diffuse_color = gbuffer_material.diffuse_color_;
	float3 f0 = gbuffer_material.f0_;
	roughness = gbuffer_material.roughness_;
	float clearcoat_factor = gbuffer_material.clearcoat_factor_;
	float clearcoat_roughness = gbuffer_material.clearcoat_roughness_;
	float3 clearcoat_normal = gbuffer_material.clearcoat_normal_;
	float3 sheen_color = gbuffer_material.sheen_color_;
	float sheen_roughness = gbuffer_material.sheen_roughness_;
	float3 anisotropy_tangent = gbuffer_material.anisotropy_tangent_;
	float3 anisotropy_bitangent = gbuffer_material.anisotropy_bitangent_;
	float anisotropy_strength = gbuffer_material.anisotropy_strength_;

	/// [JP] ShadowRT.hlsl(+ShadowDenoiseCS.hlsl で時間+空間積分済み)が書いた
	///      ディレクショナル可視性。ビューごとの蓄積チェーンなので
	///      constant_indices 側から取る。shadow_strength_ で効きの強さを
	///      0(常に照射扱い)〜1(可視性をそのまま適用)で調整する。
	///      ディレクショナルの項【だけ】に掛ける。環境光(IBL)には掛けない —
	///      太陽から見えない面(壁の裏・屋内)は可視性0になるため、環境光まで
	///      消すと真っ黒に潰れる。影の暗さの下限は環境光が決める(物理的にも
	///      それが正しい)。もっと暗い影にしたければ環境光/スカイ強度を下げるか
	///      太陽強度を上げる。パンクチュアル側は既にBRDF評価済みのRGB放射輝度
	///      なので同じ掛け方はできない — shadow_strength_ の適用は
	///      ShadowRT.hlsl 側で完結させている(下の punctual_direct_lighting 参照)。
	Texture2D<float> directional_visibility_texture = ResourceDescriptorHeap[constant_indices.shadow_.directional_visibility_srv_index_];
	float directional_visibility = directional_visibility_texture.Load(int3(pixel, 0));

	ConstantBuffer<ShadowRayConstantBuffer> shadow_tuning = ResourceDescriptorHeap[structured_indices.shadow_.ray_constant_index_];
	float directional_shadow_factor = lerp(1.0, directional_visibility, saturate(shadow_tuning.shadow_strength_));

	/// [EN] Ambient term: image-based lighting when a skymap is bound
	///      (irradiance index != 0), otherwise the flat fallback ambient.
	/// [JP] 環境光項: スカイマップがバインドされていれば（irradiance インデックス
	///      != 0）IBL、なければ従来のフラットな環境光にフォールバック。
	/// [JP] レイトレ反射(ReflectionDenoiseCS.hlsl がビューごとに SVGF で
	///      デノイズした後の放射輝度)。GGX重点サンプリング
	///      (ReflectionRT.hlsl)は roughness に応じて自然にボケる(roughness 0
	///      は厳密ミラーへ縮退)ので、以前のように (1-roughness) で重みを
	///      落として粗い面だけプリフィルタ済みIBLへ逃がす必要はない —
	///      トレース結果をそのまま信頼してよい。ここで読むのは
	///      structured_indices 側の生1sppではなく、GI/AO/Shadowと同じく
	///      constant_indices 側のデノイズ済み結果(最後のA-Trousパスの出力)。
	///
	///      アルファは GI/AO と同じ単純な有効フラグ(1=有効、0=背景・機能無効)。
	///      背景・機能無効時はシェーダ側とレンダラー側の双方が 0 を書くので、
	///      a > 0 がそのまま有効判定になる。
	Texture2D<float4> reflection_texture = ResourceDescriptorHeap[constant_indices.reflection_.radiance_srv_index_];
	float4 traced_reflection = reflection_texture.Load(int3(pixel, 0));

	ConstantBuffer<ReflectionRayConstantBuffer> reflection_tuning = ResourceDescriptorHeap[structured_indices.reflection_.ray_constant_index_];
	float reflection_weight = (traced_reflection.a > 0.0 ? 1.0 : 0.0) * saturate(reflection_tuning.strength_);

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

	/// [JP] レイトレ屈折(RefractionRT.hlsl、a=1 有効)。KHR_materials_transmission
	///      が無いピクセルは常に a=0 なので、ここは自然にスキップされる。
	///      diffuse_color は既に (1-transmission_factor_) 分減衰済み(上の
	///      dielectric_f0/diffuse_color 算出ブロック参照)なので、ここでは
	///      その分を屈折放射輝度で埋め戻す形で加算する。Fresnel透過率
	///      (1-反射率)ぶんだけ通す — グレージング角ほど反射が支配的になり
	///      屈折は減る。
	Texture2D<float4> refraction_texture = ResourceDescriptorHeap[structured_indices.refraction_.output_srv_index_];
	float4 traced_refraction = refraction_texture.Load(int3(pixel, 0));
	ConstantBuffer<RefractionRayConstantBuffer> refraction_tuning = ResourceDescriptorHeap[structured_indices.refraction_.ray_constant_index_];
	float refraction_weight = traced_refraction.a * saturate(refraction_tuning.strength_) * saturate(material_instance.transmission_factor_);
	if (refraction_weight > 0.0)
	{
		float normal_dot_view_for_refraction = clamp(dot(normal, view), 0.0, 1.0);
		float3 fresnel_reflectance = f0 + (max(1.0 - roughness, f0) - f0) * pow(1.0 - normal_dot_view_for_refraction, 5.0);
		lighting += traced_refraction.rgb * (1.0 - fresnel_reflectance) * refraction_weight;
	}

	/// [JP] レイトレAO(AmbientOcclusionRT.hlsl + 時間積分済み)。AO は
	///      「環境光がどれだけ届くか」の近似なので環境光項だけに掛ける
	///      (直接光には掛けない — そちらの遮蔽はレイトレ影が担当)。
	///      power_ でコントラストを調整(1=そのまま)。無効時はレンダラー側が
	///      バッファを 1.0 でクリアするので、ここは常に読んでよい。
	Texture2D<float> ao_openness = ResourceDescriptorHeap[constant_indices.ambient_occlusion_.openness_srv_index_];
	ConstantBuffer<AmbientOcclusionRayConstantBuffer> ao_tuning = ResourceDescriptorHeap[structured_indices.ambient_occlusion_.ray_constant_index_];
	float ao = pow(saturate(ao_openness.Load(int3(pixel, 0))), max(ao_tuning.power_, 0.0001));

	/// [JP] glTF コアの occlusionTexture(.r)。レイトレAOと同じ「環境光がどれだけ
	///      届くか」を表すので同じ環境光項へ掛ける。両者は別物 — こちらは
	///      アーティストが焼き込んだ静的な遮蔽、AO は実行時に計算する動的な遮蔽。
	if (material_instance.occlusion_texture_index_ != 0xFFFFFFFF)
	{
		Texture2D<float4> occlusion_texture = ResourceDescriptorHeap[material_instance.occlusion_texture_index_];
		ao *= occlusion_texture.SampleLevel(sampler_aniso_wrap, material_texcoord, 0).r;
	}

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
		lighting += EvalDirectLightDispatch(material_instance.shading_model_, normal, view, light_direction, diffuse_color, f0, roughness, clearcoat_factor, clearcoat_roughness, clearcoat_normal, sheen_color, sheen_roughness, anisotropy_tangent, anisotropy_bitangent, anisotropy_strength) * directional_color;

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

	/// [JP] Point/Spot/Rect の直接光は、決定論的なクラスタループではなく
	///      Raytracing/Shadow/ShadowRT.hlsl の ReSTIR DI(このピクセルのクラスタで
	///      重み付きリザーバーサンプリングにより毎フレーム1灯だけ確率的に選び、
	///      ResolveGBufferMaterial + EvalDirectLightDispatch でフルBRDF評価した
	///      RGB放射輝度を選択pdfで割って影レイの可視性を掛けたもの)を
	///      ShadowDenoiseCS.hlsl が時間+空間積分した結果を直接加算する。
	///      光ごとに個別の影/鏡面ハイライトが立つため、複数の Point/Spot/Rect が
	///      同一ピクセルへ及ぶ場面で「全灯の可視性を1つの確率的スカラーへ平均化
	///      して共有する」という以前の破綻(ある灯が遮蔽されていても他の灯と
	///      混ざって中途半端に暗くなる)が起きない。
	Texture2D<float4> punctual_direct_lighting_texture = ResourceDescriptorHeap[constant_indices.shadow_.punctual_radiance_srv_index_];
	lighting += punctual_direct_lighting_texture.Load(int3(pixel, 0)).rgb;

	float4 view_position = mul(float4(world_position, 1.0), scene.view_);
	float linear_depth = view_position.z;

	/// [JP] 雷の閃光: シーン全体を一瞬明るく染める簡易表現(実際の稲妻ジオメトリは
	///      描かない)。
	lighting += light_constant_buffer.thunder_flash_ * float3(0.85, 0.9, 1.0) * 2.5;

	/// [JP] 最後にフォグ/体積光を合成(このピクセルのビュー空間Zのスライスで
	///      サンプル)。ゴッドレイ(fog.rgb)もここで乗る。
	float3 final_color = ApplyVolumetricFog(lighting + emissive, input.texcoord, linear_depth);

	/// [JP] 雷の閃光はこの面にも独立して効く。
	final_color += LightningBoltMask(input.texcoord, light_constant_buffer.thunder_seed_, saturate(light_constant_buffer.thunder_flash_ - 0.5) * 2.0);

	return float4(final_color, 1.0);
}
