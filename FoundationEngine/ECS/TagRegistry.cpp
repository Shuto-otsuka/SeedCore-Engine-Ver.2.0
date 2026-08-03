#include <FoundationEngine/ECS/TagRegistry.h>

namespace SeedCore
{
	/**
	* [EN]
	* Returns tag's existing bit index, registering a new one if tag has
	* not been seen before.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* tag の既存のビットインデックスを返す。tag がまだ登録されていなければ、
	* 新しく登録する。
	*/
	Size TagRegistry::GetOrCreate(String tag)
	{
		auto it = tagToIndex_.find(tag);
		if (it != tagToIndex_.end())
		{
			return it->second;
		}

		/// [EN] New tag: append it to the end of names_/removed_ (parallel arrays), assigning the next free bit index.
		/// [JP] 新規タグ: names_/removed_（対応する配列）の末尾へ追加し、次の空きビットインデックスを割り当てる。
		Size index = names_.size();
		names_.push_back(tag);
		removed_.push_back(false);
		tagToIndex_[tag] = index;
		return index;
	}

	/**
	* [EN]
	* Returns tag's bit index, or InvalidIndex if tag has not been registered.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* tag のビットインデックスを返す。tag が未登録であれば InvalidIndex
	* を返す。
	*/
	Size TagRegistry::Find(String tag)
	{
		auto it = tagToIndex_.find(tag);
		if (it != tagToIndex_.end())
		{
			return it->second;
		}
		return InvalidIndex;
	}

	/**
	* [EN]
	* Marks tag as removed; its bit index is not reassigned or reused,
	* so existing Actor Bitsets remain valid.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* tag を削除済みとしてマークする。そのビットインデックスは再割り当て・
	* 再利用されないため、既存の Actor の Bitset は有効なまま維持される。
	*/
	void TagRegistry::Remove(String tag)
	{
		auto it = tagToIndex_.find(tag);
		if (it == tagToIndex_.end())
		{
			return;
		}

		removed_[it->second] = true;
		tagToIndex_.erase(it);
	}

	/**
	* [EN]
	* Returns every registered tag name, indexed by bit index (including
	* removed tags' now-unused slots).
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 登録済みの全タグ名を、ビットインデックスでアクセスできる形で返す
	* （削除済みタグの、現在は使われていないスロットも含む）。
	*/
	const DynamicArray<String>& TagRegistry::GetNames()
	{
		return names_;
	}

	/**
	* [EN]
	* Returns whether the tag at the given bit index has been removed.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 指定されたビットインデックスのタグが削除済みかどうかを返す。
	*/
	Bool TagRegistry::IsRemoved(Size index)
	{
		if (index >= removed_.size())
		{
			return true;
		}
		return removed_[index];
	}
}
