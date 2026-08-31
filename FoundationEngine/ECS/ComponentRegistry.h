#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/ECS/Component.h>
#include <FoundationEngine/ECS/EcsID.h>
#include <FoundationEngine/ECS/SparseSet.h>
#include <FoundationEngine/ECS/Component/ComponentBase.h>
#include <FoundationEngine/Utility/FlatMap.h>

namespace SeedCore
{
	/**
	* [EN]
	* Fallback: false for any T lacking an is_component_base_tag member
	* typedef (i.e. T doesn't derive from ComponentBase).
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* フォールバック: is_component_base_tag というメンバ型定義を持たない
	* 任意の T に対して false になる（すなわち T が ComponentBase から
	* 派生していない）。
	*/
	template<typename T, typename = void>
	struct HasComponentBaseTag : std::false_type {};

	/**
	* [EN]
	* Specialization: true when T::is_component_base_tag exists (i.e. T
	* derives from ComponentBase).
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 特殊化: T::is_component_base_tag が存在する場合に true になる
	* （すなわち T が ComponentBase から派生している）。
	*/
	template<typename T>
	struct HasComponentBaseTag<T, std::void_t<typename T::is_component_base_tag>> : std::true_type {};

	/**
	* [EN]
	* Process-wide registry of every component type known to the ECS:
	* maps each ComponentID to its ComponentMetadata (size/alignment/
	* construct/destruct/move functions, storage kind), and back and
	* forth between component names and IDs. Also wires up
	* ComponentBase-derived types' lifecycle function pointers
	* (Awake/Start/Tick/... ) at registration time via the concepts
	* defined alongside each lifecycle mixin.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* ECS が認識する全コンポーネント型の、プロセス全体で共有される
	* レジストリ。各 ComponentID をその ComponentMetadata（サイズ/
	* アラインメント/構築・破棄・移動関数、ストレージ種別）へ対応付け、
	* コンポーネント名と ID の相互変換も行う。また、登録時に、各
	* ライフサイクルミックスインと共に定義されたコンセプトを通じて、
	* ComponentBase 派生型のライフサイクル関数ポインタ
	* （Awake/Start/Tick/...）を配線する。
	*/
	class SEEDCORE_API ComponentRegistry
	{
	public:
		/**
		* [EN]
		* Registers T as a component: builds its ComponentMetadata (via
		* Component<T>::Metadata), wires up a sparse-set storage factory
		* if storage is SparseSet, and (if T derives from ComponentBase)
		* installs a setupLifecycle_ callback that binds each lifecycle
		* function pointer (awake_/start_/tick_/... ) T actually
		* implements. Records the mapping between name, T's type_index,
		* and the resulting ComponentID.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* T をコンポーネントとして登録する: Component<T>::Metadata 経由で
		* その ComponentMetadata を構築し、storage が SparseSet であれば
		* スパースセットストレージのファクトリを配線する。T が
		* ComponentBase から派生していれば、T が実際に実装している各
		* ライフサイクル関数ポインタ（awake_/start_/tick_/...）を束縛する
		* setupLifecycle_ コールバックを設定する。名前、T の type_index、
		* 結果として得られる ComponentID の対応関係を記録する。
		*/
		template<typename T>
		static void Register(String name, String category = String::intern("Custom"), ComponentStorage storage = ComponentStorage::SparseSet)
		{
			ComponentID id = name.view().data();
			ComponentMetadata meta = Component<T>::Metadata(storage);
			meta.category_ = category;

			/// [EN] Sparse-set-stored components need a factory that can construct their SparseSetStorage<T> lazily, the first time an instance is actually added.
			/// [JP] スパースセット格納コンポーネントには、実際にインスタンスが追加される最初のタイミングで SparseSetStorage<T> を遅延構築できるファクトリが必要になる。
			if (storage == ComponentStorage::SparseSet)
			{
				meta.createSparseStorage_ = []() -> ResourcePtr<InterfaceSparseSetStorage>
				{
					return MakePtr<SparseSetStorage<T>>();
				};
			}

			if constexpr (HasComponentBaseTag<T>::value)
			{
				meta.isComponentBase_ = true;

				meta.setupLifecycle_ = [](void* component, void* world, Entity entity)
				{
					T* ptr = static_cast<T*>(component);
					ptr->world_ = static_cast<World*>(world);
					ptr->entity_ = entity;

					/// [EN] Each HasXxx<T> concept check below binds the corresponding type-erased function pointer only if T actually implements that lifecycle mixin, otherwise the pointer stays nullptr and ComponentBase::DispatchXxx becomes a no-op for that hook.
					/// [JP] 以下の各 HasXxx<T> コンセプトチェックは、T が実際にそのライフサイクルミックスインを実装している場合にのみ、対応する型消去された関数ポインタを束縛する。実装していなければポインタは nullptr のままとなり、そのフックに対する ComponentBase::DispatchXxx は無操作になる。
					if constexpr (HasAwake<T>)
					{
						ptr->awake_ = [](ComponentBase* cb) { static_cast<T*>(cb)->OnAwake(); };
					}
					if constexpr (HasStart<T>)
					{
						ptr->start_ = [](ComponentBase* cb) { static_cast<T*>(cb)->OnStart(); };
					}
					if constexpr (HasTick<T>)
					{
						ptr->tick_ = [](ComponentBase* cb, Float dt) { static_cast<T*>(cb)->OnTick(dt); };
					}
					if constexpr (HasFixedTick<T>)
					{
						ptr->fixedTick_ = [](ComponentBase* cb, Float dt) { static_cast<T*>(cb)->OnFixedTick(dt); };
					}
					if constexpr (HasLateTick<T>)
					{
						ptr->lateTick_ = [](ComponentBase* cb, Float dt) { static_cast<T*>(cb)->OnLateTick(dt); };
					}
					if constexpr (HasDestroy<T>)
					{
						ptr->destroy_ = [](ComponentBase* cb) { static_cast<T*>(cb)->OnDestroy(); };
					}
					if constexpr (HasInspectorGUI<T>)
					{
						ptr->inspectorGUI_ = [](ComponentBase* cb) { static_cast<T*>(cb)->OnInspectorGUI(); };
					}
					if constexpr (HasCollisionEnter<T>)
					{
						ptr->collisionEnter_ = [](ComponentBase* cb, Entity other) { static_cast<T*>(cb)->OnCollisionEnter(other); };
					}
					if constexpr (HasCollisionStay<T>)
					{
						ptr->collisionStay_ = [](ComponentBase* cb, Entity other) { static_cast<T*>(cb)->OnCollisionStay(other); };
					}
					if constexpr (HasCollisionExit<T>)
					{
						ptr->collisionExit_ = [](ComponentBase* cb, Entity other) { static_cast<T*>(cb)->OnCollisionExit(other); };
					}
					if constexpr (HasTriggerEnter<T>)
					{
						ptr->triggerEnter_ = [](ComponentBase* cb, Entity other) { static_cast<T*>(cb)->OnTriggerEnter(other); };
					}
					if constexpr (HasTriggerStay<T>)
					{
						ptr->triggerStay_ = [](ComponentBase* cb, Entity other) { static_cast<T*>(cb)->OnTriggerStay(other); };
					}
					if constexpr (HasTriggerExit<T>)
					{
						ptr->triggerExit_ = [](ComponentBase* cb, Entity other) { static_cast<T*>(cb)->OnTriggerExit(other); };
					}
				};
			}

			/// [EN] Record all four cross-references at once so every lookup path (by ComponentID, by type_index, by name) stays in sync.
			/// [JP] 4つの相互参照を一度に記録し、全ての検索経路（ComponentID による、type_index による、名前による）が同期した状態を保つようにする。
			Registry()[id] = meta;
			TypeIndex()[std::type_index(typeid(T))] = id;
			NameMap()[id] = name;
			NameToID()[name] = id;
		}

