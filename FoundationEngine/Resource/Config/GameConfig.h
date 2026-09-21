#pragma once
#include <FoundationEngine/Prelude.h>
#include <GraphicsEngine/Quality/Upscale.h>
#include <GraphicsEngine/D3D12/SwapChain/GraphicsResolution.h>

namespace SeedCore
{
	struct SEEDCORE_API GameConfig
	{
		Uint32 windowWidth_ = 1920;

		Uint32 windowHeight_ = 1080;

		Bool fullscreen_ = false;

		ResolutionPreset resolution_ = ResolutionPreset::FHD;

		Bool useDlss_ = false;

		UpscaleMode upscaleMode_ = UpscaleMode::Balanced;

		Bool useFrameGeneration_ = false;

		Bool vsync_ = false;

		Bool useReflex_ = false;

		Bool useDeepDVC_ = false;

		String initialScenePath_;

		/// [EN] Whether the startup splash shows the warning screen (Warning.sub.logo) before the logos.
		/// [JP] 起動スプラッシュで、ロゴの前に警告画面（Warning.sub.logo）を表示するか。
		Bool showSplashWarning_ = true;

		/// [EN] Whether the startup splash shows the fiction disclaimer screen (Fiction.sub.logo) before the logos.
		/// [JP] 起動スプラッシュで、ロゴの前にフィクション表記画面（Fiction.sub.logo）を表示するか。
		Bool showSplashFiction_ = true;

		/// [EN] File name (without ".exe") given to the launcher executable of an exported package, the one players start. Any language may be used; empty means "Launcher".
		/// [JP] 書き出したパッケージで、プレイヤーが起動するランチャー実行ファイルに付けるファイル名（".exe" なし）。言語は問わない。空なら "Launcher"。
		String executableName_;

		void Load(const std::filesystem::path& path = "../UserProject/Assets/Config/GameConfig.scg");

		void Save(const std::filesystem::path& path = "../UserProject/Assets/Config/GameConfig.scg")const;
	};
}
