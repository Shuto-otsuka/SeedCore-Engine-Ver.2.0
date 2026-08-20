#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	struct EditorContext;

	class BoneControllerPanel
	{
	public:
		BoneControllerPanel(EditorContext& context);

		void Draw();

		void Open();

	private:
		EditorContext& context_;

		Bool show_ = false;
	};
}