		/**
		* [EN]
		* Returns the ComponentID that T was registered under, or
		* nullptr if T has not been registered.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* T が登録された際の ComponentID を返す。T が未登録であれば
		* nullptr を返す。
		*/
		template<typename T>
		static ComponentID GetComponentID()
		{
			auto it = TypeIndex().find(std::type_index(typeid(T)));
			if (it == TypeIndex().end())
			{
				return nullptr;
			}
			return it->second;
		}

		/**
		* [EN]
		* Returns the ComponentID registered under name, or nullptr if
		* no component was registered with that name.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* name で登録された ComponentID を返す。その名前で登録された
		* コンポーネントが無ければ nullptr を返す。
		*/
		static ComponentID GetComponentID(String name);

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
		static String GetName(ComponentID id);

		/**
		* [EN]
		* Returns the full registry mapping every registered ComponentID
		* to its ComponentMetadata.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 登録済みの全 ComponentID をその ComponentMetadata へ対応付ける、
		* 完全なレジストリを返す。
		*/
		static const FlatMap<ComponentID, ComponentMetadata>& GetRegistry();

		/**
		* [EN]
		* Returns the ComponentMetadata registered for id.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* id に対して登録されている ComponentMetadata を返す。
		*/
		static const ComponentMetadata& Get(ComponentID id);

		/**
		* [EN]
		* Returns id's dense internal index, assigning a new one on
		* first request (used e.g. to index into Archetype signature bitsets).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* id の密な内部インデックスを返す。初回リクエスト時には新しい
		* インデックスを割り当てる（例えば Archetype のシグネチャ
		* ビットセットへのインデックス付けに使われる）。
		*/
		static Size GetID(ComponentID id);

