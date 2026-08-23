#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/ECS/Entity.h>
#include <FoundationEngine/ECS/Component.h>
#include <FoundationEngine/ECS/EcsID.h>
#include <FoundationEngine/ECS/EcsConcept.h>
#include <FoundationEngine/ECS/ComponentRegistry.h>
#include <FoundationEngine/ECS/EntityRecord.h>
#include <FoundationEngine/ECS/Archetype.h>
#include <FoundationEngine/ECS/ArchetypeRegistry.h>
#include <FoundationEngine/ECS/SparseSet.h>
#include <FoundationEngine/ECS/Chunk.h>
#include <FoundationEngine/ECS/Actor.h>
#include <FoundationEngine/Pool/StablePool.h>
#include <FoundationEngine/Utility/FlatMap.h>
#include <PhysicsEngine/Physics/Physics.h>

namespace SeedCore
{
	/**
	* [EN]
	* Central owner of a scene's ECS state: entities, archetype-chunked
	* and sparse-set component storage, physics resources, and the
	* higher-level Actor wrappers built on top of raw entities. All
	* entity/component operations (create/destroy, add/remove/get
	* components, chunk lookup) are routed through this class.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* シーンの ECS 状態を統括するクラス: エンティティ、アーキタイプ・
	* チャンク構造化ストレージとスパースセットストレージ、物理リソース、
	* および生のエンティティの上に構築されるより高水準な Actor
	* ラッパーを保持する。全てのエンティティ/コンポーネント操作
	* （生成・破棄、コンポーネントの追加・削除・取得、チャンク検索）は
	* このクラスを経由する。
	*/
	class SEEDCORE_API World
	{
	public:
		// ============================================================
		// Lifecycle
		// ============================================================

		/**
		* [EN]
		* Default constructor: starts with an empty scene.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* デフォルトコンストラクタ: 空のシーンから開始する。
		*/
		World() = default;

		/**
		* [EN]
		* Destructor; uses the compiler-generated default.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* デストラクタ。コンパイラ生成のデフォルトを使用する。
		*/
		~World() = default;

		// ============================================================
		// Entity
		// ============================================================

		/**
		* [EN]
		* Creates a new, componentless entity and returns a handle to it.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* コンポーネントを持たない新しいエンティティを生成し、そのハンドルを
		* 返す。
		*/
		Entity CreateEntity();

		/**
		* [EN]
		* Destroys entity, removing its record, components, and (if
		* archetype-stored) its chunk row.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* entity を破棄し、そのレコード、コンポーネント、（アーキタイプ
		* 格納であれば）チャンク行を削除する。
		*/
		void DestroyEntity(Entity entity);

		/**
		* [EN]
		* Moves entityID's archetype-stored components from
		* sourceChunk (of sourceArcketype) into a chunk of destArcketype,
		* updating its EntityRecord.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* entityID のアーキタイプ格納コンポーネントを、sourceArcketype の
		* sourceChunk から destArcketype のチャンクへ移動し、その
		* EntityRecord を更新する。
		*/
		void MoveEntity(EntityID entityID, Archetype* sourceArcketype, Chunk* sourceChunk, Archetype* destArcketype);

		/**
		* [EN]
		* Returns the ordered list of component types making up entity's
		* archetype (empty if entity is sparse-set-only or componentless).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* entity のアーキタイプを構成する、順序付きのコンポーネント型
		* 一覧を返す（entity がスパースセットのみ、またはコンポーネントを
		* 持たない場合は空になる）。
		*/
		const DynamicArray<ComponentID>& GetLayout(Entity entity)const;

		// ============================================================
		// Component
		// ============================================================

