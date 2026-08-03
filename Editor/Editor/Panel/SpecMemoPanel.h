#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	struct EditorContext;

	struct LibraryEntry
	{
		const Char* name_;
		const Char* version_;
		const Char* usage_;
	};

	class SpecMemoPanel
	{
	public:
		SpecMemoPanel(EditorContext& context);
		~SpecMemoPanel() = default;

		void Draw();

		void Open();

	private:
		Bool show_ = false;
	};
}
