#include <GraphicsEngine/Font/Font.h>
#include <GraphicsEngine/D3D12/Descriptor/BindlessHeap.h>

namespace SeedCore
{
	Bool Font::Initialize(FT_Library library, const std::string& filePath, Float fontSize)
	{
		fontSize_ = fontSize;

		if (FT_New_Face(library, filePath.c_str(), 0, &ftFace_) != FT_Err_Ok)
		{
			return false;
		}

		hb_face_t* hbFace = hb_face_create_from_file_or_fail(filePath.c_str(), 0);
		if (!hbFace)
		{
			Finalize();
			return false;
		}

		hbFont_ = hb_font_create(hbFace);
		hb_face_destroy(hbFace);

		const Int hbScale = static_cast<Int>(fontSize_ * 64.0f);
		hb_font_set_scale(hbFont_, hbScale, hbScale);

		msdfFont_ = msdfgen::adoptFreetypeFont(ftFace_);
		if (!msdfFont_)
		{
			Finalize();
			return false;
		}

		msdfgen::FontMetrics metrics{};
		if (!msdfgen::getFontMetrics(metrics, msdfFont_, msdfgen::FONT_SCALING_EM_NORMALIZED))
		{
			Finalize();
			return false;
		}

		geometryScale_ = static_cast<Double>(fontSize_);
		lineHeight_ = static_cast<Float>(metrics.lineHeight * geometryScale_);
		ascender_ = static_cast<Float>(metrics.ascenderY * geometryScale_);
		descender_ = static_cast<Float>(metrics.descenderY * geometryScale_);

		atlas_ = Atlas(AtlasGenerator(256, 256));
		atlas_.atlasGenerator().setThreadCount(static_cast<Int>(Max<Uint>(1u, std::thread::hardware_concurrency() / 2)));

		return true;
	}

	void Font::Finalize()
	{
		glyphs_.clear();
		pendingGlyphs_.clear();

		atlasTexture_.Reset();

		if (msdfFont_)
		{
			msdfgen::destroyFont(msdfFont_);
			msdfFont_ = nullptr;
		}

		if (hbFont_)
		{
			hb_font_destroy(hbFont_);
			hbFont_ = nullptr;
		}

		if (ftFace_)
		{
			FT_Done_Face(ftFace_);
			ftFace_ = nullptr;
		}
	}

	Bool Font::Update()
	{
		if (pendingGlyphs_.empty())
		{
			return false;
		}

		DynamicArray<msdf_atlas::GlyphGeometry> geometries;
		geometries.reserve(pendingGlyphs_.size());

		for (Uint32 glyphIndex : pendingGlyphs_)
		{
			msdf_atlas::GlyphGeometry geometry;
			if (!geometry.load(msdfFont_, geometryScale_ / static_cast<Double>(ftFace_->units_per_EM), msdfgen::GlyphIndex(glyphIndex)))
			{
				continue;
			}

			geometry.edgeColoring(&msdfgen::edgeColoringInkTrap, 3.0, 0);
			geometry.wrapBox(1.0, pixelRange_, 1.0);

			geometries.emplace_back(std::move(geometry));
		}
		pendingGlyphs_.clear();

		if (geometries.empty())
		{
			return false;
		}

		atlas_.add(geometries.data(), static_cast<Int>(geometries.size()), false);

		for (const msdf_atlas::GlyphGeometry& geometry : geometries)
		{
			GlyphAtlasEntry entry = {};
			entry.isWhitespace_ = geometry.isWhitespace();

			Double l = 0.0, b = 0.0, r = 0.0, t = 0.0;
			geometry.getQuadAtlasBounds(l, b, r, t);
			entry.atlasLeft_ = static_cast<Float>(l);
			entry.atlasBottom_ = static_cast<Float>(b);
			entry.atlasRight_ = static_cast<Float>(r);
			entry.atlasTop_ = static_cast<Float>(t);

			geometry.getQuadPlaneBounds(l, b, r, t);
			entry.planeLeft_ = static_cast<Float>(l);
			entry.planeBottom_ = static_cast<Float>(b);
			entry.planeRight_ = static_cast<Float>(r);
			entry.planeTop_ = static_cast<Float>(t);

			glyphs_[static_cast<Uint32>(geometry.getIndex())] = entry;
		}

		atlasDirty_ = true;

		return true;
	}

