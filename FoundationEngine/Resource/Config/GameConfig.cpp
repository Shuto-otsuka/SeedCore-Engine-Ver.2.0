#include <FoundationEngine/Resource/Config/GameConfig.h>
#include <FoundationEngine/Serialization/Binary/BinaryArchive.h>

namespace SeedCore
{
	void GameConfig::Load(const std::filesystem::path& path)
	{
		BinaryInputArchive archive;
		if (!archive.Read(String(path.string())))
		{
			return;
		}

		Int32 upscaleModeValue = static_cast<Int32>(upscaleMode_);
		Int32 resolutionValue = static_cast<Int32>(resolution_);

		archive.TryField("windowWidth", windowWidth_);
		archive.TryField("windowHeight", windowHeight_);
		archive.TryField("fullscreen", fullscreen_);
		archive.TryField("resolution", resolutionValue);
		archive.TryField("useDlss", useDlss_);
		archive.TryField("upscaleMode", upscaleModeValue);
		archive.TryField("useFrameGeneration", useFrameGeneration_);
		archive.TryField("vsync", vsync_);
		archive.TryField("useReflex", useReflex_);
		archive.TryField("useDeepDVC", useDeepDVC_);
		archive.TryField("initialScenePath", initialScenePath_);
		archive.TryField("showSplashWarning", showSplashWarning_);
		archive.TryField("showSplashFiction", showSplashFiction_);
		archive.TryField("executableName", executableName_);

		upscaleMode_ = static_cast<UpscaleMode>(upscaleModeValue);
		resolution_ = static_cast<ResolutionPreset>(resolutionValue);
	}

	void GameConfig::Save(const std::filesystem::path& path)const
	{
		if (path.has_parent_path())
		{
			std::filesystem::create_directories(path.parent_path());
		}

		BinaryOutputArchive archive;
		Int32 upscaleModeValue = static_cast<Int32>(upscaleMode_);
		Int32 resolutionValue = static_cast<Int32>(resolution_);

		archive.Field("windowWidth", windowWidth_);
		archive.Field("windowHeight", windowHeight_);
		archive.Field("fullscreen", fullscreen_);
		archive.Field("resolution", resolutionValue);
		archive.Field("useDlss", useDlss_);
		archive.Field("upscaleMode", upscaleModeValue);
		archive.Field("useFrameGeneration", useFrameGeneration_);
		archive.Field("vsync", vsync_);
		archive.Field("useReflex", useReflex_);
		archive.Field("useDeepDVC", useDeepDVC_);
		archive.Field("initialScenePath", initialScenePath_);
		archive.Field("showSplashWarning", showSplashWarning_);
		archive.Field("showSplashFiction", showSplashFiction_);
		archive.Field("executableName", executableName_);

		archive.Write(String(path.string()));
	}
}