		/**
		* [EN]
		* Adds a copy of value as entity's T component, routing to
		* sparse-set or archetype storage based on T's registered
		* ComponentStorage.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* value のコピーを entity の T コンポーネントとして追加する。
		* T の登録済み ComponentStorage に応じて、スパースセットまたは
		* アーキタイプストレージへルーティングする。
		*/
		template<typename T>
			requires(IsComponent<T> || std::derived_from<T, ComponentBase>)
		void AddComponent(Entity entity, const T& value)
		{
			ComponentID id = ComponentRegistry::GetComponentID<T>();
			EntityID entityID = static_cast<Uint32>(entity.GetHandle().index_);

			const ComponentMetadata& meta = ComponentRegistry::Get(id);

			if (meta.storage_ == ComponentStorage::SparseSet)
			{
				AddComponentSparse<T>(entityID, value);
			}
			else
			{
				AddComponentArchetype(entity, entityID, id, &value, sizeof(T));
			}
		}

		/**
		* [EN]
		* Type-erased overload of AddComponent: adds a default-constructed
		* instance of the component registered under id to entity.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* AddComponent の型消去版: id に登録されているコンポーネントの
		* デフォルト構築されたインスタンスを entity へ追加する。
		*/
		void AddComponent(Entity entity, ComponentID id);

		/**
		* [EN]
		* Returns a mutable pointer to entity's T component, or nullptr
		* if entity doesn't have one, routing to sparse-set or archetype
		* storage based on T's registered ComponentStorage.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* entity の T コンポーネントへの変更可能なポインタを返す。持って
		* いなければ nullptr を返す。T の登録済み ComponentStorage に
		* 応じて、スパースセットまたはアーキタイプストレージへ
		* ルーティングする。
		*/
		template<typename T>
			requires(IsComponent<T> || std::derived_from<T, ComponentBase>)
		T* GetComponent(Entity entity)
		{
			ComponentID id = ComponentRegistry::GetComponentID<T>();
			EntityID entityID = static_cast<Uint32>(entity.GetHandle().index_);

			const ComponentMetadata& meta = ComponentRegistry::Get(id);

			if (meta.storage_ == ComponentStorage::SparseSet)
			{
				return reinterpret_cast<T*>(GetComponentSparse<T>(entityID));
			}
			else
			{
				return reinterpret_cast<T*>(GetComponentArchetype(entityID, id));
			}
		}

		/**
		* [EN]
		* Const overload of GetComponent<T>(Entity), forwarding through a const_cast.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* GetComponent<T>(Entity) の const オーバーロード。const_cast
		* 経由で転送する。
		*/
		template<typename T>
		const T* GetComponent(Entity entity)const
		{
			return const_cast<World*>(this)->GetComponent<T>(entity);
		}

		/**
		* [EN]
		* Type-erased overload of GetComponent: returns a pointer to
		* entity's component registered under id, or nullptr if absent.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* GetComponent の型消去版: id に登録されている entity の
		* コンポーネントへのポインタを返す。無ければ nullptr を返す。
		*/
		void* GetComponent(Entity entity, ComponentID id);

		/**
		* [EN]
		* Overload of the type-erased GetComponent taking a raw EntityID
		* directly instead of an Entity handle.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* Entity ハンドルの代わりに生の EntityID を直接受け取る、型消去版
		* GetComponent のオーバーロード。
		*/
		void* GetComponent(EntityID entityID, ComponentID id);

		/**
		* [EN]
		* Returns whether entity currently has a T component.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* entity が現在 T コンポーネントを持っているかどうかを返す。
		*/
		template<typename T>
		Bool HasComponent(Entity entity)const
		{
			EntityID entityID = static_cast<Uint32>(entity.GetHandle().index_);
			return HasComponent<T>(entityID);
		}