	DynamicArray<ShapedGlyph> Font::Shape(const std::string& utf8Text)
	{
		DynamicArray<ShapedGlyph> result;

		if (!hbFont_ || utf8Text.empty())
		{
			return result;
		}

		hb_buffer_t* buffer = hb_buffer_create();
		hb_buffer_add_utf8(buffer, utf8Text.c_str(), static_cast<Int>(utf8Text.size()), 0, static_cast<Int>(utf8Text.size()));
		hb_buffer_guess_segment_properties(buffer);

		hb_shape(hbFont_, buffer, nullptr, 0);

		Uint glyphCount = 0;
		hb_glyph_info_t* infos = hb_buffer_get_glyph_infos(buffer, &glyphCount);
		hb_glyph_position_t* positions = hb_buffer_get_glyph_positions(buffer, &glyphCount);

		constexpr Float toPixel = 1.0f / 64.0f;

		result.reserve(glyphCount);
		for (Uint index = 0; index < glyphCount; ++index)
		{
			ShapedGlyph glyph = {};
			glyph.glyphIndex_ = infos[index].codepoint;
			glyph.cluster_ = infos[index].cluster;
			glyph.advance_.x = static_cast<Float>(positions[index].x_advance) * toPixel;
			glyph.advance_.y = static_cast<Float>(positions[index].y_advance) * toPixel;
			glyph.offset_.x = static_cast<Float>(positions[index].x_offset) * toPixel;
			glyph.offset_.y = static_cast<Float>(positions[index].y_offset) * toPixel;
			result.emplace_back(glyph);

			RequestGlyph(glyph.glyphIndex_);
		}

		hb_buffer_destroy(buffer);

		return result;
	}

	const GlyphAtlasEntry* Font::FindGlyph(Uint32 glyphIndex)const
	{
		auto it = glyphs_.find(glyphIndex);
		return it != glyphs_.end() ? &it->second : nullptr;
	}

	msdfgen::BitmapConstRef<msdf_atlas::byte, 4> Font::GetAtlasBitmap()const
	{
		return atlas_.atlasGenerator().atlasStorage();
	}

	Bool Font::IsAtlasDirty()const
	{
		return atlasDirty_;
	}

	void Font::ClearAtlasDirty()
	{
		atlasDirty_ = false;
	}

	void Font::UploadAtlas(ID3D12Device* device, ID3D12CommandQueue* cmdQueue, BindlessHeap* bindlessHeap)
	{
		if (!atlasDirty_)
		{
			return;
		}

		msdfgen::BitmapConstRef<msdf_atlas::byte, 4> bitmap = GetAtlasBitmap();
		if (bitmap.width <= 0 || bitmap.height <= 0)
		{
			return;
		}

		if (!atlasTextureIndexAllocated_)
		{
			atlasTextureIndex_ = bindlessHeap->AllocateIndex();
			atlasTextureIndexAllocated_ = true;
		}

		if (!atlasTexture_ || atlasTextureWidth_ != bitmap.width || atlasTextureHeight_ != bitmap.height)
		{
			D3D12_HEAP_PROPERTIES heapProperties{};
			heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

			D3D12_RESOURCE_DESC resourceDesc{};
			resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
			resourceDesc.Width = static_cast<Uint64>(bitmap.width);
			resourceDesc.Height = static_cast<Uint>(bitmap.height);
			resourceDesc.DepthOrArraySize = 1;
			resourceDesc.MipLevels = 1;
			resourceDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			resourceDesc.SampleDesc.Count = 1;
			resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

			atlasTexture_.Reset();
			if (FAILED(device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(atlasTexture_.ReleaseAndGetAddressOf()))))
			{
				return;
			}

			atlasTextureWidth_ = bitmap.width;
			atlasTextureHeight_ = bitmap.height;

			D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
			srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srvDesc.Texture2D.MipLevels = 1;
			device->CreateShaderResourceView(atlasTexture_.Get(), &srvDesc, bindlessHeap->CPUHandle(atlasTextureIndex_));
		}
		else
		{
			DirectX::ResourceUploadBatch transition(device);
			transition.Begin();
			transition.Transition(atlasTexture_.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST);
			transition.End(cmdQueue).wait();
		}

		D3D12_SUBRESOURCE_DATA subresource{};
		subresource.pData = bitmap.pixels;
		subresource.RowPitch = static_cast<Longlong>(bitmap.width) * 4;
		subresource.SlicePitch = subresource.RowPitch * bitmap.height;

		DirectX::ResourceUploadBatch upload(device);
		upload.Begin();
		upload.Upload(atlasTexture_.Get(), 0, &subresource, 1);
		upload.Transition(atlasTexture_.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		upload.End(cmdQueue).wait();

		ClearAtlasDirty();
	}

	Uint Font::GetAtlasTextureIndex()const
	{
		return atlasTextureIndex_;
	}

	Bool Font::HasAtlasTexture()const
	{
		return atlasTexture_ != nullptr;
	}

	Float Font::GetPixelRange()const
	{
		return static_cast<Float>(pixelRange_);
	}

	Float Font::GetFontSize()const
	{
		return fontSize_;
	}

	Float Font::GetLineHeight()const 
	{
		return lineHeight_;
	}

	Float Font::GetAscender()const
	{
		return ascender_;
	}

	Float Font::GetDescender()const
	{
		return descender_;
	}

	void Font::RequestGlyph(Uint32 glyphIndex)
	{
		if (glyphs_.contains(glyphIndex))
		{
			return;
		}

		if (std::ranges::find(pendingGlyphs_, glyphIndex) == pendingGlyphs_.end())
		{
			pendingGlyphs_.emplace_back(glyphIndex);
		}
	}
}
