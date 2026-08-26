#pragma once
#include <FoundationEngine/Prelude.h>
#include <GraphicsEngine/DLSS/DlssMode.h>
#include <GraphicsEngine/D3D12/SwapChain/GraphicsResolution.h>

namespace SeedCore
{
	class EditorCamera;
	class EditorCameraController;
	class ImGuiRenderer;

	struct GameConfig
	{
		Uint32 windowWidth_ = 1920;

		Uint32 windowHeight_ = 1080;

		Bool fullscreen_ = false;

		/// [EN] Render/output resolution — the DLSS-RR/PostProcess upscale
		///      target and G-Buffer-relative render resolution's source. Fixed
		///      presets (HHD..UHD8K), not a free-form value. Independent of
		///      windowWidth_/windowHeight_ above (the OS window's client size)
		///      — the rendered image is scaled to fit the window/viewport
		///      regardless of this value.
		/// [JP] レンダー/出力解像度 — DLSS-RR/PostProcessのアップスケール先、
		///      および G-Buffer 関連のレンダー解像度の元になる値。固定プリセット
		///      (HHD..UHD8K)、自由入力ではない。上の windowWidth_/windowHeight_
		///      (OSウィンドウのクライアントサイズ)とは独立 — 描画された画像は
		///      この値に関わらずウィンドウ/ビューポートに収まるよう表示される。
		ResolutionPreset resolution_ = ResolutionPreset::FHD;

		Bool useDlss_ = false;

		UpscaleMode upscaleMode_ = UpscaleMode::Balanced;

		Bool useFrameGeneration_ = false;

		Bool vsync_ = false;

		Bool useReflex_ = false;

		Bool useDeepDVC_ = false;

		String initialScenePath_;

		void Load(const std::filesystem::path& path = "../UserProject/Assets/Config/GameConfig.scg");

		void Save(const std::filesystem::path& path = "../UserProject/Assets/Config/GameConfig.scg")const;
	};

	struct EditorConfig
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

		void Load(const std::filesystem::path& path = "../Editor/Config/EditorConfig.sc");

		void Save(const std::filesystem::path& path = "../Editor/Config/EditorConfig.sc")const;

		void Capture(const EditorCamera& camera, const EditorCameraController& controller, const ImGuiRenderer& imgui, const std::filesystem::path& currentScenePath);

		void Apply(EditorCamera& camera, EditorCameraController& controller, ImGuiRenderer& imgui)const;
	};
}
