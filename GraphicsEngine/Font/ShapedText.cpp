#include <GraphicsEngine/Font/ShapedText.h>

namespace SeedCore
{
	void ShapedText::SetFont(Font* font)
	{
		if (font_ != font)
		{
			font_ = font;
			dirty_ = true;
		}
	}

	void ShapedText::SetText(const std::string& text)
	{
		if (text_ != text)
		{
			text_ = text;
			dirty_ = true;
		}
	}

	void ShapedText::Update()
	{
		if (!dirty_ || !font_)
		{
			return;
		}

		shapedGlyphs_ = font_->Shape(text_);
		dirty_ = false;
	}

	Font* ShapedText::GetFont()const
	{
		return font_;
	}

	const std::string& ShapedText::GetText()const
	{
		return text_;
	}

	const DynamicArray<ShapedGlyph>& ShapedText::GetShapedGlyphs()const
	{
		return shapedGlyphs_;
	}
}