		/**
		* [EN]
		* Overload of HasComponent<T> taking a raw EntityID directly:
		* checks the sparse set for sparse-set-stored components, or
		* scans the owning archetype's layout for archetype-stored ones.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 生の EntityID を直接受け取る HasComponent<T> のオーバーロード:
		* スパースセット格納コンポーネントであればスパースセットを
		* チェックし、アーキタイプ格納であれば所有元アーキタイプの
		* レイアウトを走査する。
		*/
		template<typename T>
		Bool HasComponent(EntityID entityID)const
		{
			ComponentID id = ComponentRegistry::GetComponentID<T>();
			const ComponentMetadata& meta = ComponentRegistry::Get(id);

			if (meta.storage_ == ComponentStorage::SparseSet)
			{
				auto* sparseSet = const_cast<World*>(this)->FindSparseSet<T>();
				return sparseSet && sparseSet->Contains(entityID);
			}
			else
			{
				auto it = records_.find(entityID);
				if (it == records_.end())
				{
					return false;
				}

				for (const ComponentID& cid : it->second.archetype_->Layout())
				{
					if (cid == id)
					{
						return true;
					}
				}
				return false;
			}
		}

		/**
		* [EN]
		* Type-erased overload of HasComponent: returns whether entity
		* has the component registered under id.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* HasComponent の型消去版: entity が id に登録されている
		* コンポーネントを持っているかどうかを返す。
		*/
		Bool HasComponent(Entity entity, ComponentID id)const;

		/**
		* [EN]
		* Returns how many entities currently have a T component.
		* Sparse-set-stored T resolves in O(1) without walking every actor
		* -- lets callers cheaply check "does any entity have this
		* component at all" (e.g. to skip a whole system when its
		* component is unused this frame); returns 0 if T's storage hasn't
		* been created yet. Archetype-stored T has no such index, so this
		* instead walks every actor and counts the ones that have T (same
		* O(N) trade-off as GetComponents<T>()).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 現在 T コンポーネントを持つエンティティ数を返す。スパースセット
		* 格納の T は全 actor を走査せず O(1) で解決する — 「このコンポーネント
		* を持つエンティティが1つでもあるか」を安価に確認できる(例: 今フレーム
		* 未使用のシステム全体をスキップする)。T のストレージがまだ作られて
		* いなければ 0 を返す。アーキタイプ格納の T にはそのようなインデックス
		* が無いため、代わりに全 actor を走査し T を持つものを数える
		* （GetComponents<T>() と同じ O(N) のトレードオフ）。
		*/
		template<typename T>
		Uint32 GetComponentCount()const
		{
			ComponentID id = ComponentRegistry::GetComponentID<T>();
			const ComponentMetadata& meta = ComponentRegistry::Get(id);

			if (meta.storage_ == ComponentStorage::SparseSet)
			{
				auto it = sparseSet_.find(id);
				if (it == sparseSet_.end())
				{
					return 0;
				}

				return static_cast<SparseSetStorage<T>*>(it->second.get())->data_.Length();
			}

			Uint32 count = 0;
			for (const auto& actor : GetActors())
			{
				if (actor->HasComponent(id))
				{
					count++;
				}
			}
			return count;
		}

		/**
		* [EN]
		* Returns the EntityID of every entity currently holding a T
		* component. Sparse-set-stored T resolves in O(1) via its dense
		* array (returns empty if T's storage hasn't been created yet).
		* Archetype-stored T has no such index, so this instead walks every
		* actor and keeps the ones that have T (see the in-body comment) —
		* fine for editor-scale actor counts; a hot per-frame path over
		* thousands of actors should use Query<Read<T>> instead, which
		* chunk-iterates archetypes directly.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 現在 T コンポーネントを持つ全エンティティの EntityID を返す。
		* スパースセット格納の T は密配列経由で O(1) 解決する（T のストレージ
		* がまだ作られていなければ空を返す）。アーキタイプ格納の T にはそのような
		* インデックスが無いため、代わりに全 actor を走査し T を持つものだけ
		* 残す（本体内コメント参照）— エディタ規模の actor 数なら十分。数千
		* actor 規模の毎フレームホットパスでは、アーキタイプを直接チャンク
		* 走査する Query<Read<T>> を使うこと。
		*/
		template<typename T>
		DynamicArray<EntityID> GetComponents()const
		{
			DynamicArray<EntityID> result;

			ComponentID id = ComponentRegistry::GetComponentID<T>();
			const ComponentMetadata& meta = ComponentRegistry::Get(id);

			if (meta.storage_ == ComponentStorage::SparseSet)
			{
				auto it = sparseSet_.find(id);
				if (it == sparseSet_.end())
				{
					return result;
				}

				const SparseSet<T>& sparseSet = static_cast<SparseSetStorage<T>*>(it->second.get())->data_;
				result.reserve(sparseSet.Length());
				for (const auto& element : sparseSet.Dense())
				{
					result.push_back(element.id_);
				}
			}
			else
			{
				/// [EN] No O(1) "every entity with T" index for archetype
				///      storage — walk every actor and keep the ones that
				///      have T.
				/// [JP] アーキタイプ格納には「Tを持つ全エンティティ」への
				///      O(1) インデックスが無い — 全 actor を走査し、T を
				///      持つものだけ残す。
				for (const auto& actor : GetActors())
				{
					if (actor->HasComponent(id))
					{
						result.push_back(static_cast<Uint32>(actor->GetEntity().GetHandle().index_));
					}
				}
			}

			return result;
		}

