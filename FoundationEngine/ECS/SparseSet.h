#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/ECS/EcsID.h>

namespace SeedCore
{
	/**
	* [EN]
	* Classic sparse-set container mapping EntityID to a densely-packed
	* T, giving O(1) add/remove/lookup while keeping T's storage
	* contiguous for fast iteration. sparse_ maps an entity ID to its
	* index in the dense array (or UINT32_MAX if absent); removal is
	* swap-remove, so Remove() returns whichever entity got moved into
	* the vacated slot (so callers can fix up that entity's index cache).
	* Used as the alternative storage strategy to archetype/chunk
	* storage for components registered with ComponentStorage::SparseSet.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* EntityID を密に詰められた T へ対応付ける、古典的なスパースセット
	* コンテナ。O(1) の追加・削除・検索を提供しつつ、T のストレージを
	* 連続配置にして高速な走査を可能にする。sparse_ はエンティティ ID を
	* 密配列内のインデックスへ対応付ける（存在しなければ UINT32_MAX）。
	* 削除は swap-remove 方式であり、Remove() は空いたスロットへ移動
	* してきたエンティティを返す（呼び出し側がそのエンティティの
	* インデックスキャッシュを修正できるように）。
	* ComponentStorage::SparseSet で登録されたコンポーネント向けの、
	* アーキタイプ/チャンクストレージに代わる格納戦略として使われる。
	*/
	template<typename T>
	class SparseSet
	{
	private:
		/**
		* [EN]
		* A single dense-array slot: the owning entity's ID alongside its
		* component data.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 密配列の単一スロット: 所有元エンティティの ID と、その
		* コンポーネントデータを保持する。
		*/
		struct Element
		{
			/// [EN] The entity this slot's data belongs to.
			/// [JP] このスロットのデータが属するエンティティ。
			EntityID id_;

			/// [EN] The component data itself.
			/// [JP] コンポーネントデータ本体。
			T data_;
		};

	public:
		/**
		* [EN]
		* Default constructor: starts with empty sparse/dense arrays.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* デフォルトコンストラクタ: 空の sparse/dense 配列から開始する。
		*/
		SparseSet() = default;

		/**
		* [EN]
		* Destructor; uses the compiler-generated default.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* デストラクタ。コンパイラ生成のデフォルトを使用する。
		*/
		~SparseSet() = default;

		/**
		* [EN]
		* Inserts a default-constructed element for id if not already
		* present, growing sparse_ as needed. Returns the entity's dense index.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* id 用のデフォルト構築された要素を、まだ存在しなければ挿入する
		* （必要なら sparse_ を成長させる）。エンティティの密インデックスを
		* 返す。
		*/
		Uint32 Add(EntityID id)
		{
			if (id.index_ >= sparse_.size())
			{
				sparse_.resize(id.index_ + 1, UINT32_MAX);
			}

			if (sparse_[id.index_] != UINT32_MAX)
			{
				return sparse_[id.index_];
			}

			Uint32 denseIndex = static_cast<Uint32>(dense_.size());
			sparse_[id.index_] = denseIndex;
			dense_.push_back({ id, T() });
			return denseIndex;
		}

		/**
		* [EN]
		* Removes id via swap-remove: moves the last dense element into
		* id's vacated slot (unless id was already last). Returns the
		* EntityID of whichever entity was moved into the removed slot
		* (so its index cache can be updated), or a default (invalid)
		* EntityID if none was moved (id not found, or it was already last).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* id を swap-remove 方式で削除する: 最後の密要素を id の空いた
		* スロットへ移動する（id が既に最後だった場合を除く）。削除された
		* スロットへ移動したエンティティの EntityID を返す（その
		* インデックスキャッシュを更新できるように）。何も移動されな
		* かった場合（id が見つからない、または既に最後だった場合）は
		* デフォルト（無効）の EntityID を返す。
		*/
		EntityID Remove(EntityID id)
		{
			if (!Contains(id))
			{
				return EntityID{};
			}

			Uint32 removeIndex = sparse_[id.index_];
			Uint32 lastIndex = static_cast<Uint32>(dense_.size()) - 1;

			if (removeIndex == lastIndex)
			{
				dense_.pop_back();
				sparse_[id.index_] = UINT32_MAX;
				return EntityID{};
			}

			EntityID lastEntity = dense_[lastIndex].id_;
			dense_[removeIndex] = dense_[lastIndex];
			sparse_[lastEntity.index_] = removeIndex;

			dense_.pop_back();
			sparse_[id.index_] = UINT32_MAX;
			return lastEntity;
		}

		/**
		* [EN]
		* Returns a mutable reference to id's component data (undefined
		* behavior if id is not present).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* id のコンポーネントデータへの変更可能な参照を返す（id が
		* 存在しない場合は未定義動作）。
		*/
		T& Get(EntityID id)
		{
			return dense_[sparse_[id.index_]].data_;
		}

		/**
		* [EN]
		* Const overload of Get().
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* Get() の const オーバーロード。
		*/
		const T& Get(EntityID id)const
		{
			return dense_[sparse_[id.index_]].data_;
		}

