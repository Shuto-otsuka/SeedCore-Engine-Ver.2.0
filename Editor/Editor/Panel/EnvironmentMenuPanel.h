#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	struct EditorContext;

	/// [JP] 「グラフィックス」メニュー配下の「環境」サブメニュー(ビューモード/
	///      レイトレーシングと同じ並び、GraphicsMenuPanel が保持する)。
	///      DaySystem/SunLight/MoonLight を持つ(雨/雪の調整値は Weather
	///      コンポーネントのインスペクタ側)。星(VolumetricStar)は
	///      雲(VolumetricCloudScapes)と対の機能なので、こちらではなく
	///      RaytracingPanel 側に置く。RaytracingPanel と同じ形: DrawMenuItems()
	///      は「環境」BeginMenu の内側で呼び、DrawWindows() は毎フレーム
	///      無条件で呼ぶ。
	class EnvironmentMenuPanel
	{
	public:
		EnvironmentMenuPanel(EditorContext& context);
		~EnvironmentMenuPanel() = default;

		void DrawMenuItems();

		void DrawWindows();

	private:
		void DrawDaySystemSettingsWindow();

		void DrawSunLightSettingsWindow();

		void DrawMoonLightSettingsWindow();

	private:
		EditorContext& context_;

		Bool showDaySystemSettings_ = false;
		Bool showSunLightSettings_ = false;
		Bool showMoonLightSettings_ = false;
	};
}
