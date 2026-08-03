#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	struct EditorContext;

	class ShortCutKeyPanel
	{
	public:
		ShortCutKeyPanel(EditorContext& context);
		~ShortCutKeyPanel() = default;

		void Draw();

		void Open();

	private:
		Bool show_ = false;
	};
}
