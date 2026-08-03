#pragma once
#include <FoundationEngine/Prelude.h>

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
		void Move(Matrix& view, Matrix& projection, Matrix& world, ImGuizmo::OPERATION operation, const Float* snap);

	private:
		EditorContext& context_;

		ImGuizmo::MODE currentMode_ = ImGuizmo::WORLD;
	};
}
