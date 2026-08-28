#pragma once
#include <FoundationEngine/Prelude.h>
#include <Editor/Editor/Panel/GuizmoPanel2D.h>

namespace SeedCore
{
	struct EditorContext;
	class ImGuiTexture;

	class CanvasViewPanel
	{
	public:
		CanvasViewPanel(EditorContext& context, ImGuiTexture& imguiTexture);
		~CanvasViewPanel() = default;

		void Draw(D3D12_GPU_DESCRIPTOR_HANDLE frameBufferHandle);

	private:
		void DrawGizmoMenu();

		/**
		* [EN]
		* Canvas selection input, called each frame the canvas image is live.
		* A left click that never becomes a drag selects the topmost actor
		* whose canvas-space quad the mouse is inside (Ctrl toggles the actor
		* in the multi-selection; a plain click replaces it; a miss clears
		* it). A left drag over empty canvas draws a rubber-band box and, on
		* release, selects every actor whose canvas quad overlaps it (Ctrl
		* adds to the current selection instead of replacing).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* キャンバス選択入力。キャンバス画像が有効な間、毎フレーム呼ばれる。
		* ドラッグにならなかった左クリックは、マウスがキャンバス空間クアッド
		* の内側にある最前面のアクターを選択する(Ctrl はトグル、通常クリック
		* は置き換え、外れると解除)。空のキャンバス上での左ドラッグはラバー
		* バンドボックスを描き、離した時にそれと重なる全アクターを選択する
		* (Ctrl は置き換えずに現在の選択へ追加)。
		*/
		void HandlePicking(const ImVec2& imageScreenPos, Float imageWidth, Float imageHeight);

		EditorContext& context_;
		ImGuiTexture& imguiTexture_;

		GuizmoPanel2D guizmoPanel_;

		Bool isPanning_ = false;

		Bool isResettingView_ = false;

		/// [EN] A left-button press landed on the canvas (not on a gizmo handle) and is still held - still ambiguous between a click and a rubber-band box until the mouse moves far enough.
		/// [JP] 左ボタンの押下がキャンバス上(ギズモハンドル以外)で始まり、まだ押されている — マウスが十分動くまでクリックとラバーバンドボックスのどちらか未確定。
		Bool isBoxSelectPending_ = false;

		/// [EN] The pending press has moved far enough to be a rubber-band box select; the rectangle is drawn each frame and applied on release.
		/// [JP] 押下がラバーバンドボックス選択と確定するまで動いた。矩形を毎フレーム描画し、離した時に適用する。
		Bool isBoxSelecting_ = false;

		/// [EN] Screen-space position of the left-button press that started the current click / box-select gesture.
		/// [JP] 現在のクリック / ボックス選択ジェスチャを開始した左ボタン押下のスクリーン空間座標。
		ImVec2 boxSelectStartScreen_ = ImVec2(0.0f, 0.0f);
	};
}
