#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/Log/Warning.h>
#include <FoundationEngine/Serialization/SerializeSupport.h>

namespace SeedCore
{
	template<typename T>
	concept BinarySequenceContainer = requires(T& container)
	{
		typename T::value_type;
		container.begin();
		container.end();
		container.push_back(std::declval<typename T::value_type>());
	} && !std::same_as<T, String> && !std::same_as<T, std::string> && !std::same_as<T, std::u16string>;

	template<typename T>
	concept BinaryAssociativeContainer = requires(T& container)
	{
		typename T::key_type;
		typename T::mapped_type;
		container.begin();
		container.end();
		container.clear();
		container.emplace(std::declval<typename T::key_type>(), std::declval<typename T::mapped_type>());
	};

	template<typename T>
	concept BinaryFixedSizeArray = requires(T& container)
	{
		typename T::value_type;
		container.begin();
		container.end();
		requires std::tuple_size<T>::value >= 0;
	};

	class SEEDCORE_API BinaryField
	{
	public:
		explicit BinaryField(const Char* name);

		Uint32 GetValue()const;

	private:
		Uint32 value_ = 0;
	};

	class SEEDCORE_API BinaryOutputArchive
	{
	public:
		BinaryOutputArchive() = default;

		template<typename T>
		void Field(const Char* name, const T& value)
		{
			DynamicArray<Byte> payload;
			WriteInto(payload, value);

			WriteUint32(body_, BinaryField(name).GetValue());
			WriteUint32(body_, static_cast<Uint32>(payload.size()));
			body_.insert(body_.end(), payload.begin(), payload.end());
		}

		template<typename T>
		Bool TryField(const Char* name, const T& value)
		{
			Field(name, value);
			return true;
		}

		Bool Write(const String& filePath)const;

	private:
		template<typename T>
		void WriteInto(DynamicArray<Byte>& payload, const T& value)
		{
			if constexpr (requires { value.Save(*this); })
			{
				BinaryOutputArchive child;
				value.Save(child);
				payload = std::move(child.body_);
			}
			else if constexpr (requires(T& mutableValue) { mutableValue.Serialize(*this); })
			{
				BinaryOutputArchive child;
				const_cast<T&>(value).Serialize(child);
				payload = std::move(child.body_);
			}
			else if constexpr (requires { Save(*this, value); })
			{
				BinaryOutputArchive child;
				Save(child, value);
				payload = std::move(child.body_);
			}
			else if constexpr (BinarySequenceContainer<T>)
			{
				using ElementType = typename T::value_type;

				if constexpr (std::is_trivially_copyable_v<ElementType> && !std::same_as<ElementType, Bool> && requires { value.data(); })
				{
					WriteUint32(payload, static_cast<Uint32>(value.size()));
					Size byteSize = value.size() * sizeof(ElementType);
					Size offset = payload.size();
					payload.resize(offset + byteSize);
					std::memcpy(payload.data() + offset, value.data(), byteSize);
				}
				else
				{
					WriteUint32(payload, static_cast<Uint32>(value.size()));
					for (const auto& element : value)
					{
						DynamicArray<Byte> elementPayload;
						WriteInto(elementPayload, element);
						WriteUint32(payload, static_cast<Uint32>(elementPayload.size()));
						payload.insert(payload.end(), elementPayload.begin(), elementPayload.end());
					}
				}
			}
			else if constexpr (BinaryAssociativeContainer<T>)
			{
				WriteUint32(payload, static_cast<Uint32>(value.size()));
				for (const auto& entry : value)
				{
					BinaryOutputArchive entryArchive;
					entryArchive.Field("key", entry.first);
					entryArchive.Field("value", entry.second);

					WriteUint32(payload, static_cast<Uint32>(entryArchive.body_.size()));
					payload.insert(payload.end(), entryArchive.body_.begin(), entryArchive.body_.end());
				}
			}
			else if constexpr (BinaryFixedSizeArray<T>)
			{
				WriteUint32(payload, static_cast<Uint32>(value.size()));
				for (const auto& element : value)
				{
					DynamicArray<Byte> elementPayload;
					WriteInto(elementPayload, element);
					WriteUint32(payload, static_cast<Uint32>(elementPayload.size()));
					payload.insert(payload.end(), elementPayload.begin(), elementPayload.end());
				}
			}
			else if constexpr (std::same_as<T, String>)
			{
				std::string raw = value.str();
				payload.assign(raw.begin(), raw.end());
			}
			else if constexpr (std::same_as<T, std::string>)
			{
				payload.assign(value.begin(), value.end());
			}
			else if constexpr (std::same_as<T, std::u16string>)
			{
				payload.resize(value.size() * sizeof(Char16));
				std::memcpy(payload.data(), value.data(), payload.size());
			}
			else if constexpr (std::is_trivially_copyable_v<T>)
			{
				payload.resize(sizeof(T));
				std::memcpy(payload.data(), &value, sizeof(T));
			}
			else
			{
				static_assert(sizeof(T) == 0, "BinaryOutputArchive: no serialization support for this type, add a free Save/Load overload");
			}
		}