		/**
		* [EN]
		* Removes entity's component registered under id, if present.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* entity の、id に登録されているコンポーネントを、存在すれば
		* 削除する。
		*/
		void RemoveComponent(Entity entity, ComponentID id);

		/**
		* [EN]
		* Erases id's sparse-set storage container entirely (not just its
		* entries), destroying it and any component data it still holds.
		* Callers should remove every entity's component data first (e.g.
		* via RemoveComponent per actor) so ComponentBase::OnDestroy still
		* runs — this just drops the container itself. Intended for
		* tearing down a component type whose code lives in a DLL that is
		* about to be unloaded, since the storage container's own vtable
		* would otherwise dangle. No-op if id has no sparse-set storage.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* id のスパースセットストレージコンテナ自体を(単なるエントリではなく)
		* 消去し、破棄する。まだ保持しているコンポーネントデータもろとも破棄する。
		* ComponentBase::OnDestroy を確実に発火させるため、呼び出し側は先に
		* actor単位の RemoveComponent 等で各エンティティのコンポーネントデータを
		* 削除しておくべき — これはコンテナ自体を落とすだけ。これからアンロード
		* される DLL 内にコードがあるコンポーネント型を解体する用途を想定している
		* — そうしないとストレージコンテナ自身の仮想関数テーブルが宙に浮いてしまう
		* ため。id にスパースセットストレージが無い場合は何もしない。
		*/
		void UnregisterSparseSetStorage(ComponentID id);

		/**
		* [EN]
		* Returns the mapping from every live Archetype to its list of Chunks.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 現在有効な全 Archetype を、そのチャンク一覧へ対応付けるマップを
		* 返す。
		*/
		const FlatMap<Archetype*, DynamicArray<Chunk*>>& GetChunk()const;

		// ============================================================
		// Actor
		// ============================================================

		/**
		* [EN]
		* Creates a new entity wrapped in an Actor with the given display
		* name, and returns a pointer to it. If persistentId is nonzero
		* and not already taken, the new actor is assigned that ID
		* (restoring a Scene/Prefab-captured actor's identity); otherwise
		* (persistentId is 0, or already in use by another live actor) a
		* fresh ID is auto-allocated instead.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 指定された表示名を持つ Actor で包まれた新しいエンティティを生成
		* し、そのポインタを返す。persistentId が非ゼロかつ未使用であれば、
		* 新しい actor にその ID を割り当てる（Scene/Prefab から取得済みの
		* actor の識別情報を復元する）。そうでなければ（persistentId が
		* 0、または他の生存中の actor が既に使用中であれば）代わりに
		* 新規IDを自動割り当てする。
		*/
		Actor* CreateActor(String name, Uint32 persistentId = 0);

		/**
		* [EN]
		* Destroys actor's underlying entity and removes it from this world.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* actor の内部エンティティを破棄し、このワールドから削除する。
		*/
		void DestroyActor(Actor* actor);

		/**
		* [EN]
		* Destroys every Actor currently owned by this world.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* このワールドが現在所有している全ての Actor を破棄する。
		*/
		void DestroyActors();