		/**
		* [EN]
		* Returns whether id currently has an entry in this set. A stale id
		* (its slot index is in range and occupied, but by an entity of a
		* different generation) reports false.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* id が現在このセット内にエントリを持つかどうかを返す。古い id
		* （スロットインデックスは範囲内かつ占有済みだが、別の世代の
		* エンティティによる）は false を返す。
		*/
		Bool Contains(EntityID id)const
		{
			return id.index_ < sparse_.size()
				&& sparse_[id.index_] != UINT32_MAX
				&& dense_[sparse_[id.index_]].id_ == id;
		}

		/**
		* [EN]
		* Returns id's index within the dense array (undefined behavior
		* if id is not present).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 密配列内における id のインデックスを返す（id が存在しない
		* 場合は未定義動作）。
		*/
		Uint32 IndexOf(EntityID id)const
		{
			return sparse_[id.index_];
		}

		/**
		* [EN]
		* Returns the underlying dense array, for direct iteration.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 直接走査するための、内部の密配列を返す。
		*/
		const DynamicArray<Element>& Dense()const
		{
			return dense_;
		}

		/**
		* [EN]
		* Returns the current number of entities stored in this set.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* このセットに現在格納されているエンティティ数を返す。
		*/
		Uint32 Length()const
		{
			return static_cast<Uint32>(dense_.size());
		}

	private:
		/// [EN] Maps an entity ID to its index in dense_, or UINT32_MAX if absent.
		/// [JP] エンティティ ID を dense_ 内のインデックスへ対応付ける。存在しなければ UINT32_MAX。
		DynamicArray<Uint32> sparse_;

		/// [EN] Densely-packed storage of every present entity's ID and component data.
		/// [JP] 現在存在する全エンティティの ID とコンポーネントデータを、密に詰めて格納するストレージ。
		DynamicArray<Element> dense_;
	};

	/**
	* [EN]
	* Type-erased interface over a SparseSet<T>, letting World/Actor
	* manipulate sparse-set-backed components without knowing their
	* concrete type at the call site (the concrete type is captured by
	* SparseSetStorage<T> and constructed via ComponentMetadata::createSparseStorage_).
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* SparseSet<T> に対する型消去されたインターフェース。World/Actor が、
	* 呼び出し側で具体的な型を知らずとも、スパースセットで裏付けられた
	* コンポーネントを操作できるようにする（具体的な型は
	* SparseSetStorage<T> に捕捉され、ComponentMetadata::createSparseStorage_
	* 経由で構築される）。
	*/
	class InterfaceSparseSetStorage
	{
	public:
		/**
		* [EN]
		* Virtual destructor; uses the compiler-generated default.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 仮想デストラクタ。コンパイラ生成のデフォルトを使用する。
		*/
		virtual ~InterfaceSparseSetStorage() = default;

		/**
		* [EN]
		* Type-erased counterpart of SparseSet<T>::Remove: removes the
		* entity's entry, if present.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* SparseSet<T>::Remove の型消去版: エンティティのエントリが
		* 存在すれば削除する。
		*/
		virtual void Remove(EntityID id) = 0;

		/**
		* [EN]
		* Type-erased counterpart of SparseSet<T>::Add: inserts a
		* default-constructed element for the entity if not present.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* SparseSet<T>::Add の型消去版: エンティティが未登録なら
		* デフォルト構築した要素を挿入する。
		*/
		virtual void Add(EntityID id) = 0;

		/**
		* [EN]
		* Type-erased counterpart of SparseSet<T>::Contains.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* SparseSet<T>::Contains の型消去版。
		*/
		virtual Bool Contains(EntityID id)const = 0;

		/**
		* [EN]
		* Returns a type-erased pointer to the entity's component data,
		* or nullptr if the entity has no entry.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* エンティティのコンポーネントデータへの型消去されたポインタを
		* 返す。エンティティにエントリが無ければ nullptr を返す。
		*/
		virtual void* Raw(EntityID id) = 0;
	};

	/**
	* [EN]
	* Concrete InterfaceSparseSetStorage implementation wrapping a
	* SparseSet<T>, forwarding every virtual call to it.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* SparseSet<T> を包む、具体的な InterfaceSparseSetStorage の実装。
	* すべての仮想呼び出しをそれへ転送する。
	*/
	template<typename T>
	class SparseSetStorage final :public InterfaceSparseSetStorage
	{
	public:
		/**
		* [EN]
		* Forwards to data_.Remove(id).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* data_.Remove(id) へ転送する。
		*/
		void Remove(EntityID id) override
		{
			data_.Remove(id);
		}

		/**
		* [EN]
		* Forwards to data_.Add(id).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* data_.Add(id) へ転送する。
		*/
		void Add(EntityID id) override
		{
			data_.Add(id);
		}

		/**
		* [EN]
		* Forwards to data_.Contains(id).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* data_.Contains(id) へ転送する。
		*/
		Bool Contains(EntityID id)const override
		{
			return data_.Contains(id);
		}

		/**
		* [EN]
		* Returns a pointer to data_.Get(id), or nullptr if id is not present.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* data_.Get(id) へのポインタを返す。id が存在しなければ nullptr
		* を返す。
		*/
		void* Raw(EntityID id) override
		{
			if (!data_.Contains(id))
			{
				return nullptr;
			}

			return &data_.Get(id);
		}

		/// [EN] The underlying typed sparse set this storage wraps.
		/// [JP] このストレージが包む、内部の型付きスパースセット。
		SparseSet<T> data_;
	};
}
