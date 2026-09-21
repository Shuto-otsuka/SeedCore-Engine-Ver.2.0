#include <FoundationEngine/Resource/Config/EditorConfig.h>
#include <FoundationEngine/Serialization/Json/JsonArchive.h>

namespace SeedCore
{
	void EditorConfig::Load(const std::filesystem::path& path)
	{
		JsonInputArchive archive;
		if (!archive.Read(String(path.string())))
		{
			return;
		}

		archive.TryField("cameraEye", cameraEye_);
		archive.TryField("cameraFocus", cameraFocus_);
		archive.TryField("cameraUp", cameraUp_);
		archive.TryField("cameraFov", cameraFov_);
		archive.TryField("cameraMoveSpeed", cameraMoveSpeed_);
		archive.TryField("cameraRotateSpeed", cameraRotateSpeed_);
		archive.TryField("cameraScrollSpeed", cameraScrollSpeed_);
		archive.TryField("cameraPanSpeed", cameraPanSpeed_);
		archive.TryField("cameraShiftSpeedMultiplier", cameraShiftSpeedMultiplier_);
		archive.TryField("fontScale", fontScale_);
		archive.TryField("lastScenePath", lastScenePath_);
		archive.TryField("atomCraftPath", atomCraftPath_);
		archive.TryField("acfPath", acfPath_);
	}

	void EditorConfig::Save(const std::filesystem::path& path)const
	{
		if (path.has_parent_path())
		{
			std::filesystem::create_directories(path.parent_path());
		}

		JsonOutputArchive archive;
		archive.Field("cameraEye", cameraEye_);
		archive.Field("cameraFocus", cameraFocus_);
		archive.Field("cameraUp", cameraUp_);
		archive.Field("cameraFov", cameraFov_);
		archive.Field("cameraMoveSpeed", cameraMoveSpeed_);
		archive.Field("cameraRotateSpeed", cameraRotateSpeed_);
		archive.Field("cameraScrollSpeed", cameraScrollSpeed_);
		archive.Field("cameraPanSpeed", cameraPanSpeed_);
		archive.Field("cameraShiftSpeedMultiplier", cameraShiftSpeedMultiplier_);
		archive.Field("fontScale", fontScale_);
		archive.Field("lastScenePath", lastScenePath_);
		archive.Field("atomCraftPath", atomCraftPath_);
		archive.Field("acfPath", acfPath_);

		archive.Write(String(path.string()));
	}
}
