#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/ECS/Entity.h>

namespace SeedCore
{
	struct EditorContext;

	class GuizmoPanel3D
	{
	public:
		GuizmoPanel3D(EditorContext& context);
		~GuizmoPanel3D() = default;

		void Draw(const Vector2& position, const Vector2& size);

	private:
		void Move(Matrix& view, Matrix& projection, Matrix& pivot, ImGuizmo::OPERATION operation, const Float* snap);

	private:
		EditorContext& context_;

		ImGuizmo::MODE currentMode_ = ImGuizmo::WORLD;

		/// [EN] Whether a gizmo drag (ImGuizmo::IsUsing()) was in progress as of the previous frame - used to detect the drag-start/drag-end edges so a single undo Command can be pushed spanning the whole drag rather than one per frame.
		/// [JP] 前フレーム時点でギズモのドラッグ(ImGuizmo::IsUsing())が進行中だったかどうか — ドラッグ開始/終了のエッジを検出し、フレーム毎ではなくドラッグ全体で1つのundo Commandを積むために使う。
		Bool wasDragging_ = false;

		/// [EN] The actors being dragged, captured at drag-start so drag-end can push commands against the same entities even if selection changed mid-drag. Index-parallel with dragStartWorldMatrices_/dragStartPositions_/dragStartRotations_/dragStartScales_.
		/// [JP] ドラッグ開始時点で捕捉した操作対象actor群。ドラッグ中に選択が変わってもドラッグ終了時に同じentity群へコマンドを積めるようにする。dragStartWorldMatrices_/dragStartPositions_/dragStartRotations_/dragStartScales_とインデックスが対応する。
		DynamicArray<Entity> dragEntities_;

		/// [EN] Each dragged actor's world matrix at drag-start - combined with the pivot's delta transform every frame to compute that actor's new world matrix.
		/// [JP] 各ドラッグ対象actorのドラッグ開始時点のワールド行列 — 毎フレーム、ピボットのデルタ変換と組み合わせてそのactorの新しいワールド行列を求める。
		DynamicArray<Matrix> dragStartWorldMatrices_;

		/// [EN] Position/Rotation/Scale values captured at drag-start, diffed against the current values at drag-end to build undo Commands.
		/// [JP] ドラッグ開始時点で捕捉したPosition/Rotation/Scale値。ドラッグ終了時点の現在値と比較し、undo Commandを組み立てる。
		DynamicArray<Vector3> dragStartPositions_;
		DynamicArray<Vector3> dragStartRotations_;
		DynamicArray<Vector3> dragStartScales_;

		/// [EN] The gizmo's own matrix at drag-start. With a single actor selected this equals that actor's world matrix; with multiple actors selected it's their unrotated average position (see Draw()). Move() computes each frame's pivot delta as dragStartPivotMatrix_.Invert() * (current gizmo matrix) and applies it to every dragged actor's dragStartWorldMatrices_ entry.
		/// [JP] ドラッグ開始時点のギズモ自身の行列。単一選択時はそのactorのワールド行列と等しく、複数選択時はそれらの（無回転の）平均位置になる（Draw()参照）。Move()は毎フレーム、dragStartPivotMatrix_.Invert() * (現在のギズモ行列) としてピボットのデルタを求め、各ドラッグ対象actorのdragStartWorldMatrices_へ適用する。
		Matrix dragStartPivotMatrix_ = Matrix::Identity;

		/// [EN] The gizmo's displayed matrix, persisted across frames. Only rebuilt from the live selection while NOT dragging (ImGuizmo::IsUsing() false) - while a drag is in progress it is left alone here and instead mutated in-place by ImGuizmo::Manipulate() inside Move(), so a multi-select pivot's accumulated rotation/scale from earlier frames of the same drag isn't discarded (recomputing it fresh every frame from actor positions would collapse it back to an unrotated translate-only matrix mid-drag).
		/// [JP] ギズモの表示行列。フレームをまたいで保持される。ドラッグ中でない（ImGuizmo::IsUsing()がfalseの）間だけ、現在の選択から作り直す — ドラッグ中はここでは触らず、Move()内のImGuizmo::Manipulate()がその場で書き換える。こうしないと、複数選択ピボットが同一ドラッグの前フレームまでに蓄積した回転/スケールが、毎フレームactorの位置から作り直すことで無回転の平行移動行列へ巻き戻ってしまう。
		Matrix pivotMatrix_ = Matrix::Identity;
	};
}
