#pragma once
#include <FoundationEngine/Prelude.h>

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
	};
}
