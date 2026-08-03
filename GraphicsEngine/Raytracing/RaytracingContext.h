#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/Utility/SerializeFallback.h>
#include <GraphicsEngine/DLSS/DlssMode.h>
#include <GraphicsEngine/Renderer/ShadowRenderer.h>
#include <GraphicsEngine/Renderer/AmbientOcclusionRenderer.h>
#include <GraphicsEngine/Renderer/SubsurfaceScatteringRenderer.h>
#include <GraphicsEngine/Renderer/ReflectionRenderer.h>
#include <GraphicsEngine/Renderer/GlobalIlluminationRenderer.h>
#include <GraphicsEngine/Renderer/VolumetricCloudScapesRenderer.h>
#include <GraphicsEngine/Renderer/VolumetricLightRenderer.h>
#include <GraphicsEngine/Renderer/VolumetricStarRenderer.h>
#include <GraphicsEngine/Renderer/WeatherParticleRenderer.h>
#include <GraphicsEngine/System/CelestialSystem.h>

namespace SeedCore
{
	/// [EN] Groups every raytraced-effect setting. shadow_ and
	///      ambientOcclusion_ are real, implemented tuning structs (see
	///      Raytracing/Shadow/ShadowRT.hlsl and Raytracing/AmbientOcclusion/
	///      AmbientOcclusionRT.hlsl). The rest (GlobalIllumination/Reflection/
	///      Refraction/VolumetricLight/VolumetricCloudScapes/
	///      SubsurfaceScattering — see each folder's *RT.hlsl, all still TODO
	///      scaffold) don't have a tuning struct yet since their design isn't
	///      settled; only a plain enabled_ flag exists for each as a
	///      placeholder, with nothing behind it on the Renderer side yet.
	///      Editor's EditorContext holds one of these and edits it from the
	///      グラフィックス→レイトレーシング menu; Graphics::
	///      SetRaytracingSettings threads it through to Renderer/
	///      RaytracingRenderer as a single call.
	/// [JP] レイトレーシング系エフェクトの設定をまとめる。shadow_ と
	///      ambientOcclusion_ は実装済みの実チューニング構造体
	///      (Raytracing/Shadow/ShadowRT.hlsl と Raytracing/AmbientOcclusion/
	///      AmbientOcclusionRT.hlsl 参照)。残り(GlobalIllumination/Reflection/
	///      Refraction/VolumetricLight/VolumetricCloudScapes/
	///      SubsurfaceScattering — 各フォルダの *RT.hlsl 参照、全て TODO
	///      scaffold)は設計がまだ固まっていないためチューニング構造体を持たず、
	///      enabled_ フラグだけを器として用意してある(Renderer側はまだ何も
	///      見ていない)。Editor 側の EditorContext がこれを1つ保持して
	///      メニュー(グラフィックス→レイトレーシング)から編集し、
	///      Graphics::SetRaytracingSettings 経由で Renderer/RaytracingRenderer
	///      まで1本で渡す。
	struct RaytracingContext
	{
		Bool shadowEnabled_ = true;
		ShadowRayConstantBuffer shadow_;

		Bool ambientOcclusionEnabled_ = false;
		AmbientOcclusionRayConstantBuffer ambientOcclusion_;

		Bool reflectionEnabled_ = false;
		ReflectionRayConstantBuffer reflection_;

		Bool globalIlluminationEnabled_ = false;
		GlobalIlluminationRayConstantBuffer globalIllumination_;

		Bool subsurfaceScatteringEnabled_ = false;
		SubsurfaceScatteringRayConstantBuffer subsurfaceScattering_;

		Bool volumetricCloudScapesEnabled_ = false;
		VolumetricCloudScapesRayConstantBuffer volumetricCloudScapes_;

		Bool volumetricLightEnabled_ = false;
		VolumetricLightRayConstantBuffer volumetricLight_;

		Bool daySystemEnabled_ = false;
		DaySystemConstantBuffer daySystem_;

		Bool sunLightEnabled_ = false;
		SunLightSettings sunLight_;

		Bool moonLightEnabled_ = false;
		MoonLightSettings moonLight_;

		Bool volumetricStarEnabled_ = false;
		VolumetricStarRayConstantBuffer volumetricStar_;

		Bool rainEnabled_ = false;
		RainSettings rain_;

		Bool snowEnabled_ = false;
		SnowSettings snow_;

		Bool refractionEnabled_ = false;

		Bool causticsEnabled_ = false;

		/// [EN] Switches the final composited frame's denoise+upscale path from
		///      the per-effect custom compute denoisers (Shadow/AO/GI's own
		///      spatio-temporal accumulation, each output at native 1280x720)
		///      to NVIDIA DLSS Ray Reconstruction, which denoises the raw noisy
		///      composited color as a whole and upscales it to 3840x2160 in one
		///      pass. When true, Shadow/AO/GI each skip their own denoise
		///      dispatch and expose their raw signal instead (double-denoising
		///      would fight DLSS-RR's own denoiser and over-smooth).
		/// [JP] 最終合成フレームのデノイズ+アップスケール経路を、エフェクトごとの
		///      自前コンピュートデノイザ(Shadow/AO/GIそれぞれの空間+時間蓄積、
		///      1280x720ネイティブ出力)から NVIDIA DLSS Ray Reconstruction へ
		///      切り替える。DLSS-RRは生のノイズ入り合成カラー全体を1パスで
		///      デノイズ+3840x2160へアップスケールする。true の間は Shadow/AO/GI
		///      それぞれが自前デノイズのディスパッチを止め、生信号をそのまま
		///      露出する(二重デノイズはDLSS-RR自身のデノイザと衝突し過剰な
		///      ぼけを生むため)。
		Bool dlssRayReconstructionEnabled_ = false;

