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
	};
}
