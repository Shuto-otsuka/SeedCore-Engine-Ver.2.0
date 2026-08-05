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
		void Move(Matrix& view, Matrix& projection, Matrix& world, ImGuizmo::OPERATION operation, const Float* snap);

	private:
		EditorContext& context_;

		ImGuizmo::MODE currentMode_ = ImGuizmo::WORLD;

		/// [EN] Whether a gizmo drag (ImGuizmo::IsUsing()) was in progress as of the previous frame - used to detect the drag-start/drag-end edges so a single undo Command can be pushed spanning the whole drag rather than one per frame.
		/// [JP] 前フレーム時点でギズモのドラッグ(ImGuizmo::IsUsing())が進行中だったかどうか — ドラッグ開始/終了のエッジを検出し、フレーム毎ではなくドラッグ全体で1つのundo Commandを積むために使う。
		Bool wasDragging_ = false;

		/// [EN] The actor being dragged, captured at drag-start so drag-end can push commands against the same entity even if selection changed mid-drag.
		/// [JP] ドラッグ開始時点で捕捉した操作対象actor。ドラッグ中に選択が変わってもドラッグ終了時に同じentityへコマンドを積めるようにする。
		Entity dragEntity_ = Entity::Null();

		/// [EN] Position/Rotation/Scale values captured at drag-start, diffed against the current values at drag-end to build undo Commands.
		/// [JP] ドラッグ開始時点で捕捉したPosition/Rotation/Scale値。ドラッグ終了時点の現在値と比較し、undo Commandを組み立てる。
		Vector3 dragStartPosition_ = Vector3::Zero;
		Vector3 dragStartRotation_ = Vector3::Zero;
		Vector3 dragStartScale_ = Vector3::One;
	};
}
