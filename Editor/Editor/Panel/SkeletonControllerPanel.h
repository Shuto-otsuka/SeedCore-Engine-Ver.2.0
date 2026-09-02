#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	struct EditorContext;

	class SkeletonControllerPanel
	{
	public:
		SkeletonControllerPanel(EditorContext& context);

		void Draw();

		void Open();

	private:
		EditorContext& context_;

		Bool show_ = false;
	};
}
