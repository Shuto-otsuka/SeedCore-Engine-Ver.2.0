#pragma once
#include <FoundationEngine/Prelude.h>
#include <GraphicsEngine/Renderer/ViewMode.h>
#include <Editor/Editor/Panel/RaytracingPanel.h>
#include <Editor/Editor/Panel/EnvironmentMenuPanel.h>

namespace SeedCore
{
	struct EditorContext;

	/// [JP] メインメニューバー内の「グラフィックス」メニュー。表示モード（ビューモード）
	///      の選択を EditorContext::viewMode_ に対して行う。状態はエディタービューの
	///      ツールバーと共有する。BeginMainMenuBar の内側で Draw を呼ぶこと。
	///      「レイトレーシング」配下は RaytracingPanel、「環境」配下は
	///      EnvironmentMenuPanel に委譲する。
	class GraphicsMenuPanel
	{
	public:
		GraphicsMenuPanel(EditorContext& context);
		~GraphicsMenuPanel() = default;

		void Draw();

	private:
		EditorContext& context_;

		RaytracingPanel raytracingPanel_;
		EnvironmentMenuPanel environmentMenuPanel_;
	};
}
