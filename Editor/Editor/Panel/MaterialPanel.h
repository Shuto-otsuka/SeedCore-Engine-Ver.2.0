#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	struct EditorContext;

	class MaterialPanel
	{
	public:
		MaterialPanel(EditorContext& context);

		void Draw();

		void Open();

	private:
		EditorContext& context_;

		Bool show_ = false;
	};
}
