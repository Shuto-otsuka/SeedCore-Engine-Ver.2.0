#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/Serialization/Json/JsonArchive.h>
#include <GraphicsEngine/Quality/Upscale.h>
#include <GraphicsEngine/Renderer/ShadowRenderer.h>
#include <GraphicsEngine/Renderer/AmbientOcclusionRenderer.h>
#include <GraphicsEngine/Renderer/SubsurfaceScatteringRenderer.h>
#include <GraphicsEngine/Renderer/ReflectionRenderer.h>
#include <GraphicsEngine/Renderer/RefractionRenderer.h>
#include <GraphicsEngine/Renderer/GlobalIlluminationRenderer.h>
#include <GraphicsEngine/Renderer/VolumetricCloudScapesRenderer.h>
#include <GraphicsEngine/Renderer/VolumetricLightRenderer.h>
#include <GraphicsEngine/Renderer/VolumetricStarRenderer.h>
#include <GraphicsEngine/Renderer/WeatherParticleRenderer.h>
#include <GraphicsEngine/System/CelestialSystem.h>

namespace SeedCore
{
	/// [EN] Groups every raytraced-effect setting. shadow_/ambientOcclusion_/
	///      reflection_/refraction_ are real, implemented tuning structs (see
	///      each folder's *RT.hlsl). The rest (GlobalIllumination/
	///      VolumetricLight/VolumetricCloudScapes/SubsurfaceScattering/
	///      Caustics — see each folder's *RT.hlsl, most still TODO scaffold)
	///      don't have a tuning struct yet since their design isn't settled;
	///      only a plain enabled_ flag exists for each as a placeholder, with
	///      nothing behind it on the Renderer side yet. Editor's EditorContext
	///      holds one of these and edits it from the グラフィックス→
	///      レイトレーシング menu; Graphics::Raytracing threads it
	///      through to Renderer/RaytracingRenderer as a single call.
	/// [JP] レイトレーシング系エフェクトの設定をまとめる。shadow_/
	///      ambientOcclusion_/reflection_/refraction_ は実装済みの実
	///      チューニング構造体(各フォルダの *RT.hlsl 参照)。残り
	///      (GlobalIllumination/VolumetricLight/VolumetricCloudScapes/
	///      SubsurfaceScattering/Caustics — 各フォルダの *RT.hlsl 参照、
	///      多くはまだ TODO scaffold)は設計がまだ固まっていないため
	///      チューニング構造体を持たず、enabled_ フラグだけを器として
	///      用意してある(Renderer側はまだ何も見ていない)。Editor 側の
	///      EditorContext がこれを1つ保持してメニュー(グラフィックス→
	///      レイトレーシング)から編集し、Graphics::Raytracing
	///      経由で Renderer/RaytracingRenderer まで1本で渡す。
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

		Bool refractionEnabled_ = false;
		RefractionRayConstantBuffer refraction_;

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

		/// [EN] Shared quality/performance mode for whichever upscale path is
		///      active - DLSS Ray Reconstruction (applied to slDLSSDSetOptions
		///      each frame it runs, see DlssManager::EvaluateRayReconstruction)
		///      when dlssRayReconstructionEnabled_ is true, or TAAU's render
		///      scale (see UpscaleRenderScale) when it's false. Independent of
		///      dlssRayReconstructionEnabled_, which only chooses which of the
		///      two paths consumes this mode.
		/// [JP] 有効なアップスケール経路が使う共有の画質/性能モード -
		///      dlssRayReconstructionEnabled_ が true なら DLSS Ray
		///      Reconstruction(実行される毎フレーム slDLSSDSetOptions に適用、
		///      DlssManager::EvaluateRayReconstruction 参照)、false なら TAAU の
		///      レンダースケール(UpscaleRenderScale 参照)に使われる。
		///      dlssRayReconstructionEnabled_ とは独立 - どちらの経路がこの
		///      モードを消費するかを切り替えるだけ。
		UpscaleMode upscaleMode_ = UpscaleMode::Balanced;

		/// [EN] Each field is loaded/saved independently via TryField - a
		///      missing or unparsable field (older save, schema change) only
		///      falls back to that field's default; every other field still
		///      loads normally.
		/// [JP] 各フィールドは TryField 経由で独立に読み書きする -
		///      見つからない/パースできないフィールド(古い保存データ、
		///      スキーマ変更)があってもそのフィールドだけ既定値へフォール
		///      バックし、他のフィールドは通常通り読み込まれる。
		template<class Archive>
		void Serialize(Archive& archive)
		{
			Int32 upscaleModeValue = static_cast<Int32>(upscaleMode_);

			archive.TryField("shadowEnabled", shadowEnabled_);
			archive.TryField("shadow", shadow_);
			archive.TryField("ambientOcclusionEnabled", ambientOcclusionEnabled_);
			archive.TryField("ambientOcclusion", ambientOcclusion_);
			archive.TryField("reflectionEnabled", reflectionEnabled_);
			archive.TryField("reflection", reflection_);
			archive.TryField("globalIlluminationEnabled", globalIlluminationEnabled_);
			archive.TryField("globalIllumination", globalIllumination_);
			archive.TryField("subsurfaceScatteringEnabled", subsurfaceScatteringEnabled_);
			archive.TryField("subsurfaceScattering", subsurfaceScattering_);
			archive.TryField("volumetricCloudScapesEnabled", volumetricCloudScapesEnabled_);
			archive.TryField("volumetricCloudScapes", volumetricCloudScapes_);
			archive.TryField("volumetricLightEnabled", volumetricLightEnabled_);
			archive.TryField("volumetricLight", volumetricLight_);
			archive.TryField("daySystemEnabled", daySystemEnabled_);
			archive.TryField("daySystem", daySystem_);
			archive.TryField("sunLightEnabled", sunLightEnabled_);
			archive.TryField("sunLight", sunLight_);
			archive.TryField("moonLightEnabled", moonLightEnabled_);
			archive.TryField("moonLight", moonLight_);
			archive.TryField("volumetricStarEnabled", volumetricStarEnabled_);
			archive.TryField("volumetricStar", volumetricStar_);
			archive.TryField("refractionEnabled", refractionEnabled_);
			archive.TryField("refraction", refraction_);
			archive.TryField("dlssRayReconstructionEnabled", dlssRayReconstructionEnabled_);
			archive.TryField("upscaleMode", upscaleModeValue);

			upscaleMode_ = static_cast<UpscaleMode>(upscaleModeValue);
		}
	};

	/// [EN] Encodes settings as a JSON string, for embedding into Scene::raytracingSettingsJson_.
	/// [JP] settings を JSON 文字列へ変換する。Scene::raytracingSettingsJson_ に埋め込むために使う。
	inline String SerializeRaytracingContext(const RaytracingContext& settings)
	{
		JsonOutputArchive archive;
		archive.Field("raytracing", settings);
		return archive.Dump();
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

		JsonInputArchive archive;
		if (!archive.Parse(json))
		{
			return RaytracingContext();
		}

		archive.TryField("raytracing", settings);

		return settings;
	}
}
