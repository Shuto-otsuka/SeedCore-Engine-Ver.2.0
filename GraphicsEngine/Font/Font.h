#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	class BindlessHeap;

	struct ShapedGlyph
	{
		Uint32 glyphIndex_ = 0;
		Uint32 cluster_ = 0;
		Vector2 advance_ = { 0.0f, 0.0f };
		Vector2 offset_ = { 0.0f, 0.0f };
	};

	struct GlyphAtlasEntry
	{
		Float atlasLeft_ = 0.0f;
		Float atlasBottom_ = 0.0f;
		Float atlasRight_ = 0.0f;
		Float atlasTop_ = 0.0f;

		Float planeLeft_ = 0.0f;
		Float planeBottom_ = 0.0f;
		Float planeRight_ = 0.0f;
		Float planeTop_ = 0.0f;

		Bool isWhitespace_ = false;
	};

	class Font
	{
	public:
		using AtlasStorage = msdf_atlas::BitmapAtlasStorage<msdf_atlas::byte, 4>;
		using AtlasGenerator = msdf_atlas::ImmediateAtlasGenerator<Float, 4, msdf_atlas::mtsdfGenerator, AtlasStorage>;
		using Atlas = msdf_atlas::DynamicAtlas<AtlasGenerator>;

		Font() = default;
		~Font() = default;

		Bool Initialize(FT_Library library, const std::string& filePath, Float fontSize);

		void Finalize();

		Bool Update();

		DynamicArray<ShapedGlyph> Shape(const std::string& utf8Text);

		const GlyphAtlasEntry* FindGlyph(Uint32 glyphIndex)const;

		msdfgen::BitmapConstRef<msdf_atlas::byte, 4> GetAtlasBitmap()const;

		Bool IsAtlasDirty()const;

		void ClearAtlasDirty();

		void UploadAtlas(ID3D12Device* device, ID3D12CommandQueue* cmdQueue, BindlessHeap* bindlessHeap);

		Uint GetAtlasTextureIndex()const;

		Bool HasAtlasTexture()const;

		Float GetPixelRange()const;

		Float GetFontSize()const;

		Float GetLineHeight()const;

		Float GetAscender()const;

		Float GetDescender()const;

	private:
		void RequestGlyph(Uint32 glyphIndex);

		FT_Face ftFace_ = nullptr;

		hb_font_t* hbFont_ = nullptr;

		msdfgen::FontHandle* msdfFont_ = nullptr;

		Atlas atlas_;

		std::unordered_map<Uint32, GlyphAtlasEntry> glyphs_;

		DynamicArray<Uint32> pendingGlyphs_;

		Bool atlasDirty_ = false;

		Microsoft::WRL::ComPtr<ID3D12Resource> atlasTexture_;

		Uint atlasTextureIndex_ = 0;

		Bool atlasTextureIndexAllocated_ = false;

		Int atlasTextureWidth_ = 0;

		Int atlasTextureHeight_ = 0;

		Double pixelRange_ = 32.0;

		Float fontSize_ = 32.0f;

		Double geometryScale_ = 1.0;

		Float lineHeight_ = 0.0f;

		Float ascender_ = 0.0f;

		Float descender_ = 0.0f;
	};
}
