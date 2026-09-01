#pragma once
#include <FoundationEngine/Prelude.h>
#include <GraphicsEngine/Quality/GraphicsQuality.h>

namespace SeedCore
{
	struct EditorContext;

	/// [JP] グラフィックス→スクリーンスペース メニュー配下。GraphicsMenuPanel が保持する。
	///      RaytracingPanel と同じ作法: DrawMenuItems() は「スクリーンスペース」BeginMenu の
	///      内側で、DrawWindows() は BeginMenu の外側・毎フレーム無条件で呼ぶこと。
	class ScreenSpacePanel
	{
	public:
		ScreenSpacePanel(EditorContext& context);
		~ScreenSpacePanel() = default;

		void DrawMenuItems();

		void DrawWindows();

	private:
		void DrawGroundTruthAmbientOcclusionSettingsWindow();

		void DrawAmbientOcclusionSettingsWindow();

		void DrawGlobalIlluminationSettingsWindow();

		void DrawReflectionSettingsWindow();

		Bool DrawEnableCheckbox(GraphicsEffect effect, Bool siblingClaimed, Bool& enabled);

		Bool BeginSettingsWindow(const Char* title, Bool& show);

		void EndSettingsWindow(Bool& show);

		EditorContext& context_;

		Bool showGroundTruthAmbientOcclusionSettings_ = false;
		Bool showAmbientOcclusionSettings_ = false;
		Bool showGlobalIlluminationSettings_ = false;
		Bool showReflectionSettings_ = false;
	};
}
