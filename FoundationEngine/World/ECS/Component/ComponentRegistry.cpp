#include <FoundationEngine/World/ECS/Component/ComponentRegistry.h>

namespace SeedCore
{
	/**
	* [EN]
	* Removes id from every cross-reference map. TypeIndex() is keyed
	* by std::type_index rather than ComponentID, so it can't be erased
	* by key directly here; instead this scans its entries for the one
	* whose value equals id and erases that.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* id を全ての相互参照マップから削除する。TypeIndex() は ComponentID
	* ではなく std::type_index をキーにしているため、ここではキー指定で
	* 直接削除できない。代わりに、値が id と一致するエントリをスキャンして
	* そのエントリを削除する。
	*/
	void ComponentRegistry::Unregister(ComponentID id)
	{
		auto nameIt = NameMap().find(id);
		if (nameIt != NameMap().end())
		{
			NameIndex().erase(nameIt->second);
			NameMap().erase(id);
		}

		MetadataMap().erase(id);
		InternalID().erase(id);

		for (auto it = TypeIndex().begin(); it != TypeIndex().end(); )
		{
			if (it->second == id)
			{
				it = TypeIndex().erase(it);
			}
			else
			{
				++it;
			}
		}
	}

	/**
	* [EN]
	* Returns the ComponentID registered under name, or nullptr if no
	* component was registered with that name.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* name で登録された ComponentID を返す。その名前で登録された
	* コンポーネントが無ければ nullptr を返す。
	*/
	ComponentID ComponentRegistry::GetComponentID(String name)
	{
		auto it = NameIndex().find(name);
		if (it == NameIndex().end())
		{
			return nullptr;
		}
		return it->second;
	}

	/**
	* [EN]
	* Returns the display name id was registered under, or an empty
	* string if id is unregistered.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* id が登録された際の表示名を返す。id が未登録であれば空文字列を
	* 返す。
	*/
	String ComponentRegistry::Name(ComponentID id)
	{
		auto it = NameMap().find(id);
		if (it == NameMap().end())
		{
			return String();
		}
		return it->second;
	}

	/**
	* [EN]
	* Returns the full registry mapping every registered ComponentID to
	* its ComponentMetadata.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 登録済みの全 ComponentID をその ComponentMetadata へ対応付ける、
	* 完全なレジストリを返す。
	*/
	const FlatMap<ComponentID, ComponentMetadata>& ComponentRegistry::Registry()
	{
		return MetadataMap();
	}

	/**
	* [EN]
	* Returns the ComponentMetadata registered for id.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* id に対して登録されている ComponentMetadata を返す。
	*/
	const ComponentMetadata& ComponentRegistry::Get(ComponentID id)
	{
		return MetadataMap().at(id);
	}

	/**
	* [EN]
	* Returns id's dense internal index, assigning a new one on first
	* request.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* id の密な内部インデックスを返す。初回リクエスト時には新しい
	* インデックスを割り当てる。
	*/
	Size ComponentRegistry::GetID(ComponentID id)
	{
		auto it = InternalID().find(id);
		if (it != InternalID().end())
		{
			return it->second;
		}

		/// [EN] First time id has been queried: hand out the next sequential dense index and remember the mapping.
		/// [JP] id が問い合わせられるのが初めての場合: 次の連番の密インデックスを払い出し、その対応関係を記憶する。
		Size newID = nextID_++;
		InternalID().insert({ id, newID });
		return newID;
	}

	/**
	* [EN]
	* Returns the byte size of the component type registered under id.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* id で登録されているコンポーネント型のバイトサイズを返す。
	*/
	Size ComponentRegistry::ComponentSize(ComponentID id)
	{
		return Get(id).size_;
	}

	/**
	* [EN]
	* Returns the alignment requirement of the component type registered
	* under id.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* id で登録されているコンポーネント型のアラインメント要件を返す。
	*/
	Size ComponentRegistry::ComponentAlignment(ComponentID id)
	{
		return Get(id).alignment_;
	}

	/**
	* [EN]
	* Returns the full mapping from registered component names to their
	* ComponentID.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 登録済みのコンポーネント名からその ComponentID への、完全な
	* マッピングを返す。
	*/
	const FlatMap<String, ComponentID>& ComponentRegistry::ComponentList()
	{
		return NameIndex();
	}

	/**
	* [EN]
	* The static initializer the REGISTER_COMPONENT macro emits can
	* call Register<T>() before a namespace-scope member is
	* constructed, since initialization order across translation units
	* is unspecified (Static Initialization Order Fiasco). Every
	* registry map is therefore routed through a function-local static
	* (Meyer's singleton), guaranteed constructed on first access.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* REGISTER_COMPONENT マクロが生成する静的初期化子は、TU 間の
	* 初期化順序が未規定であるため(Static Initialization Order
	* Fiasco)、名前空間スコープのメンバが構築される前に Register<T>()
	* を呼び得る。そのため各レジストリマップを関数ローカル static
	* (Meyer のシングルトン)経由にして初回アクセス時に確実に構築させる。
	*/
	FlatMap<ComponentID, ComponentMetadata>& ComponentRegistry::MetadataMap()
	{
		static FlatMap<ComponentID, ComponentMetadata> instance;
		return instance;
	}

	/**
	* [EN]
	* Returns the ComponentID-to-name map backing Name(); see the
	* comment above MetadataMap() for the initialization-order rationale.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* Name() の裏付けとなる、ComponentID から名前へのマップを
	* 返す。初期化順序の根拠については MetadataMap() 上のコメントを参照。
	*/
	FlatMap<ComponentID, String>& ComponentRegistry::NameMap()
	{
		static FlatMap<ComponentID, String> instance;
		return instance;
	}

	/**
	* [EN]
	* Returns the type_index-to-ComponentID map used by the
	* templated GetComponentID<T>() overload; see the comment above
	* MetadataMap() for the initialization-order rationale.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* テンプレート版 GetComponentID<T>() オーバーロードが使用する、
	* type_index から ComponentID へのマップを返す。
	* 初期化順序の根拠については MetadataMap() 上のコメントを参照。
	*/
	FlatMap<std::type_index, ComponentID>& ComponentRegistry::TypeIndex()
	{
		static FlatMap<std::type_index, ComponentID> instance;
		return instance;
	}

	/**
	* [EN]
	* Returns the name-to-ComponentID map backing GetComponentID(String)
	* and ComponentList(); see the comment above MetadataMap() for
	* the initialization-order rationale.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* GetComponentID(String) と ComponentList() の裏付けとなる、
	* 名前から ComponentID へのマップを返す。
	* 初期化順序の根拠については MetadataMap() 上のコメントを参照。
	*/
	FlatMap<String, ComponentID>& ComponentRegistry::NameIndex()
	{
		static FlatMap<String, ComponentID> instance;
		return instance;
	}

	/**
	* [EN]
	* Returns the id-to-dense-index map backing GetID(); see the
	* comment above MetadataMap() for the initialization-order rationale.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* GetID() の裏付けとなる、ID から密インデックスへのマップを返す。
	* 初期化順序の根拠については MetadataMap() 上のコメントを参照。
	*/
	FlatMap<ComponentID, Size>& ComponentRegistry::InternalID()
	{
		static FlatMap<ComponentID, Size> instance;
		return instance;
	}
}
