#include <GraphicsEngine/Sky/SkymapCache.h>
#include <FoundationEngine/Log/Error.h>

namespace SeedCore
{
	Bool WriteSkymapCache(const String& filePath, const SkymapCacheHeader& header, const void* pixels)
	{
		std::ofstream stream(filePath.str(), std::ios::binary | std::ios::trunc);
		if (!stream)
		{
			SC_LOG_ERROR("スカイマップキャッシュの書き出しに失敗しました: {}", filePath.str());
			return false;
		}

		stream.write(reinterpret_cast<const Byte*>(&header), sizeof(SkymapCacheHeader));
		if (header.dataSize_ > 0 && pixels != nullptr)
		{
			stream.write(reinterpret_cast<const Byte*>(pixels), static_cast<std::streamsize>(header.dataSize_));
		}

		return static_cast<Bool>(stream);
	}

	Bool ReadSkymapCache(const String& filePath, SkymapCacheHeader& header, DynamicArray<Uint8>& pixels)
	{
		std::ifstream stream(filePath.str(), std::ios::binary);
		if (!stream)
		{
			return false;
		}

		stream.read(reinterpret_cast<Byte*>(&header), sizeof(SkymapCacheHeader));
		if (!stream || stream.gcount() != static_cast<std::streamsize>(sizeof(SkymapCacheHeader)))
		{
			return false;
		}

		if (std::memcmp(header.magic_, skymapCacheMagic_, sizeof(header.magic_)) != 0)
		{
			return false;
		}
		if (header.version_ != skymapCacheVersion_)
		{
			return false;
		}
		if (header.dataSize_ == 0)
		{
			return false;
		}

		pixels.resize(header.dataSize_);
		stream.read(reinterpret_cast<Byte*>(pixels.data()), static_cast<std::streamsize>(header.dataSize_));
		if (!stream || stream.gcount() != static_cast<std::streamsize>(header.dataSize_))
		{
			return false;
		}

		return true;
	}
}
