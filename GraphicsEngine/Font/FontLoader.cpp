#include <GraphicsEngine/Font/FontLoader.h>
#include <GraphicsEngine/Font/FontManager.h>
#include <FoundationEngine/Resource/Gateway.h>

namespace SeedCore
{
	Handle<Font> FontLoader::Load(String filePath, Float fontSize)
	{
		Handle<Font> handle = pool_.Create();
		Font* font = pool_.Get(handle);
		if (!font)
		{
			return Handle<Font>::null();
		}

		font->fontSize_ = fontSize;

		std::string path = filePath.str();
		if (FT_New_Face(Gateway::GetFontManager().GetLibrary(), path.c_str(), 0, &font->ftFace_) != FT_Err_Ok)
		{
			Clear(handle);
			return Handle<Font>::null();
		}

		hb_face_t* hbFace = hb_face_create_from_file_or_fail(path.c_str(), 0);
		if (!hbFace)
		{
			Clear(handle);
			return Handle<Font>::null();
		}

		font->hbFont_ = hb_font_create(hbFace);
		hb_face_destroy(hbFace);

		const Int hbScale = static_cast<Int>(font->fontSize_ * 64.0f);
		hb_font_set_scale(font->hbFont_, hbScale, hbScale);

		font->msdfFont_ = msdfgen::adoptFreetypeFont(font->ftFace_);
		if (!font->msdfFont_)
		{
			Clear(handle);
			return Handle<Font>::null();
		}

		msdfgen::FontMetrics metrics{};
		if (!msdfgen::getFontMetrics(metrics, font->msdfFont_, msdfgen::FONT_SCALING_EM_NORMALIZED))
		{
			Clear(handle);
			return Handle<Font>::null();
		}

		font->geometryScale_ = static_cast<Double>(font->fontSize_);
		font->lineHeight_ = static_cast<Float>(metrics.lineHeight * font->geometryScale_);
		font->ascender_ = static_cast<Float>(metrics.ascenderY * font->geometryScale_);
		font->descender_ = static_cast<Float>(metrics.descenderY * font->geometryScale_);

		font->atlas_ = Font::Atlas(Font::AtlasGenerator(256, 256));
		font->atlas_.atlasGenerator().setThreadCount(static_cast<Int>(Max<Uint>(1u, std::thread::hardware_concurrency() / 2)));

		return handle;
	}

	Font* FontLoader::Get(const Handle<Font>& handle)
	{
		return pool_.Get(handle);
	}

	void FontLoader::Clear(Handle<Font>& handle)noexcept
	{
		Font* font = pool_.Get(handle);
		if (font)
		{
			font->glyphs_.clear();
			font->pendingGlyphs_.clear();

			font->atlasTexture_.Reset();

			if (font->msdfFont_)
			{
				msdfgen::destroyFont(font->msdfFont_);
				font->msdfFont_ = nullptr;
			}

			if (font->hbFont_)
			{
				hb_font_destroy(font->hbFont_);
				font->hbFont_ = nullptr;
			}

			if (font->ftFace_)
			{
				FT_Done_Face(font->ftFace_);
				font->ftFace_ = nullptr;
			}
		}

		pool_.Destroy(handle);
	}
}
