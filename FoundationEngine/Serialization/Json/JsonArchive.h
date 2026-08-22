#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/Log/Warning.h>
#include <FoundationEngine/Serialization/SerializeSupport.h>

namespace SeedCore
{
	template<typename T>
	concept JsonSequenceContainer = requires(T& container)
	{
		typename T::value_type;
		container.begin();
		container.end();
		container.push_back(std::declval<typename T::value_type>());
	} && !std::same_as<T, String> && !std::same_as<T, std::string>;

	template<typename T>
	concept AssociativeContainer = requires(T& container)
	{
		typename T::key_type;
		typename T::mapped_type;
		container.begin();
		container.end();
		container.clear();
		container.emplace(std::declval<typename T::key_type>(), std::declval<typename T::mapped_type>());
	};

	template<typename T>
	concept FixedSizeArray = requires(T& container)
	{
		typename T::value_type;
		container.begin();
		container.end();
		requires std::tuple_size<T>::value >= 0;
	};

	class SEEDCORE_API JsonOutputArchive
	{
	public:
		JsonOutputArchive() = default;

		template<typename T>
		void Field(const Char* name, const T& value)
		{
			WriteInto(node_[name], value);
		}

		template<typename T>
		Bool TryField(const Char* name, const T& value)
		{
			Field(name, value);
			return true;
		}

		Bool Write(const String& filePath)const;

		String Dump()const;

	private:
		template<typename T>
		void WriteInto(nlohmann::json& target, const T& value)
		{
			if constexpr (requires { value.Save(*this); })
			{
				JsonOutputArchive child;
				value.Save(child);
				target = std::move(child.node_);
			}
			else if constexpr (requires(T& mutableValue) { mutableValue.Serialize(*this); })
			{
				JsonOutputArchive child;
				const_cast<T&>(value).Serialize(child);
				target = std::move(child.node_);
			}
			else if constexpr (requires { Save(*this, value); })
			{
				JsonOutputArchive child;
				Save(child, value);
				target = std::move(child.node_);
			}
			else if constexpr (JsonSequenceContainer<T>)
			{
				target = nlohmann::json::array();
				for (const auto& element : value)
				{
					nlohmann::json elementNode;
					WriteInto(elementNode, element);
					target.push_back(std::move(elementNode));
				}
			}
			else if constexpr (AssociativeContainer<T>)
			{
				target = nlohmann::json::array();
				for (const auto& entry : value)
				{
					nlohmann::json entryNode;
					WriteInto(entryNode["key"], entry.first);
					WriteInto(entryNode["value"], entry.second);
					target.push_back(std::move(entryNode));
				}
			}
			else if constexpr (FixedSizeArray<T>)
			{
				target = nlohmann::json::array();
				for (const auto& element : value)
				{
					nlohmann::json elementNode;
					WriteInto(elementNode, element);
					target.push_back(std::move(elementNode));
				}
			}
			else if constexpr (std::is_array_v<T>)
			{
				target = nlohmann::json::array();
				constexpr Size elementCount = std::extent_v<T>;
				for (Size elementIndex = 0; elementIndex < elementCount; ++elementIndex)
				{
					nlohmann::json elementNode;
					WriteInto(elementNode, value[elementIndex]);
					target.push_back(std::move(elementNode));
				}
			}
			else if constexpr (std::same_as<T, String>)
			{
				target = value.str();
			}
			else
			{
				target = value;
			}
		}

		nlohmann::json node_;

		friend class JsonInputArchive;
	};

	class SEEDCORE_API JsonInputArchive
	{
	public:
		JsonInputArchive() = default;

		template<typename T>
		void Field(const Char* name, T& value)
		{
			if (!node_->contains(name))
			{
				SC_LOG_WARNING("jsonフィールド \"{}\" が見つかりません。", name);
				return;
			}
			ReadFrom(node_->at(name), value);
		}