		void WriteUint32(DynamicArray<Byte>& buffer, Uint32 value);

	private:
		DynamicArray<Byte> body_;

		friend class BinaryInputArchive;
	};

	class SEEDCORE_API BinaryInputArchive
	{
	public:
		BinaryInputArchive() = default;

		template<typename T>
		void Field(const Char* name, T& value)
		{
			auto entry = index_.find(BinaryField(name).GetValue());
			if (entry == index_.end())
			{
				SC_LOG_WARNING("バイナリフィールド \"{}\" が見つかりません。", name);
				return;
			}
			ReadFrom(data_.subspan(entry->second.first, entry->second.second), value);
		}

		template<typename T>
		Bool TryField(const Char* name, T& value)
		{
			auto entry = index_.find(BinaryField(name).GetValue());
			if (entry == index_.end())
			{
				return false;
			}
			ReadFrom(data_.subspan(entry->second.first, entry->second.second), value);
			return true;
		}

		Bool Has(const Char* name)const;

		Bool Read(const String& filePath);

	private:
		explicit BinaryInputArchive(std::span<const Byte> buffer);

		template<typename T>
		void ReadFrom(std::span<const Byte> payload, T& value)
		{
			if constexpr (requires { value.Load(*this); })
			{
				BinaryInputArchive child(payload);
				value.Load(child);
			}
			else if constexpr (requires { value.Serialize(*this); })
			{
				BinaryInputArchive child(payload);
				value.Serialize(child);
			}
			else if constexpr (requires { Load(*this, value); })
			{
				BinaryInputArchive child(payload);
				Load(child, value);
			}
			else if constexpr (BinarySequenceContainer<T>)
			{
				using ElementType = typename T::value_type;

				if (payload.size() < sizeof(Uint32))
				{
					SC_LOG_WARNING("バイナリ配列のヘッダが不正です。初期値を使用します。");
					return;
				}

				Uint32 elementCount = ReadUint32(payload, 0);

				if constexpr (std::is_trivially_copyable_v<ElementType> && !std::same_as<ElementType, Bool> && requires { value.data(); value.resize(Size(0)); })
				{
					Size expectedBytes = static_cast<Size>(elementCount) * sizeof(ElementType);
					if (payload.size() != sizeof(Uint32) + expectedBytes)
					{
						SC_LOG_WARNING("バイナリ配列のサイズが要素数と一致しません。初期値を使用します。");
						return;
					}
					value.resize(elementCount);
					std::memcpy(value.data(), payload.data() + sizeof(Uint32), expectedBytes);
				}
				else
				{
					Size offset = sizeof(Uint32);

					value.clear();
					for (Uint32 elementIndex = 0; elementIndex < elementCount; ++elementIndex)
					{
						if (offset + sizeof(Uint32) > payload.size())
						{
							SC_LOG_WARNING("バイナリ配列の要素ヘッダが不正です。読み込みを打ち切ります。");
							break;
						}

						Uint32 elementSize = ReadUint32(payload, offset);
						offset += sizeof(Uint32);

						if (offset + elementSize > payload.size())
						{
							SC_LOG_WARNING("バイナリ配列の要素サイズが不正です。読み込みを打ち切ります。");
							break;
						}

						typename T::value_type element{};
						ReadFrom(payload.subspan(offset, elementSize), element);
						value.push_back(std::move(element));
						offset += elementSize;
					}
				}
			}
			else if constexpr (BinaryAssociativeContainer<T>)
			{
				if (payload.size() < sizeof(Uint32))
				{
					SC_LOG_WARNING("バイナリマップのヘッダが不正です。初期値を使用します。");
					return;
				}

				Uint32 entryCount = ReadUint32(payload, 0);
				Size offset = sizeof(Uint32);

				value.clear();
				for (Uint32 entryIndex = 0; entryIndex < entryCount; ++entryIndex)
				{
					if (offset + sizeof(Uint32) > payload.size())
					{
						SC_LOG_WARNING("バイナリマップの要素ヘッダが不正です。読み込みを打ち切ります。");
						break;
					}

					Uint32 entrySize = ReadUint32(payload, offset);
					offset += sizeof(Uint32);

					if (offset + entrySize > payload.size())
					{
						SC_LOG_WARNING("バイナリマップの要素サイズが不正です。読み込みを打ち切ります。");
						break;
					}

					BinaryInputArchive entryArchive(payload.subspan(offset, entrySize));
					offset += entrySize;

					typename T::key_type key{};
					typename T::mapped_type mapped{};
					entryArchive.TryField("key", key);
					entryArchive.TryField("value", mapped);
					value.emplace(std::move(key), std::move(mapped));
				}
			}
			else if constexpr (BinaryFixedSizeArray<T>)
			{
				if (payload.size() < sizeof(Uint32))
				{
					SC_LOG_WARNING("バイナリ配列のヘッダが不正です。初期値を使用します。");
					return;
				}

				Uint32 elementCount = ReadUint32(payload, 0);
				Size offset = sizeof(Uint32);

				Size elementIndex = 0;
				for (Uint32 sourceIndex = 0; sourceIndex < elementCount; ++sourceIndex)
				{
					if (offset + sizeof(Uint32) > payload.size())
					{
						SC_LOG_WARNING("バイナリ配列の要素ヘッダが不正です。読み込みを打ち切ります。");
						break;
					}

					Uint32 elementSize = ReadUint32(payload, offset);
					offset += sizeof(Uint32);

					if (offset + elementSize > payload.size())
					{
						SC_LOG_WARNING("バイナリ配列の要素サイズが不正です。読み込みを打ち切ります。");
						break;
					}

					if (elementIndex >= std::tuple_size<T>::value)
					{
						SC_LOG_WARNING("バイナリ配列の要素数が固定長配列より多いため、超過分を無視します。");
						break;
					}

					ReadFrom(payload.subspan(offset, elementSize), value[elementIndex]);
					offset += elementSize;
					++elementIndex;
				}
			}
			else if constexpr (std::same_as<T, String>)
			{
				value = String(std::string_view(reinterpret_cast<const Char*>(payload.data()), payload.size()));
			}
			else if constexpr (std::same_as<T, std::string>)
			{
				value.assign(reinterpret_cast<const Char*>(payload.data()), payload.size());
			}
			else if constexpr (std::same_as<T, std::u16string>)
			{
				value.assign(reinterpret_cast<const Char16*>(payload.data()), payload.size() / sizeof(Char16));
			}
			else if constexpr (std::is_trivially_copyable_v<T>)
			{
				if (payload.size() != sizeof(T))
				{
					SC_LOG_WARNING("バイナリフィールドのサイズが型と一致しません。初期値を使用します。");
					return;
				}
				std::memcpy(&value, payload.data(), sizeof(T));
			}
			else
			{
				static_assert(sizeof(T) == 0, "BinaryInputArchive: no serialization support for this type, add a free Save/Load overload");
			}
		}

		Uint32 ReadUint32(std::span<const Byte> buffer, Size offset);

	private:
		DynamicArray<Byte> ownedData_;

		std::unordered_map<Uint32, std::pair<Size, Size>> index_;

		std::span<const Byte> data_;
	};
}