		/**
		* [EN]
		* Returns a mutable reference to the list of Actors owned by this world.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* このワールドが所有する Actor 一覧への変更可能な参照を返す。
		*/
		DynamicArray<ResourcePtr<Actor>>& GetActors();

		/**
		* [EN]
		* Const overload of GetActors().
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* GetActors() の const オーバーロード。
		*/
		const DynamicArray<ResourcePtr<Actor>>& GetActors()const;

		/**
		* [EN]
		* Returns the Actor wrapping entityID, or nullptr if entityID has no Actor.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* entityID を包む Actor を返す。entityID に Actor が無ければ
		* nullptr を返す。
		*/
		Actor* GetActor(EntityID entityID)const;

		/**
		* [EN]
		* Returns the Actor wrapping entity, or nullptr if entity has no Actor.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* entity を包む Actor を返す。entity に Actor が無ければ nullptr
		* を返す。
		*/
		Actor* GetActor(Entity entity)const;

		/**
		* [EN]
		* Returns the first Actor whose Name component's name_ equals
		* name, or nullptr if none match (or the actor has no Name
		* component). Unlike the EntityID/Entity overloads above, this is
		* O(actor count) - there is no name index - so prefer caching the
		* result (or an EntityID/persistent ID) over calling this every
		* frame.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* Name コンポーネントの name_ が name と一致する最初の Actor を
		* 返す。一致するものが無い（または Name コンポーネントを持たない）
		* 場合は nullptr。上の EntityID/Entity 版と違い、名前用の索引は
		* 無いため O(actor数) になる - 毎フレーム呼ぶより、結果(または
		* EntityID/永続ID)をキャッシュしておくことを推奨する。
		*/
		Actor* GetActor(const String& name)const;

		/**
		* [EN]
		* Returns the Actor whose persistent ID is persistentId, or
		* nullptr if no live actor currently holds it.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 永続IDが persistentId である Actor を返す。現在それを保持している
		* 生存中の actor が無ければ nullptr を返す。
		*/
		Actor* FindActor(Uint32 persistentId)const;

		// ============================================================
		// Physics
		// ============================================================

		/**
		* [EN]
		* Creates and returns a new Physics resource owned by this world.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* このワールドが所有する新しい Physics リソースを生成して返す。
		*/
		Physics* CreatePhysics();

		/**
		* [EN]
		* Returns a mutable reference to this world's Physics resource.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* このワールドの Physics リソースへの変更可能な参照を返す。
		*/
		ResourcePtr<Physics>& GetPhysics();

		/**
		* [EN]
		* Const overload of GetPhysics().
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* GetPhysics() の const オーバーロード。
		*/
		const ResourcePtr<Physics>& GetPhysics()const;

	private:
		// ============================================================
		// Component (sparse-set storage helpers)
		// ============================================================

		/**
		* [EN]
		* Returns a mutable reference to the SparseSet<T>, creating its
		* backing storage if it doesn't exist yet.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* SparseSet<T> への変更可能な参照を返す。まだ裏付けストレージが
		* 存在しなければ作成する。
		*/
		template<typename T>
		SparseSet<T>& GetSparseSet()
		{
			if (auto* sparseSet = FindSparseSet<T>())
			{
				return *sparseSet;
			}

			ComponentID id = ComponentRegistry::GetComponentID<T>();

			auto storage = MakePtr<SparseSetStorage<T>>();
			auto* ptr = storage.get();

			sparseSet_.emplace(id, std::move(storage));

			return ptr->data_;
		}

		/**
		* [EN]
		* Returns a pointer to the existing SparseSet<T>, or nullptr if T
		* has no sparse-set storage yet.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 既存の SparseSet<T> へのポインタを返す。T にまだスパースセット
		* ストレージが存在しなければ nullptr を返す。
		*/
		template<typename T>
		SparseSet<T>* FindSparseSet()
		{
			ComponentID id = ComponentRegistry::GetComponentID<T>();

			auto it = sparseSet_.find(id);

			if (it == sparseSet_.end())
			{
				return nullptr;
			}

			return &static_cast<SparseSetStorage<T>*>(it->second.get())->data_;
		}

