#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/Utility/FlatMap.h>

namespace SeedCore
{
	/**
	* [EN]
	* Global registry mapping tag names to stable bit indices.
	* Any Actor can hold any number of tags by setting the
	* corresponding bit in its own Bitset. Removing a tag never
	* reassigns or shifts existing bit indices; the slot is just
	* marked as removed so other actors' Bitsets stay valid.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* タグ名とビットインデックスを対応付けるグローバルレジストリ。
	* 各 Actor は自身の Bitset に対応するビットを立てることで、
	* 個数・種類を問わず任意のタグを保持できる。タグを削除しても
	* 既存のビットインデックスはずらさず、削除済みフラグを立てるだけ
	* にすることで、他の Actor の Bitset を壊さないようにしている。
	*/
	class SEEDCORE_API TagRegistry
	{
	public:
		/// [EN] Sentinel returned by Find() when a tag has not been registered.
		/// [JP] タグが未登録の場合に Find() が返す番兵値。
		static constexpr Size InvalidIndex = static_cast<Size>(-1);

		/**
		* [EN]
		* Returns tag's existing bit index, registering a new one if tag
		* has not been seen before.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* tag の既存のビットインデックスを返す。tag がまだ登録されて
		* いなければ、新しく登録する。
		*/
		static Size GetOrCreate(String tag);

		/**
		* [EN]
		* Returns tag's bit index, or InvalidIndex if tag has not been registered.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* tag のビットインデックスを返す。tag が未登録であれば
		* InvalidIndex を返す。
		*/
		static Size Find(String tag);

		/**
		* [EN]
		* Marks tag as removed; its bit index is not reassigned or
		* reused, so existing Actor Bitsets remain valid.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* tag を削除済みとしてマークする。そのビットインデックスは
		* 再割り当て・再利用されないため、既存の Actor の Bitset は
		* 有効なまま維持される。
		*/
		static void Remove(String tag);

		/**
		* [EN]
		* Returns every registered tag name, indexed by bit index
		* (including removed tags' now-unused slots).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 登録済みの全タグ名を、ビットインデックスでアクセスできる形で
		* 返す（削除済みタグの、現在は使われていないスロットも含む）。
		*/
		static const DynamicArray<String>& GetNames();

		/**
		* [EN]
		* Returns whether the tag at the given bit index has been removed.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 指定されたビットインデックスのタグが削除済みかどうかを返す。
		*/
		static Bool IsRemoved(Size index);

	private:
		/// [EN] Maps registered tag names to their bit index.
		/// [JP] 登録済みのタグ名をそのビットインデックスへ対応付ける。
		static inline FlatMap<String, Size> tagToIndex_;

		/// [EN] Tag names indexed by bit index (parallel to removed_).
		/// [JP] ビットインデックスでアクセスできるタグ名一覧（removed_ と対応する）。
		static inline DynamicArray<String> names_;

		/// [EN] Removed-flag per bit index, parallel to names_.
		/// [JP] ビットインデックスごとの削除済みフラグ。names_ と対応する。
		static inline DynamicArray<Bool> removed_;
	};
}
