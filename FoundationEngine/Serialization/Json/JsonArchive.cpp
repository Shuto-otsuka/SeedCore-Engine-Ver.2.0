#include <FoundationEngine/Serialization/Json/JsonArchive.h>

namespace SeedCore
{
	Bool JsonOutputArchive::Write(const String& filePath)const
	{
		std::ofstream ofs(filePath.c_str());
		if (!ofs.is_open())
		{
			SC_LOG_WARNING("\"{}\" を書き込み用に開けませんでした。", filePath.c_str());
			return false;
		}

		ofs << node_.dump(1, '\t');
		return true;
	}

	String JsonOutputArchive::Dump()const
	{
		return String(node_.dump(1, '\t'));
	}

	JsonInputArchive::JsonInputArchive(const nlohmann::json& node) : node_(&node)
	{
		/// No Code
	}

	Bool JsonInputArchive::Has(const Char* name)const
	{
		return node_->contains(name);
	}

	Bool JsonInputArchive::Read(const String& filePath)
	{
		std::ifstream ifs(filePath.c_str());
		if (!ifs.is_open())
		{
			SC_LOG_WARNING("\"{}\" を読み込み用に開けませんでした。", filePath.c_str());
			return false;
		}

		ownedNode_ = nlohmann::json::parse(ifs, nullptr, false);
		if (ownedNode_.is_discarded())
		{
			SC_LOG_WARNING("\"{}\" のjsonとしての解析に失敗しました。", filePath.c_str());
			return false;
		}

		node_ = &ownedNode_;
		return true;
	}

	Bool JsonInputArchive::Parse(const String& json)
	{
		ownedNode_ = nlohmann::json::parse(json.str(), nullptr, false);
		if (ownedNode_.is_discarded())
		{
			SC_LOG_WARNING("jsonとしての解析に失敗しました。");
			return false;
		}

		node_ = &ownedNode_;
		return true;
	}
}