		/**
		* [EN]
		* Adds a copy of value as entityID's T component in its sparse set.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* value のコピーを、entityID のスパースセット内の T コンポーネント
		* として追加する。
		*/
		template<typename T>
		void AddComponentSparse(EntityID entityID, const T& value)
		{
			auto& sparseSet = GetSparseSet<T>();

			sparseSet.Add(entityID);
			sparseSet.Get(entityID) = value;
		}

		/**
		* [EN]
		* Returns a pointer to entityID's T component in its sparse set,
		* or nullptr if it has none.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* entityID のスパースセット内の T コンポーネントへのポインタを
		* 返す。無ければ nullptr を返す。
		*/
		template<typename T>
		T* GetComponentSparse(EntityID entityID)
		{
			auto* sparseSet = FindSparseSet<T>();

			if (!sparseSet)
			{
				return nullptr;
			}

			if (!sparseSet->Contains(entityID))
			{
				return nullptr;
			}

			return &sparseSet->Get(entityID);
		}

		/**
		* [EN]
		* Type-erased removal from sparse-set storage: removes
		* entityID's entry from the sparse set registered under id, if present.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* スパースセットストレージからの型消去された削除: id に
		* 登録されているスパースセットから entityID のエントリを、
		* 存在すれば削除する。
		*/
		void RemoveComponentSparse(EntityID entityID, ComponentID id);

		/**
		* [EN]
		* Type-erased addition to sparse-set storage: adds a
		* default-constructed entry for entityID to the sparse set
		* registered under id.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* スパースセットストレージへの型消去された追加: id に登録されて
		* いるスパースセットへ、entityID 用のデフォルト構築されたエントリを
		* 追加する。
		*/
		void AddComponentSparse(EntityID entityID, ComponentID id);

		// ============================================================
		// Component (archetype/chunk storage helpers)
		// ============================================================

		/**
		* [EN]
		* Adds the component registered under id to entity's archetype
		* storage, copying size bytes from source and migrating entity
		* to the new (component-extended) archetype.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* id に登録されているコンポーネントを entity のアーキタイプ
		* ストレージへ追加する。source から size バイトをコピーし、
		* entity を新しい（コンポーネントが拡張された）アーキタイプへ
		* 移行する。
		*/
		void AddComponentArchetype(Entity entity, EntityID entityID, ComponentID id, const void* source, Size size);

		/**
		* [EN]
		* Overload of AddComponentArchetype that default-constructs the
		* new component instead of copying from a source buffer.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* ソースバッファからコピーする代わりに、新しいコンポーネントを
		* デフォルト構築する AddComponentArchetype のオーバーロード。
		*/
		void AddComponentArchetype(Entity entity, EntityID entityID, ComponentID id);

		/**
		* [EN]
		* Removes the component registered under id from entity's
		* archetype storage, migrating entity to the new
		* (component-reduced) archetype.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* id に登録されているコンポーネントを entity のアーキタイプ
		* ストレージから削除し、entity を新しい（コンポーネントが
		* 減った）アーキタイプへ移行する。
		*/
		void RemoveComponentArchetype(Entity entity, EntityID entityID, ComponentID id);

		/**
		* [EN]
		* Returns a pointer to entityID's archetype-stored component
		* registered under id, or nullptr if entityID's archetype
		* doesn't include it.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* entityID のアーキタイプ格納コンポーネント（id に登録されて
		* いるもの）へのポインタを返す。entityID のアーキタイプがそれを
		* 含んでいなければ nullptr を返す。
		*/
		Uint8* GetComponentArchetype(EntityID entityID, ComponentID id);

		/**
		* [EN]
		* Returns a non-full chunk of arcketype, creating a new one if
		* every existing chunk is full.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* arcketype の満杯でないチャンクを返す。既存の全チャンクが満杯で
		* あれば新しいチャンクを作成する。
		*/
		Chunk* GetOrCreateChunk(Archetype* arcketype);

