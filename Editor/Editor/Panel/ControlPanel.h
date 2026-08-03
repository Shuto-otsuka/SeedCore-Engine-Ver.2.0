#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	struct EditorContext;
	class ImGuiTexture;

	class ControlPanel
	{
	public:
		ControlPanel(EditorContext& context, ImGuiTexture& imguiTexture);
		~ControlPanel() = default;

		Float Draw();

	private:
		EditorContext& context_;

		ImGuiTexture& imguiTexture_;
	};
}
