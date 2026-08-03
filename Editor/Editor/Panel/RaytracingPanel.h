#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	struct EditorContext;

	/// [JP] グラフィックス→レイトレーシング メニュー配下。GraphicsMenuPanel が保持する。
	///      DrawMenuItems() は「レイトレーシング」BeginMenu の内側で呼ぶこと
	///      (そこの MenuItem がクリックされたら show フラグを立てるだけ)。
	///      DrawWindows() は BeginMenu の外側・毎フレーム無条件で呼ぶこと
	///      (メニューを閉じてもウィンドウ自体は表示し続けるため)。
	class RaytracingPanel
	{
	public:
		RaytracingPanel(EditorContext& context);
		~RaytracingPanel() = default;

		void DrawMenuItems();

		void DrawWindows();

	private:
		void DrawShadowSettingsWindow();

		void DrawAmbientOcclusionSettingsWindow();

		void DrawSubsurfaceScatteringSettingsWindow();

		void DrawReflectionSettingsWindow();

		void DrawGlobalIlluminationSettingsWindow();

		void DrawRefractionSettingsWindow();

		void DrawCausticsSettingsWindow();

		void DrawVolumetricCloudScapesSettingsWindow();

		void DrawVolumetricStarSettingsWindow();

		void DrawVolumetricLightSettingsWindow();

		/// [JP] チューニング構造体がまだ無い(器のみの)RT機能用。タイトルと
		///      enabled フラグだけ受け取り、「有効」チェックボックス1つだけの
		///      小さいウィンドウを描く。show は呼び出し側(このクラス)が持つ bool。
		void DrawPlaceholderSettingsWindow(const Char* title, Bool& show, Bool& enabled);

		EditorContext& context_;

		Bool showShadowSettings_ = false;

		Bool showAmbientOcclusionSettings_ = false;
		Bool showGlobalIlluminationSettings_ = false;
		Bool showReflectionSettings_ = false;
		Bool showRefractionSettings_ = false;
		Bool showCausticsSettings_ = false;
		Bool showVolumetricLightSettings_ = false;
		Bool showVolumetricCloudScapesSettings_ = false;
		Bool showVolumetricStarSettings_ = false;
		Bool showSubsurfaceScatteringSettings_ = false;
	};
}
