#pragma once
#include <FoundationEngine/Prelude.h>
#include <Editor/Editor/Panel/GuizmoPanel3D.h>

namespace SeedCore
{
	struct EditorContext;
	class ImGuiTexture;

	class EditorWindowPanel
	{
	public:
		EditorWindowPanel(EditorContext& context, ImGuiTexture& imguiTexture);
		~EditorWindowPanel() = default;

		void Draw(D3D12_GPU_DESCRIPTOR_HANDLE frameBufferHandle);

	private:
		void DrawGizmoMenu();

		EditorContext& context_;
		ImGuiTexture& imguiTexture_;

		GuizmoPanel3D guizmoPanel_;

		Bool focusedOnce_ = false;

		/// [EN] Seconds remaining to keep showing the move-speed tooltip after the
		///      last right-click+wheel tick, so it's readable instead of flashing
		///      for a single frame - 0 (the default) means don't show it.
		/// [JP] 右クリック+ホイールの最後の入力から、移動速度ツールチップを
		///      表示し続ける残り秒数。1フレームだけ点滅して読めなくなるのを防ぐ
		///      - 0（デフォルト）は非表示を意味する。
		Float moveSpeedTooltipTimer_ = 0.0f;
	};
}
