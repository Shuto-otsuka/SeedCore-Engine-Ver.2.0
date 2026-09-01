#pragma once
#include <FoundationEngine/Prelude.h>
#include <GraphicsEngine/Quality/GraphicsQuality.h>

namespace SeedCore
{
	struct EditorContext;

	/// [JP] グラフィックス→ラスタライゼーション メニュー配下。GraphicsMenuPanel が保持する。
	///      RaytracingPanel と同じ作法: DrawMenuItems() は「ラスタライゼーション」BeginMenu の
	///      内側で、DrawWindows() は BeginMenu の外側・毎フレーム無条件で呼ぶこと。
	///      SDFR / DDGI（レイトレーシングでもスクリーンスペースでもない中間段の手法）も
	///      置き場所としてここに同居する。
	class RasterizationPanel
	{
	public:
		RasterizationPanel(EditorContext& context);
		~RasterizationPanel() = default;

		void DrawMenuItems();

		void DrawWindows();

	private:
		void DrawVirtualShadowMapSettingsWindow();

		void DrawCascadedShadowMapSettingsWindow();

		void DrawSignedDistanceFieldReflectionSettingsWindow();

		void DrawDynamicDiffuseGlobalIlluminationSettingsWindow();

		Bool DrawEnableCheckbox(GraphicsEffect effect, Bool siblingClaimed, Bool& enabled);

		Bool BeginSettingsWindow(const Char* title, Bool& show);

		void EndSettingsWindow(Bool& show);

		EditorContext& context_;

		Bool showVirtualShadowMapSettings_ = false;
		Bool showCascadedShadowMapSettings_ = false;
		Bool showSignedDistanceFieldReflectionSettings_ = false;
		Bool showDynamicDiffuseGlobalIlluminationSettings_ = false;
	};
}