		/// [EN] DLSS Ray Reconstruction's quality/performance mode, applied to
		///      slDLSSDSetOptions each frame it runs (see
		///      DlssManager::EvaluateRayReconstruction). Independent of
		///      dlssRayReconstructionEnabled_, which only turns the pass on/off.
		/// [JP] DLSS Ray Reconstruction の画質/性能モード。実行される毎フレーム
		///      slDLSSDSetOptions に適用される(DlssManager::
		///      EvaluateRayReconstruction 参照)。パスのON/OFFを切り替える
		///      dlssRayReconstructionEnabled_ とは独立。
		DlssMode dlssMode_ = DlssMode::Balanced;

		/// [EN] Each field is loaded/saved independently via TryLoadField (see
		///      FoundationEngine/Utility/SerializeFallback.h) - a missing or
		///      unparsable field (older save, schema change) only falls back to
		///      that field's default; every other field still loads normally.
		/// [JP] 各フィールドは TryLoadField(FoundationEngine/Utility/
		///      SerializeFallback.h 参照)経由で独立に読み書きする -
		///      見つからない/パースできないフィールド(古い保存データ、
		///      スキーマ変更)があってもそのフィールドだけ既定値へフォール
		///      バックし、他のフィールドは通常通り読み込まれる。
		template<class Archive>
		void serialize(Archive& archive)
		{
			Int32 dlssModeValue = static_cast<Int32>(dlssMode_);

			TryLoadField(archive, "shadowEnabled", shadowEnabled_);
			TryLoadField(archive, "shadow", shadow_);
			TryLoadField(archive, "ambientOcclusionEnabled", ambientOcclusionEnabled_);
			TryLoadField(archive, "ambientOcclusion", ambientOcclusion_);
			TryLoadField(archive, "reflectionEnabled", reflectionEnabled_);
			TryLoadField(archive, "reflection", reflection_);
			TryLoadField(archive, "globalIlluminationEnabled", globalIlluminationEnabled_);
			TryLoadField(archive, "globalIllumination", globalIllumination_);
			TryLoadField(archive, "subsurfaceScatteringEnabled", subsurfaceScatteringEnabled_);
			TryLoadField(archive, "subsurfaceScattering", subsurfaceScattering_);
			TryLoadField(archive, "volumetricCloudScapesEnabled", volumetricCloudScapesEnabled_);
			TryLoadField(archive, "volumetricCloudScapes", volumetricCloudScapes_);
			TryLoadField(archive, "volumetricLightEnabled", volumetricLightEnabled_);
			TryLoadField(archive, "volumetricLight", volumetricLight_);
			TryLoadField(archive, "daySystemEnabled", daySystemEnabled_);
			TryLoadField(archive, "daySystem", daySystem_);
			TryLoadField(archive, "sunLightEnabled", sunLightEnabled_);
			TryLoadField(archive, "sunLight", sunLight_);
			TryLoadField(archive, "moonLightEnabled", moonLightEnabled_);
			TryLoadField(archive, "moonLight", moonLight_);
			TryLoadField(archive, "volumetricStarEnabled", volumetricStarEnabled_);
			TryLoadField(archive, "volumetricStar", volumetricStar_);
			TryLoadField(archive, "rainEnabled", rainEnabled_);
			TryLoadField(archive, "rain", rain_);
			TryLoadField(archive, "snowEnabled", snowEnabled_);
			TryLoadField(archive, "snow", snow_);
			TryLoadField(archive, "refractionEnabled", refractionEnabled_);
			TryLoadField(archive, "dlssRayReconstructionEnabled", dlssRayReconstructionEnabled_);
			TryLoadField(archive, "dlssMode", dlssModeValue);

			dlssMode_ = static_cast<DlssMode>(dlssModeValue);
		}
	};

	/// [EN] Encodes settings as a JSON string, for embedding into Scene::raytracingSettingsJson_.
	/// [JP] settings を JSON 文字列へ変換する。Scene::raytracingSettingsJson_ に埋め込むために使う。
	inline String SerializeRaytracingContext(const RaytracingContext& settings)
	{
		std::ostringstream stream;
		{
			cereal::JSONOutputArchive archive(stream);
			archive(cereal::make_nvp("raytracing", settings));
		}

		return stream.str().c_str();
	}

	/// [EN] Decodes a JSON string produced by SerializeRaytracingContext back into a RaytracingContext. Returns a default-constructed RaytracingContext if json is empty or malformed.
	/// [JP] SerializeRaytracingContext が生成した JSON 文字列を RaytracingContext へ復元する。json が空または不正な場合はデフォルト構築の RaytracingContext を返す。
	inline RaytracingContext DeserializeRaytracingContext(const String& json)
	{
		RaytracingContext settings;

		if (json.view().empty())
		{
			return settings;
		}

		try
		{
			std::istringstream stream(json.c_str());
			cereal::JSONInputArchive archive(stream);
			archive(cereal::make_nvp("raytracing", settings));
		}
		catch (const cereal::Exception&)
		{
			return RaytracingContext();
		}

		return settings;
	}
}
