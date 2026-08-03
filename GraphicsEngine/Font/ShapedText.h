#pragma once
#include <FoundationEngine/Prelude.h>
#include <GraphicsEngine/Font/Font.h>

namespace SeedCore
{
	class ShapedText
	{
	public:
		ShapedText() = default;
		~ShapedText() = default;

		void SetFont(Font* font);

		void SetText(const std::string& text);

		void Update();

		Font* GetFont()const;

		const std::string& GetText()const;

		const DynamicArray<ShapedGlyph>& GetShapedGlyphs()const;

	private:
		Font* font_ = nullptr;

		std::string text_;

		DynamicArray<ShapedGlyph> shapedGlyphs_;

		Bool dirty_ = false;
	};
}
