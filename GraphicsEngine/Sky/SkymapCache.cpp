#include <GraphicsEngine/Sky/SkymapCache.h>
#include <FoundationEngine/Log/Error.h>
#include <FoundationEngine/Serialization/Binary/BinaryArchive.h>

namespace SeedCore
{
	Bool WriteSkymapCache(const String& filePath, const SkymapCacheHeader& header, const void* pixels)
	{
		BinaryOutputArchive archive;
		archive.Field("format", header.format_);
		archive.Field("width", header.width_);
		archive.Field("height", header.height_);
		archive.Field("rowPitch", header.rowPitch_);

		DynamicArray<Uint8> pixelBytes(header.dataSize_);
		if (header.dataSize_ > 0 && pixels != nullptr)
		{
			std::memcpy(pixelBytes.data(), pixels, header.dataSize_);
		}
		archive.Field("pixels", pixelBytes);

		if (!archive.Write(filePath))
		{
			SC_LOG_ERROR("スカイマップキャッシュの書き出しに失敗しました: {}", filePath.str());
			return false;
		}
		return true;
	}

	Bool ReadSkymapCache(const String& filePath, SkymapCacheHeader& header, DynamicArray<Uint8>& pixels)
	{
		BinaryInputArchive archive;
		if (!archive.Read(filePath))
		{
			return false;
		}

		archive.TryField("format", header.format_);
		archive.TryField("width", header.width_);
		archive.TryField("height", header.height_);
		archive.TryField("rowPitch", header.rowPitch_);

		DynamicArray<Uint8> pixelBytes;
		archive.TryField("pixels", pixelBytes);
		if (pixelBytes.empty())
		{
			return false;
		}

		header.dataSize_ = static_cast<Uint32>(pixelBytes.size());
		pixels = std::move(pixelBytes);

		return true;
	}
}
