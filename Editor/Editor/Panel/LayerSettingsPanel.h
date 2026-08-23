#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	struct EditorContext;

	class LayerSettingsPanel
	{
	public:
		LayerSettingsPanel(EditorContext& context);
		~LayerSettingsPanel() = default;

		void Draw();

		void Open();

	private:
		void DrawCollisionMatrixTab();

	private:
		EditorContext& context_;

		Bool show_ = false;
	};
}