		// ============================================================
		// Actor (persistent ID helpers)
		// ============================================================

		/**
		* [EN]
		* Returns the next unused persistent ID and advances the counter.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 次の未使用の永続IDを返し、カウンタを進める。
		*/
		Uint32 AllocatePersistentID();

	private:
		// ============================================================
		// Entity
		// ============================================================

		/// [EN] Stable-pooled storage of Entity handles, providing generation-checked recycling of entity slots.
		/// [JP] Entity ハンドルの安定プール格納。世代チェック付きのエンティティスロット再利用を提供する。
		StablePool<Entity> entityPool_;

		/// [EN] Maps an EntityID to its archetype/chunk/row location.
		/// [JP] EntityID をそのアーキタイプ/チャンク/行の位置へ対応付ける。
		FlatMap<EntityID, EntityRecord> records_;

		// ============================================================
		// Component
		// ============================================================

		/// [EN] Maps each live Archetype to its list of Chunks.
		/// [JP] 現在有効な各 Archetype を、そのチャンク一覧へ対応付ける。
		FlatMap<Archetype*, DynamicArray<Chunk*>> chunks_;

		/// [EN] Maps each sparse-set-stored component's ComponentID to its type-erased storage.
		/// [JP] スパースセット格納の各コンポーネントの ComponentID を、その型消去されたストレージへ対応付ける。
		FlatMap<ComponentID, ResourcePtr<InterfaceSparseSetStorage>> sparseSet_;

		// ============================================================
		// Actor
		// ============================================================

		/// [EN] Every Actor currently owned by this world.
		/// [JP] このワールドが現在所有している全ての Actor。
		DynamicArray<ResourcePtr<Actor>> actors_;

		/// [EN] Maps an EntityID to the Actor wrapping it, for fast GetActor lookups.
		/// [JP] EntityID を、それを包む Actor へ対応付ける。GetActor の高速な検索に使う。
		FlatMap<EntityID, Actor*> actorLookup_;

		/// [EN] Maps a persistent ID to the Actor currently holding it, for FindActor lookups.
		/// [JP] 永続IDを、それを現在保持している Actor へ対応付ける。FindActor の検索に使う。
		FlatMap<Uint32, Actor*> persistentIdLookup_;

		/// [EN] Next persistent ID to hand out from AllocatePersistentID. 0 is reserved as "unassigned", so this starts at 1.
		/// [JP] AllocatePersistentID が次に払い出す永続ID。0 は「未割り当て」として予約されているため、1 から始まる。
		Uint32 nextPersistentId_ = 1;

		// ============================================================
		// Physics
		// ============================================================

		/// [EN] This world's Physics resource.
		/// [JP] このワールドの Physics リソース。
		ResourcePtr<Physics> physics_;
	};

	/**
	* [EN]
	* Adds a copy of value as this actor's T component (T not deriving
	* from ComponentBase), by delegating to World::AddComponent.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* value のコピーをこの actor の T コンポーネント（T は ComponentBase
	* から派生しない）として追加する。World::AddComponent へ委譲する。
	*/
	template<typename T>
		requires(!std::derived_from<T, ComponentBase>)
	void Actor::AddComponent(const T& value)
	{
		world_.AddComponent(entity_, value);
	}