		/**
		* [EN]
		* Returns the byte size of the component type registered under id.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* id で登録されているコンポーネント型のバイトサイズを返す。
		*/
		static Size GetComponentSize(ComponentID id);

		/**
		* [EN]
		* Returns the alignment requirement of the component type
		* registered under id.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* id で登録されているコンポーネント型のアラインメント要件を返す。
		*/
		static Size GetComponentAlignment(ComponentID id);

		/**
		* [EN]
		* Returns the full mapping from registered component names to
		* their ComponentID.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 登録済みのコンポーネント名からその ComponentID への、完全な
		* マッピングを返す。
		*/
		static const FlatMap<String, ComponentID>& GetComponentList();

		/**
		* [EN]
		* Removes id's entry from every cross-reference map Register<T>()
		* populates (Registry(), InternalID(), TypeIndex(), NameMap(),
		* NameToID()). Used by hot reload to purge a component before the
		* module owning its construct_/destruct_ function pointers — and,
		* more importantly, T's std::type_info — is unloaded. Left undone,
		* a stale std::type_index entry would remain in TypeIndex() after
		* FreeLibrary(), pointing at a type_info living in freed memory;
		* the next Register<T>() call anywhere that happens to probe the
		* same slot would compare/hash against it and crash.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* id のエントリを、Register<T>() が書き込む全ての相互参照マップ
		* (Registry()、InternalID()、TypeIndex()、NameMap()、NameToID())
		* から削除する。ホットリロードが、construct_/destruct_ 関数
		* ポインタ — そしてより重要な T の std::type_info — を所有する
		* モジュールがアンロードされる前に、コンポーネントを消し去るために
		* 使う。これを怠ると、FreeLibrary() 後も TypeIndex() に古い
		* std::type_index エントリが残り続け、解放済みメモリ上の
		* type_info を指したままになる。以後どこかで行われる
		* Register<T>() 呼び出しが同じスロットを探査した際、それと
		* 比較/ハッシュしてクラッシュする。
		*/
		static void Unregister(ComponentID id);

	private:
		/**
		* [EN]
		* The static initializer the REGISTER_COMPONENT macro emits can
		* call Register<T>() before a namespace-scope member is
		* constructed, since initialization order across translation units
		* is unspecified (Static Initialization Order Fiasco). Release
		* routes every registry map through a function-local static
		* (Meyer's singleton), guaranteed constructed on first access;
		* Debug keeps them as static inline members.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* REGISTER_COMPONENT マクロが生成する静的初期化子は、TU 間の
		* 初期化順序が未規定であるため(Static Initialization Order
		* Fiasco)、名前空間スコープのメンバが構築される前に Register<T>()
		* を呼び得る。Release は各レジストリマップを関数ローカル static
		* (Meyer のシングルトン)経由にして初回アクセス時に確実に構築させ、
		* Debug は static inline メンバのまま保持する。
		*/
		static FlatMap<ComponentID, ComponentMetadata>& Registry();

		/**
		* [EN]
		* Returns the id-to-dense-index map backing GetID(); see the
		* comment above Registry() for the Debug/Release storage split.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* GetID() の裏付けとなる、ID から密インデックスへのマップを返す。
		* Debug/Release でのストレージ分割については Registry() 上の
		* コメントを参照。
		*/
		static FlatMap<ComponentID, Size>& InternalID();

		/**
		* [EN]
		* Returns the type_index-to-ComponentID map used by the
		* templated GetComponentID<T>() overload; see the comment above
		* Registry() for the Debug/Release storage split.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* テンプレート版 GetComponentID<T>() オーバーロードが使用する、
		* type_index から ComponentID へのマップを返す。Debug/Release
		* でのストレージ分割については Registry() 上のコメントを参照。
		*/
		static FlatMap<std::type_index, ComponentID>& TypeIndex();

		/**
		* [EN]
		* Returns the ComponentID-to-name map backing GetName(); see the
		* comment above Registry() for the Debug/Release storage split.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* GetName() の裏付けとなる、ComponentID から名前へのマップを
		* 返す。Debug/Release でのストレージ分割については Registry() 上の
		* コメントを参照。
		*/
		static FlatMap<ComponentID, String>& NameMap();

