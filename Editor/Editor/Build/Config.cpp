#include <Editor/Editor/Build/Config.h>
#include <FoundationEngine/Serialization/Json/JsonArchive.h>
#include <GraphicsEngine/Camera/EditorCamera.h>
#include <GraphicsEngine/Camera/EditorCameraController.h>
#include <Editor/Editor/ImGui/ImGuiRenderer.h>

namespace SeedCore
{
	void GameConfig::Load(const std::filesystem::path& path)
	{
		JsonInputArchive archive;
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

		upscaleMode_ = static_cast<UpscaleMode>(upscaleModeValue);
		resolution_ = static_cast<ResolutionPreset>(resolutionValue);
	}

	void GameConfig::Save(const std::filesystem::path& path)const
	{
		if (path.has_parent_path())
		{
			std::filesystem::create_directories(path.parent_path());
		}

		JsonOutputArchive archive;
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

		archive.Write(String(path.string()));
	}

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

		archive.Write(String(path.string()));
	}

	void EditorConfig::Capture(const EditorCamera& camera, const EditorCameraController& controller, const ImGuiRenderer& imgui, const std::filesystem::path& currentScenePath)
	{
		cameraEye_ = camera.Eye();
		cameraFocus_ = camera.Focus();
		cameraUp_ = camera.Up();
		cameraFov_ = camera.Fov();

		cameraMoveSpeed_ = controller.MoveSpeed();
		cameraRotateSpeed_ = controller.RotateSpeed();
		cameraScrollSpeed_ = controller.ScrollSpeed();
		cameraPanSpeed_ = controller.PanSpeed();
		cameraShiftSpeedMultiplier_ = controller.ShiftSpeedMultiplier();

		fontScale_ = imgui.FontScale();

		lastScenePath_ = String(currentScenePath.string());
	}

	void EditorConfig::Apply(EditorCamera& camera, EditorCameraController& controller, ImGuiRenderer& imgui)const
	{
		camera.Eye(cameraEye_);
		camera.Focus(cameraFocus_);
		camera.Up(cameraUp_);
		camera.Fov(cameraFov_);

		controller.MoveSpeed(cameraMoveSpeed_);
		controller.RotateSpeed(cameraRotateSpeed_);
		controller.ScrollSpeed(cameraScrollSpeed_);
		controller.PanSpeed(cameraPanSpeed_);
		controller.ShiftSpeedMultiplier(cameraShiftSpeedMultiplier_);

		imgui.FontScale(fontScale_);
	}
}
