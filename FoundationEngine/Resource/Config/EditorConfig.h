#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	struct SEEDCORE_API EditorConfig
	{
		Vector3 cameraEye_ = { 0,0,-10 };

		Vector3 cameraFocus_ = { 0,0,0 };

		Vector3 cameraUp_ = { 0,1,0 };

		Float cameraFov_ = 60.0f;

		Float cameraMoveSpeed_ = 10.0f;

		Float cameraRotateSpeed_ = 0.15f;

		Float cameraScrollSpeed_ = 5.0f;

		Float cameraPanSpeed_ = 0.02f;

		Float cameraShiftSpeedMultiplier_ = 3.0f;

		Float fontScale_ = 1.0f;

		String lastScenePath_;

		String atomCraftPath_;

		String acfPath_;

		void Load(const std::filesystem::path& path = "../Editor/Config/EditorConfig.sc");

		void Save(const std::filesystem::path& path = "../Editor/Config/EditorConfig.sc")const;
	};
}
