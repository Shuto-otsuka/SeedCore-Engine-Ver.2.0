#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/ECS/Entity.h>

namespace SeedCore
{
	struct EditorContext;

	class GuizmoPanel2D
	{
	public:
		GuizmoPanel2D(EditorContext& context);
		~GuizmoPanel2D() = default;

		void Draw(const Vector2& position, const Vector2& size);

	private:
		void Move(Matrix& view, Matrix& projection, Matrix& pivot, ImGuizmo::OPERATION operation, const Float* snap);

	private:
		EditorContext& context_;

		ImGuizmo::MODE currentMode_ = ImGuizmo::WORLD;

		/// [EN] Whether a gizmo drag (ImGuizmo::IsUsing()) was in progress as of the previous frame - used to detect the drag-start/drag-end edges so a single undo Command (a CompoundCommand across a multi-selection) is pushed spanning the whole drag rather than one per frame.
		/// [JP] 前フレーム時点でギズモのドラッグ(ImGuizmo::IsUsing())が進行中だったかどうか — ドラッグ開始/終了のエッジを検出し、フレーム毎ではなくドラッグ全体で1つの undo Command(複数選択時は CompoundCommand)を積むために使う。
		Bool wasDragging_ = false;

		/// [EN] The actors being dragged, captured at drag-start so drag-end can push commands against the same entities even if selection changed mid-drag. Index-parallel with dragStartWorldMatrices_/dragStartPositions_/dragStartRotations_/dragStartScales_.
		/// [JP] ドラッグ開始時点で捕捉した操作対象 actor 群。ドラッグ中に選択が変わってもドラッグ終了時に同じ entity 群へコマンドを積めるようにする。dragStartWorldMatrices_/dragStartPositions_/dragStartRotations_/dragStartScales_ とインデックスが対応する。
		DynamicArray<Entity> dragEntities_;

		/// [EN] Each dragged actor's world matrix at drag-start - combined with the pivot's delta transform every frame to compute that actor's new world matrix.
		/// [JP] 各ドラッグ対象 actor のドラッグ開始時点のワールド行列 — 毎フレーム、ピボットのデルタ変換と組み合わせてその actor の新しいワールド行列を求める。
		DynamicArray<Matrix> dragStartWorldMatrices_;

		/// [EN] Position/Rotation/Scale values captured at drag-start, diffed against the current values at drag-end to build undo Commands.
		/// [JP] ドラッグ開始時点で捕捉した Position/Rotation/Scale 値。ドラッグ終了時点の現在値と比較し、undo Command を組み立てる。
		DynamicArray<Vector3> dragStartPositions_;
		DynamicArray<Vector3> dragStartRotations_;
		DynamicArray<Vector3> dragStartScales_;

		/// [EN] The gizmo's own matrix at drag-start. With a single actor selected this equals that actor's world matrix; with multiple actors selected it's their unrotated average position (see Draw()). Move() computes each frame's pivot delta as dragStartPivotMatrix_.Invert() * (current gizmo matrix) and applies it to every dragged actor's dragStartWorldMatrices_ entry.
		/// [JP] ドラッグ開始時点のギズモ自身の行列。単一選択時はその actor のワールド行列と等しく、複数選択時はそれらの(無回転の)平均位置になる(Draw() 参照)。Move() は毎フレーム、dragStartPivotMatrix_.Invert() * (現在のギズモ行列) としてピボットのデルタを求め、各ドラッグ対象 actor の dragStartWorldMatrices_ へ適用する。
		Matrix dragStartPivotMatrix_ = Matrix::Identity;

		/// [EN] The gizmo's displayed matrix, persisted across frames. Only rebuilt from the live selection while NOT dragging - during a drag it is mutated in-place by ImGuizmo::Manipulate() inside Move(), so a multi-select pivot's rotation/scale accumulated over earlier frames of the same drag isn't discarded.
		/// [JP] ギズモの表示行列。フレームをまたいで保持される。ドラッグ中でない間だけ現在の選択から作り直す — ドラッグ中は Move() 内の ImGuizmo::Manipulate() がその場で書き換える。こうしないと複数選択ピボットが同一ドラッグ中に蓄積した回転/スケールが巻き戻ってしまう。
		Matrix pivotMatrix_ = Matrix::Identity;
	};
}
