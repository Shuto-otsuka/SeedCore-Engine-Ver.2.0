#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	struct GuizmoContext
	{
		Float translateSnap_ = 1.0f;
		Float rotateSnap_ = 5.0f;
		Float scaleSnap_ = 0.5f;
		Bool showGuizmo_ = true;

		ImGuizmo::OPERATION guizmoOperation_ = ImGuizmo::TRANSLATE;
	};
}
