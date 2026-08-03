#include <GraphicsEngine/Effect/Effekseer/EffekseerFileInterface.h>

namespace SeedCore
{
	EffekseerMemoryFileReader::EffekseerMemoryFileReader(DynamicArray<Byte> data) :data_(std::move(data))
	{
		/// No Code
	}

	Size EffekseerMemoryFileReader::Read(void* buffer, Size size)
	{
		Size remaining = data_.size() > static_cast<Size>(position_) ? data_.size() - static_cast<Size>(position_) : 0;
		Size readSize = size < remaining ? size : remaining;

		if (readSize > 0)
		{
			std::memcpy(buffer, data_.data() + position_, readSize);
			position_ += static_cast<Int>(readSize);
		}

		return readSize;
	}

	void EffekseerMemoryFileReader::Seek(Int position)
	{
		position_ = position;
	}

	Int EffekseerMemoryFileReader::GetPosition()const
	{
		return position_;
	}

	Size EffekseerMemoryFileReader::GetLength()const
	{
		return data_.size();
	}

	void EffekseerFileInterface::Register(const std::u16string& path, DynamicArray<Byte> data)
	{
		registry_.insert({ path, std::move(data) });
	}

	void EffekseerFileInterface::Clear()
	{
		registry_.clear();
	}

	Effekseer::FileReaderRef EffekseerFileInterface::OpenRead(const Char16* path)
	{
		std::u16string key(path);
		if (registry_.contains(key))
		{
			return Effekseer::MakeRefPtr<EffekseerMemoryFileReader>(registry_.at(key));
		}

		return diskFallback_.OpenRead(path);
	}

	Effekseer::FileWriterRef EffekseerFileInterface::OpenWrite(const Char16* path)
	{
		return diskFallback_.OpenWrite(path);
	}
}
