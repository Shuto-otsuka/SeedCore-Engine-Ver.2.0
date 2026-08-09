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

		EditorContext& context_;
		ImGuiTexture& imguiTexture_;

		GuizmoPanel2D guizmoPanel_;

		Bool isPanning_ = false;

		Bool isResettingView_ = false;
	};
}
