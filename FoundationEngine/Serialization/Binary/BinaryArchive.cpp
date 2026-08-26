#include <FoundationEngine/Serialization/Binary/BinaryArchive.h>
#include <FoundationEngine/Serialization/Encryption/Aes256.h>
#include <FoundationEngine/Serialization/Encryption/Sha256.h>

namespace SeedCore
{
	BinaryField::BinaryField(const Char* name)
	{
		Uint32 hash = 2166136261u;
		for (const Char* pointer = name; *pointer != '\0'; ++pointer)
		{
			hash ^= static_cast<Uint8>(*pointer);
			hash *= 16777619u;
		}
		value_ = hash;
	}

	Uint32 BinaryField::GetValue()const
	{
		return value_;
	}

	void BinaryOutputArchive::WriteUint32(DynamicArray<Byte>& buffer, Uint32 value)
	{
		Byte bytes[sizeof(Uint32)];
		std::memcpy(bytes, &value, sizeof(Uint32));
		buffer.insert(buffer.end(), bytes, bytes + sizeof(Uint32));
	}

	Bool BinaryOutputArchive::Write(const String& filePath)const
	{
		std::ofstream ofs(filePath.c_str(), std::ios::binary);
		if (!ofs.is_open())
		{
			SC_LOG_WARNING("\"{}\" を書き込み用に開けませんでした。", filePath.c_str());
			return false;
		}

		static const DynamicArray<Byte> key = Sha256::Hash(reinterpret_cast<const Byte*>(SC_ENCRYPTION_KEY_SEED), std::strlen(SC_ENCRYPTION_KEY_SEED));

		DynamicArray<Byte> iv(16);
		std::random_device randomDevice;
		for (Byte& ivByte : iv)
		{
			ivByte = static_cast<Byte>(randomDevice());
		}

		DynamicArray<Byte> ciphertext = Aes256::Encrypt(key, iv, body_);

		ofs.write(iv.data(), iv.size());
		ofs.write(ciphertext.data(), ciphertext.size());
		return true;
	}

	BinaryInputArchive::BinaryInputArchive(std::span<const Byte> buffer) : data_(buffer)
	{
		Size offset = 0;
		while (offset + sizeof(Uint32) * 2 <= data_.size())
		{
			Uint32 id = ReadUint32(data_, offset);
			offset += sizeof(Uint32);

			Uint32 size = ReadUint32(data_, offset);
			offset += sizeof(Uint32);

			if (offset + size > data_.size())
			{
				SC_LOG_WARNING("バイナリレコードのサイズが不正です。");
				break;
			}

			index_[id] = { offset, size };
			offset += size;
		}
	}

	Bool BinaryInputArchive::Has(const Char* name)const
	{
		return index_.contains(BinaryField(name).GetValue());
	}

	Bool BinaryInputArchive::Read(const String& filePath)
	{
		std::ifstream ifs(filePath.c_str(), std::ios::binary);
		if (!ifs.is_open())
		{
			SC_LOG_WARNING("\"{}\" を読み込み用に開けませんでした。", filePath.c_str());
			return false;
		}

		ifs.seekg(0, std::ios::end);
		Size fileSize = static_cast<Size>(ifs.tellg());
		ifs.seekg(0, std::ios::beg);

		DynamicArray<Byte> rawData(fileSize);
		if (fileSize > 0 && !ifs.read(rawData.data(), fileSize))
		{
			SC_LOG_WARNING("\"{}\" の読み込みに失敗しました。", filePath.c_str());
			return false;
		}

		if (rawData.size() < 16)
		{
			SC_LOG_WARNING("\"{}\" の暗号化データが不正です。", filePath.c_str());
			return false;
		}

		static const DynamicArray<Byte> key = Sha256::Hash(reinterpret_cast<const Byte*>(SC_ENCRYPTION_KEY_SEED), std::strlen(SC_ENCRYPTION_KEY_SEED));

		DynamicArray<Byte> iv(rawData.begin(), rawData.begin() + 16);
		DynamicArray<Byte> ciphertext(rawData.begin() + 16, rawData.end());

		ownedData_ = Aes256::Decrypt(key, iv, ciphertext);
		if (ownedData_.empty() && !ciphertext.empty())
		{
			SC_LOG_WARNING("\"{}\" の復号に失敗しました。", filePath.c_str());
			return false;
		}

		data_ = std::span<const Byte>(ownedData_);
		index_.clear();

		Size offset = 0;
		while (offset + sizeof(Uint32) * 2 <= data_.size())
		{
			Uint32 id = ReadUint32(data_, offset);
			offset += sizeof(Uint32);

			Uint32 size = ReadUint32(data_, offset);
			offset += sizeof(Uint32);

			if (offset + size > data_.size())
			{
				SC_LOG_WARNING("バイナリレコードのサイズが不正です。");
				break;
			}

			index_[id] = { offset, size };
			offset += size;
		}

		return true;
	}

	Uint32 BinaryInputArchive::ReadUint32(std::span<const Byte> buffer, Size offset)
	{
		Uint32 value = 0;
		std::memcpy(&value, buffer.data() + offset, sizeof(Uint32));
		return value;
	}
}