	/**
	* [EN]
	* Default-constructs and adds a T component (T deriving from
	* ComponentBase) to this actor, wiring its ComponentBase fields
	* (actor_/componentName_) and binding each lifecycle function
	* pointer T actually implements. Returns a pointer to the new
	* component, or nullptr on failure.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* T コンポーネント（T は ComponentBase から派生する）をデフォルト
	* 構築してこの actor へ追加し、その ComponentBase フィールド
	* （actor_/componentName_）を設定し、T が実際に実装している各
	* ライフサイクル関数ポインタを束縛する。新しいコンポーネントへの
	* ポインタを返す。失敗時は nullptr を返す。
	*/
	template<typename T>
		requires std::derived_from<T, ComponentBase>
	T* Actor::AddComponent()
	{
		ComponentID id = ComponentRegistry::GetComponentID<T>();
		world_.AddComponent(entity_, T{});
		componentBaseIDs_.push_back(id);

		T* ptr = world_.GetComponent<T>(entity_);
		if (ptr)
		{
			ptr->actor_ = this;
			ptr->componentName_ = ComponentRegistry::GetName(id);
			if constexpr (HasAwake<T>)
			{
				ptr->awake_ = [](ComponentBase* self) { static_cast<T*>(self)->OnAwake(); };
			}
			if constexpr (HasStart<T>)
			{
				ptr->start_ = [](ComponentBase* self) { static_cast<T*>(self)->OnStart(); };
			}
			if constexpr (HasTick<T>)
			{
				ptr->tick_ = [](ComponentBase* self, Float dt) { static_cast<T*>(self)->OnTick(dt); };
			}
			if constexpr (HasFixedTick<T>)
			{
				ptr->fixedTick_ = [](ComponentBase* self, Float dt) { static_cast<T*>(self)->OnFixedTick(dt); };
			}
			if constexpr (HasLateTick<T>)
			{
				ptr->lateTick_ = [](ComponentBase* self, Float dt) { static_cast<T*>(self)->OnLateTick(dt); };
			}
			if constexpr (HasDestroy<T>)
			{
				ptr->destroy_ = [](ComponentBase* self) { static_cast<T*>(self)->OnDestroy(); };
			}
			if constexpr (HasInspectorGUI<T>)
			{
				ptr->inspectorGUI_ = [](ComponentBase* self) { static_cast<T*>(self)->OnInspectorGUI(); };
			}
			if constexpr (HasCollisionEnter<T>)
			{
				ptr->collisionEnter_ = [](ComponentBase* self, Entity other) { static_cast<T*>(self)->OnCollisionEnter(other); };
			}
			if constexpr (HasCollisionStay<T>)
			{
				ptr->collisionStay_ = [](ComponentBase* self, Entity other) { static_cast<T*>(self)->OnCollisionStay(other); };
			}
			if constexpr (HasCollisionExit<T>)
			{
				ptr->collisionExit_ = [](ComponentBase* self, Entity other) { static_cast<T*>(self)->OnCollisionExit(other); };
			}
			if constexpr (HasTriggerEnter<T>)
			{
				ptr->triggerEnter_ = [](ComponentBase* self, Entity other) { static_cast<T*>(self)->OnTriggerEnter(other); };
			}
			if constexpr (HasTriggerStay<T>)
			{
				ptr->triggerStay_ = [](ComponentBase* self, Entity other) { static_cast<T*>(self)->OnTriggerStay(other); };
			}
			if constexpr (HasTriggerExit<T>)
			{
				ptr->triggerExit_ = [](ComponentBase* self, Entity other) { static_cast<T*>(self)->OnTriggerExit(other); };
			}
		}
		return ptr;
	}

	/**
	* [EN]
	* Returns a const pointer to this actor's T component (T not
	* deriving from ComponentBase), by delegating to World::GetComponent.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* この actor の T コンポーネント（T は ComponentBase から派生しない）
	* への const ポインタを返す。World::GetComponent へ委譲する。
	*/
	template<typename T>
		requires(!std::derived_from<T, ComponentBase>)
	const T* Actor::GetComponent()const
	{
		return world_.GetComponent<T>(entity_);
	}

	/**
	* [EN]
	* Returns a mutable pointer to this actor's T component (T deriving
	* from ComponentBase), by delegating to World::GetComponent.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* この actor の T コンポーネント（T は ComponentBase から派生する）
	* への変更可能なポインタを返す。World::GetComponent へ委譲する。
	*/
	template<typename T>
		requires std::derived_from<T, ComponentBase>
	T* Actor::GetComponent()const
	{
		return const_cast<World&>(world_).GetComponent<T>(entity_);
	}
}