		/**
		* [EN]
		* Returns the name-to-ComponentID map backing GetComponentID(String)
		* and GetComponentList(); see the comment above Registry() for
		* the Debug/Release storage split.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* GetComponentID(String) と GetComponentList() の裏付けとなる、
		* 名前から ComponentID へのマップを返す。Debug/Release での
		* ストレージ分割については Registry() 上のコメントを参照。
		*/
		static FlatMap<String, ComponentID>& NameToID();

#ifdef _DEBUG
		/// [EN] Debug-only storage for Registry(); see the comment above Registry() for why Release uses a function-local static instead.
		/// [JP] Registry() 用の Debug 専用ストレージ。Release が代わりに関数ローカル static を使う理由については Registry() 上のコメントを参照。
		static inline FlatMap<ComponentID, ComponentMetadata> registry_;

		/// [EN] Debug-only storage for InternalID(); see the comment above Registry() for why Release uses a function-local static instead.
		/// [JP] InternalID() 用の Debug 専用ストレージ。Release が代わりに関数ローカル static を使う理由については Registry() 上のコメントを参照。
		static inline FlatMap<ComponentID, Size> internalID_;

		/// [EN] Debug-only storage for TypeIndex(); see the comment above Registry() for why Release uses a function-local static instead.
		/// [JP] TypeIndex() 用の Debug 専用ストレージ。Release が代わりに関数ローカル static を使う理由については Registry() 上のコメントを参照。
		static inline FlatMap<std::type_index, ComponentID> typeIndex_;

		/// [EN] Debug-only storage for NameMap(); see the comment above Registry() for why Release uses a function-local static instead.
		/// [JP] NameMap() 用の Debug 専用ストレージ。Release が代わりに関数ローカル static を使う理由については Registry() 上のコメントを参照。
		static inline FlatMap<ComponentID, String> nameMap_;

		/// [EN] Debug-only storage for NameToID(); see the comment above Registry() for why Release uses a function-local static instead.
		/// [JP] NameToID() 用の Debug 専用ストレージ。Release が代わりに関数ローカル static を使う理由については Registry() 上のコメントを参照。
		static inline FlatMap<String, ComponentID> nameToID_;
#endif
		/// [EN] Next dense internal index to hand out from GetID().
		/// [JP] GetID() から次に払い出される密な内部インデックス。
		static inline Size nextID_ = 0;
	};
}

/// [EN] Emits a ComponentTraits<Type> specialization fixing its storage kind at compile time, when Storage is provided (via __VA_ARGS__); otherwise expands to nothing.
/// [JP] Storage が指定されている場合（__VA_ARGS__ 経由）、そのストレージ種別をコンパイル時に固定する ComponentTraits<Type> 特殊化を生成する。指定が無ければ何も生成しない。
#define SEED_TRAITS_SPEC(Type, ...) \
	__VA_OPT__(template<> struct ComponentTraits<Type> { \
		static constexpr ComponentStorage storage = __VA_ARGS__; \
	};)

/**
* [EN]
* Category is the grouping label for the Add Component panel (e.g.
* "Light" so PointLight/SpotLight/... show under one header). Both
* Category and Storage are optional, defaulting to "Custom" and
* SparseSet when omitted (same defaults as ComponentRegistry::Register).
* REGISTER_COMPONENT(Type)
* REGISTER_COMPONENT(Type, "Category")
* REGISTER_COMPONENT(Type, "Category", ComponentStorage::Archetype)
*
* ---------------------------------------------------------------------
*
* [JP]
* Category は Add Component パネルのグルーピングラベル（例: "Light" で
* PointLight/SpotLight/... を1つの見出しの下にまとめる）。Category も
* Storage も省略可能で、省略時はそれぞれ "Custom" / SparseSet になる
* （ComponentRegistry::Register のデフォルトと同じ）。
*/
#define REGISTER_COMPONENT_1(Type) \
	static const SeedCore::Bool Type##_registered = []() { \
		SeedCore::ComponentRegistry::Register<Type>(SeedCore::String::intern(#Type)); \
		return true; \
	}()

#define REGISTER_COMPONENT_2(Type, Category) \
	static const SeedCore::Bool Type##_registered = []() { \
		SeedCore::ComponentRegistry::Register<Type>(SeedCore::String::intern(#Type), SeedCore::String(Category)); \
		return true; \
	}()

#define REGISTER_COMPONENT_3(Type, Category, Storage) \
	SEED_TRAITS_SPEC(Type, Storage) \
	static const SeedCore::Bool Type##_registered = []() { \
		SeedCore::ComponentRegistry::Register<Type>(SeedCore::String::intern(#Type), SeedCore::String(Category), Storage); \
		return true; \
	}()

#define REGISTER_COMPONENT_GET_MACRO(_1, _2, _3, NAME, ...) NAME
#define REGISTER_COMPONENT(...) REGISTER_COMPONENT_GET_MACRO(__VA_ARGS__, REGISTER_COMPONENT_3, REGISTER_COMPONENT_2, REGISTER_COMPONENT_1)(__VA_ARGS__)
