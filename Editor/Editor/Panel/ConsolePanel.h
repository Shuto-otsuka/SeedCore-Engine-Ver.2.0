#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	class ImGuiTexture;

	class ConsolePanel
	{
	public:
		ConsolePanel(ImGuiTexture& imguiTexture);
		~ConsolePanel() = default;

		void Draw();

	private:
		ImGuiTexture& imguiTexture_;

		Bool showNotice_ = true;
		Bool showWarning_ = true;
		Bool showError_ = true;
		Bool autoScroll_ = true;
	};
}