		template<typename T>
		Bool TryField(const Char* name, T& value)
		{
			if (!node_->contains(name))
			{
				return false;
			}
			ReadFrom(node_->at(name), value);
			return true;
		}

		Bool Has(const Char* name)const;

		Bool Read(const String& filePath);

		Bool Parse(const String& json);

	private:
		explicit JsonInputArchive(const nlohmann::json& node);

		template<typename T>
		void ReadFrom(const nlohmann::json& source, T& value)
		{
			if constexpr (requires { value.Load(*this); })
			{
				JsonInputArchive child(source);
				value.Load(child);
			}
			else if constexpr (requires { value.Serialize(*this); })
			{
				JsonInputArchive child(source);
				value.Serialize(child);
			}
			else if constexpr (requires { Load(*this, value); })
			{
				JsonInputArchive child(source);
				Load(child, value);
			}
			else if constexpr (JsonSequenceContainer<T>)
			{
				value.clear();
				if (source.is_null())
				{
					return;
				}
				if (!source.is_array())
				{
					SC_LOG_WARNING("json配列が期待されていましたが異なる型でした。初期値を使用します。");
					return;
				}
				for (const nlohmann::json& elementNode : source)
				{
					typename T::value_type element{};
					ReadFrom(elementNode, element);
					value.push_back(std::move(element));
				}
			}
			else if constexpr (AssociativeContainer<T>)
			{
				value.clear();
				if (source.is_null())
				{
					return;
				}
				if (!source.is_array())
				{
					SC_LOG_WARNING("json配列（マップ）が期待されていましたが異なる型でした。初期値を使用します。");
					return;
				}
				for (const nlohmann::json& entryNode : source)
				{
					typename T::key_type key{};
					typename T::mapped_type mapped{};
					if (entryNode.contains("key"))
					{
						ReadFrom(entryNode.at("key"), key);
					}
					if (entryNode.contains("value"))
					{
						ReadFrom(entryNode.at("value"), mapped);
					}
					value.emplace(std::move(key), std::move(mapped));
				}
			}
			else if constexpr (FixedSizeArray<T>)
			{
				if (!source.is_array())
				{
					SC_LOG_WARNING("json配列が期待されていましたが異なる型でした。初期値を使用します。");
					return;
				}
				Size elementIndex = 0;
				for (const nlohmann::json& elementNode : source)
				{
					if (elementIndex >= std::tuple_size<T>::value)
					{
						SC_LOG_WARNING("json配列の要素数が固定長配列より多いため、超過分を無視します。");
						break;
					}
					ReadFrom(elementNode, value[elementIndex]);
					++elementIndex;
				}
			}
			else if constexpr (std::is_array_v<T>)
			{
				if (!source.is_array())
				{
					SC_LOG_WARNING("json配列が期待されていましたが異なる型でした。初期値を使用します。");
					return;
				}
				constexpr Size elementCount = std::extent_v<T>;
				Size elementIndex = 0;
				for (const nlohmann::json& elementNode : source)
				{
					if (elementIndex >= elementCount)
					{
						SC_LOG_WARNING("json配列の要素数が固定長配列より多いため、超過分を無視します。");
						break;
					}
					ReadFrom(elementNode, value[elementIndex]);
					++elementIndex;
				}
			}
			else if constexpr (std::same_as<T, String>)
			{
				try
				{
					value = String(std::string_view(source.get<std::string>()));
				}
				catch (const nlohmann::json::exception&)
				{
					SC_LOG_WARNING("jsonフィールドの解析に失敗しました。初期値を使用します。");
				}
			}
			else
			{
				try
				{
					value = source.get<T>();
				}
				catch (const nlohmann::json::exception&)
				{
					SC_LOG_WARNING("jsonフィールドの解析に失敗しました。初期値を使用します。");
				}
			}
		}

		nlohmann::json ownedNode_;
		const nlohmann::json* node_ = &ownedNode_;
	};
}
