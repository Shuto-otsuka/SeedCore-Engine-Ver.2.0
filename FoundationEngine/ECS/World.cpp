#include <FoundationEngine/ECS/World.h>
#include <FoundationEngine/ECS/Actor.h>

namespace SeedCore
{
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
	Entity World::CreateEntity()
	{
		Handle<Entity> handle = entityPool_.Create();
		EntityID id = static_cast<Uint32>(handle.index_);

		/// [EN] Every new entity starts in the empty archetype (no components) and is migrated as components are added.
		/// [JP] 新しいエンティティはすべて、空のアーキタイプ（コンポーネントなし）から開始し、コンポーネントが追加されるたびに移行される。
		DynamicArray<ComponentID> emptyLayout;
		Archetype* archetype = ArchetypeRegistry::GetOrCreate(emptyLayout);
		Chunk* chunk = GetOrCreateChunk(archetype);

		chunk->Add(id);
		Uint32 row = chunk->EntityCount() - 1;
		records_[id] = { archetype,chunk,row };
		return Entity(handle);
	}

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
	void World::DestroyEntity(Entity entity)
	{
		EntityID entityID = static_cast<Uint32>(entity.GetHandle().index_);

		/// [EN] Drop entityID's entry from every sparse-set-stored component, regardless of whether it actually has one (Remove is a no-op if absent).
		/// [JP] スパースセット格納の全コンポーネントから entityID のエントリを削除する。実際に持っているかどうかに関わらず（無ければ Remove は無操作）。
		for (auto& [componentID, storage] : sparseSet_)
		{
			storage->Remove(entityID);
		}

		auto it = records_.find(entityID);
		if (it == records_.end())
		{
			return;
		}

		EntityRecord& record = it->second;

		/// [EN] Swap-remove from the chunk: if another entity got moved into the vacated row, fix up its cached row index.
		/// [JP] チャンクから swap-remove する: 空いた行へ別のエンティティが移動してきた場合、そのキャッシュされた行インデックスを修正する。
		Uint32 movedID = record.chunk_->Remove(entityID);

		if (movedID != UINT32_MAX)
		{
			records_[movedID].row_ = record.row_;
		}

		records_.erase(it);

		entityPool_.Destroy(entity.GetHandle());
	}

	/**
	* [EN]
	* Moves entityID's archetype-stored components from sourceChunk (of
	* sourceArcketype) into a chunk of destArcketype, updating its
	* EntityRecord. Components common to both layouts are move-
	* constructed across; components only in sourceArcketype are
	* destructed (dropped); the vacated source row is filled via
	* swap-remove and the moved entity's record is fixed up.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* entityID のアーキタイプ格納コンポーネントを、sourceArcketype の
	* sourceChunk から destArcketype のチャンクへ移動し、その
	* EntityRecord を更新する。両方のレイアウトに共通するコンポーネントは
	* ムーブ構築で引き継がれる。sourceArcketype にのみ存在する
	* コンポーネントは破棄（削除）される。空いたソース側の行は
	* swap-remove で埋められ、移動したエンティティのレコードが修正される。
	*/
	void World::MoveEntity(EntityID entityID, Archetype* sourceArcketype, Chunk* sourceChunk, Archetype* destArcketype)
	{
		EntityRecord& record = records_.at(entityID);

		Uint32 sourceRow = record.row_;
		Chunk* destChunk = GetOrCreateChunk(destArcketype);

		Uint32 destRow = destChunk->EntityCount();
		destChunk->Add(entityID);

		const DynamicArray<ComponentID>& sourceLayout = sourceArcketype->Layout();
		const DynamicArray<ComponentID>& destLayout = destArcketype->Layout();

		/// [EN] Move every component shared by both layouts from the source slot into the freshly-added destination slot.
		/// [JP] 両方のレイアウトで共有される各コンポーネントを、ソースのスロットから新しく追加された宛先のスロットへムーブする。
		for (Size destIndex = 0; destIndex < destLayout.size(); ++destIndex)
		{
			ComponentID componentID = destLayout[destIndex];
			for (Size sourceIndex = 0; sourceIndex < sourceLayout.size(); ++sourceIndex)
			{
				if (sourceLayout[sourceIndex] == componentID)
				{
					const ComponentMetadata& meta = ComponentRegistry::Get(componentID);
					Uint8* source = sourceChunk->Data(sourceIndex) + (meta.size_ * sourceRow);
					Uint8* dest = destChunk->Data(destIndex) + (meta.size_ * destRow);
					meta.destruct_(dest);
					meta.move_(dest, source);
					break;
				}
			}
		}

		/// [EN] Re-construct (as placeholders) the source slots of components that made it to the destination, so the chunk's later Remove() destructs valid objects instead of the just-moved-from state.
		/// [JP] 宛先へ移った各コンポーネントのソース側スロットを（プレースホルダーとして）再構築する。これにより、後でチャンクの Remove() が呼ばれた際、ムーブ済みの状態ではなく有効なオブジェクトを破棄することになる。
		for (Size sourceIndex = 0; sourceIndex < sourceLayout.size(); ++sourceIndex)
		{
			ComponentID componentID = sourceLayout[sourceIndex];
			Bool movedToDest = false;
			for (const ComponentID& destID : destLayout)
			{
				if (destID == componentID) { movedToDest = true; break; }
			}
			if (movedToDest)
			{
				const ComponentMetadata& meta = ComponentRegistry::Get(componentID);
				Uint8* slot = sourceChunk->Data(sourceIndex) + (meta.size_ * sourceRow);
				meta.construct_(slot);
			}
		}

		Uint32 movedID = sourceChunk->Remove(entityID);

		if (movedID != UINT32_MAX)
		{
			auto it = records_.find(movedID);
			if (it != records_.end())
			{
				it->second.row_ = sourceRow;
			}
		}

		record.archetype_ = destArcketype;
		record.chunk_ = destChunk;
		record.row_ = destRow;
	}

	/**
	* [EN]
	* Returns the ordered list of component types making up entity's archetype.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* entity のアーキタイプを構成する、順序付きのコンポーネント型
	* 一覧を返す。
	*/
	const DynamicArray<ComponentID>& World::GetLayout(Entity entity)const
	{
		EntityID entityID = static_cast<Uint32>(entity.GetHandle().index_);

		auto it = records_.find(entityID);

		return it->second.archetype_->Layout();
	}

	// ============================================================
	// Component
	// ============================================================

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
	void World::AddComponent(Entity entity, ComponentID id)
	{
		EntityID entityID = static_cast<Uint32>(entity.GetHandle().index_);
		const ComponentMetadata& meta = ComponentRegistry::Get(id);

		if (meta.storage_ == ComponentStorage::SparseSet)
		{
			AddComponentSparse(entityID, id);
		}
		else
		{
			AddComponentArchetype(entity, entityID, id);
		}
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
	void* World::GetComponent(Entity entity, ComponentID id)
	{
		EntityID entityID = static_cast<Uint32>(entity.GetHandle().index_);
		const ComponentMetadata& meta = ComponentRegistry::Get(id);

		if (meta.storage_ == ComponentStorage::SparseSet)
		{
			auto it = sparseSet_.find(id);

			if (it == sparseSet_.end())
			{
				return nullptr;
			}

			return it->second->Raw(entityID);
		}

		auto it = records_.find(entityID);
		if (it == records_.end())
		{
			return nullptr;
		}

		const EntityRecord& record = it->second;
		const DynamicArray<ComponentID>& layout = record.archetype_->Layout();

		for (Size index = 0; index < layout.size(); ++index)
		{
			if (layout[index] == id)
			{
				Uint8* base = record.chunk_->Data(index);
				base += record.chunk_->Length(index) * record.row_;
				return base;
			}
		}

		return nullptr;
	}

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
	void* World::GetComponent(EntityID entityID, ComponentID id)
	{
		const ComponentMetadata& meta = ComponentRegistry::Get(id);

		if (meta.storage_ == ComponentStorage::SparseSet)
		{
			auto it = sparseSet_.find(id);

			if (it == sparseSet_.end())
			{
				return nullptr;
			}

			return it->second->Raw(entityID);
		}

		return GetComponentArchetype(entityID, id);
	}

	/**
	* [EN]
	* Type-erased overload of HasComponent: returns whether entity has
	* the component registered under id.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* HasComponent の型消去版: entity が id に登録されている
	* コンポーネントを持っているかどうかを返す。
	*/
	Bool World::HasComponent(Entity entity, ComponentID id)const
	{
		EntityID entityID = static_cast<Uint32>(entity.GetHandle().index_);
		const ComponentMetadata& meta = ComponentRegistry::Get(id);

		if (meta.storage_ == ComponentStorage::SparseSet)
		{
			auto it = sparseSet_.find(id);
			if (it == sparseSet_.end())
			{
				return false;
			}
			return it->second->Contains(entityID);
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
	* Removes entity's component registered under id, if present.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* entity の、id に登録されているコンポーネントを、存在すれば
	* 削除する。
	*/
	void World::RemoveComponent(Entity entity, ComponentID id)
	{
		EntityID entityID = static_cast<Uint32>(entity.GetHandle().index_);
		const ComponentMetadata& meta = ComponentRegistry::Get(id);

		if (meta.storage_ == ComponentStorage::SparseSet)
		{
			RemoveComponentSparse(entityID, id);
		}
		else
		{
			RemoveComponentArchetype(entity, entityID, id);
		}
	}

	void World::UnregisterSparseSetStorage(ComponentID id)
	{
		sparseSet_.erase(id);
	}

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
	const FlatMap<Archetype*, DynamicArray<Chunk*>>& World::GetChunk()const
	{
		return chunks_;
	}

	// ============================================================
	// Actor
	// ============================================================

	/**
	* [EN]
	* Creates a new entity wrapped in an Actor with the given display
	* name, and returns a pointer to it. If persistentId is nonzero and
	* not already taken, the new actor is assigned that ID; otherwise a
	* fresh ID is auto-allocated instead.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 指定された表示名を持つ Actor で包まれた新しいエンティティを生成し、
	* そのポインタを返す。persistentId が非ゼロかつ未使用であれば、新しい
	* actor にその ID を割り当てる。そうでなければ代わりに新規IDを
	* 自動割り当てする。
	*/
	Actor* World::CreateActor(String name, Uint32 persistentId)
	{
		auto actor = MakePtr<Actor>(*this, name);
		Actor* ptr = actor.get();
		actors_.push_back(std::move(actor));

		EntityID entityID = static_cast<Uint32>(ptr->GetEntity().GetHandle().index_);
		actorLookup_[entityID] = ptr;

		/// [EN] Fall back to auto-allocation if persistentId is unset (0) or already claimed by another live actor (e.g. the same prefab instantiated twice), so two actors never collide on the same ID.
		/// [JP] persistentId が未設定(0)、または既に他の生存中の actor に使用されている(例: 同じ prefab を2回インスタンス化した場合)場合は、自動割り当てにフォールバックする。これにより2つの actor が同じIDで衝突することはない。
		Uint32 resolvedId = persistentId;
		if (resolvedId == 0 || persistentIdLookup_.find(resolvedId) != persistentIdLookup_.end())
		{
			resolvedId = AllocatePersistentID();
		}

		ptr->SetPersistentID(resolvedId);
		persistentIdLookup_[resolvedId] = ptr;

		/// [EN] Keep the auto-allocation counter ahead of any explicitly-restored ID so future allocations never collide with it.
		/// [JP] 明示的に復元されたIDより自動割り当てカウンタが常に先に進むようにし、今後の割り当てがそのIDと衝突しないようにする。
		if (resolvedId >= nextPersistentId_)
		{
			nextPersistentId_ = resolvedId + 1;
		}

		return ptr;
	}

	/**
	* [EN]
	* Destroys actor's underlying entity and removes it from this world.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* actor の内部エンティティを破棄し、このワールドから削除する。
	*/
	void World::DestroyActor(Actor* actor)
	{
		/// [EN] Detach every child before detaching actor itself, so no child is left pointing at a destroyed parent.
		/// [JP] actor 自身を親から切り離す前に、全ての子を切り離す。破棄済みの親を指す子が残らないようにする。
		while (!actor->GetChildren().empty())
		{
			Actor* child = actor->GetChildren().back();
			child->SetParent(nullptr);
		}

		actor->SetParent(nullptr);

		/// [EN] Invoke OnDestroy on every ComponentBase-derived component this actor holds before tearing down its entity.
		/// [JP] エンティティを解体する前に、この actor が保持する全ての ComponentBase 派生コンポーネントに対して OnDestroy を呼び出す。
		EntityID entityID = static_cast<Uint32>(actor->GetEntity().GetHandle().index_);
		for (ComponentID id : actor->ComponentBaseIDList())
		{
			void* data = GetComponent(entityID, id);
			if (data)
			{
				ComponentBase* component = static_cast<ComponentBase*>(data);
				if (component->destroy_)
				{
					component->destroy_(component);
				}
			}
		}

		auto actorLookupIt = actorLookup_.find(entityID);
		if (actorLookupIt != actorLookup_.end())
		{
			actorLookup_.erase(actorLookupIt);
		}

		auto persistentIdLookupIt = persistentIdLookup_.find(actor->GetPersistentID());
		if (persistentIdLookupIt != persistentIdLookup_.end())
		{
			persistentIdLookup_.erase(persistentIdLookupIt);
		}

		DestroyEntity(actor->GetEntity());

		auto it = std::find_if(actors_.begin(), actors_.end(), [actor](const ResourcePtr<Actor>& a) { return a.get() == actor; });

		if (it != actors_.end())
		{
			actors_.erase(it);
		}
	}

	/**
	* [EN]
	* Destroys every Actor currently owned by this world.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* このワールドが現在所有している全ての Actor を破棄する。
	*/
	void World::DestroyActors()
	{
		while (!actors_.empty())
		{
			DestroyActor(actors_.back().get());
		}
	}

	/**
	* [EN]
	* Returns a mutable reference to the list of Actors owned by this world.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* このワールドが所有する Actor 一覧への変更可能な参照を返す。
	*/
	DynamicArray<ResourcePtr<Actor>>& World::GetActors()
	{
		return actors_;
	}

	/**
	* [EN]
	* Const overload of GetActors().
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* GetActors() の const オーバーロード。
	*/
	const DynamicArray<ResourcePtr<Actor>>& World::GetActors()const
	{
		return actors_;
	}

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
	Actor* World::GetActor(EntityID entityID)const
	{
		auto it = actorLookup_.find(entityID);
		if (it == actorLookup_.end())
		{
			return nullptr;
		}
		return it->second;
	}

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
	Actor* World::GetActor(Entity entity)const
	{
		return GetActor(static_cast<Uint32>(entity.GetHandle().index_));
	}

	/**
	* [EN]
	* Returns the Actor whose persistent ID is persistentId, or nullptr
	* if no live actor currently holds it.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 永続IDが persistentId である Actor を返す。現在それを保持している
	* 生存中の actor が無ければ nullptr を返す。
	*/
	Actor* World::FindActor(Uint32 persistentId)const
	{
		auto it = persistentIdLookup_.find(persistentId);
		if (it == persistentIdLookup_.end())
		{
			return nullptr;
		}
		return it->second;
	}

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
	Physics* World::CreatePhysics()
	{
		if (!physics_)
		{
			physics_ = MakePtr<Physics>();
		}

		return physics_.get();
	}

	/**
	* [EN]
	* Destroys physics, releasing it back to this world's resource management.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* physics を破棄し、このワールドのリソース管理へ返却する。
	*/
	void World::DestroyPhysics(Physics* physics)
	{

	}

	/**
	* [EN]
	* Returns a mutable reference to this world's Physics resource.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* このワールドの Physics リソースへの変更可能な参照を返す。
	*/
	ResourcePtr<Physics>& World::GetPhysics()
	{
		return physics_;
	}

	/**
	* [EN]
	* Const overload of GetPhysics().
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* GetPhysics() の const オーバーロード。
	*/
	const ResourcePtr<Physics>& World::GetPhysics()const
	{
		return physics_;
	}

	// ============================================================
	// Component (sparse-set storage helpers)
	// ============================================================

	/**
	* [EN]
	* Type-erased removal from sparse-set storage: removes entityID's
	* entry from the sparse set registered under id, if present.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* スパースセットストレージからの型消去された削除: id に登録
	* されているスパースセットから entityID のエントリを、存在すれば
	* 削除する。
	*/
	void World::RemoveComponentSparse(EntityID entityID, ComponentID id)
	{
		auto it = sparseSet_.find(id);

		if (it == sparseSet_.end())
		{
			return;
		}

		it->second->Remove(entityID);
	}

	/**
	* [EN]
	* Type-erased addition to sparse-set storage: adds a
	* default-constructed entry for entityID to the sparse set
	* registered under id, creating the storage on first use.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* スパースセットストレージへの型消去された追加: id に登録されている
	* スパースセットへ、entityID 用のデフォルト構築されたエントリを
	* 追加する。初回使用時にストレージを作成する。
	*/
	void World::AddComponentSparse(EntityID entityID, ComponentID id)
	{
		auto it = sparseSet_.find(id);
		if (it == sparseSet_.end())
		{
			const ComponentMetadata& meta = ComponentRegistry::Get(id);
			if (!meta.createSparseStorage_)
			{
				return;
			}
			auto storage = meta.createSparseStorage_();
			it = sparseSet_.emplace(id, std::move(storage)).first;
		}
		it->second->Add(entityID);
	}

	// ============================================================
	// Component (archetype/chunk storage helpers)
	// ============================================================

	/**
	* [EN]
	* Adds the component registered under id to entity's archetype
	* storage, copying size bytes from source. If entity's current
	* archetype already includes id, overwrites the existing slot in
	* place; otherwise builds the new (component-extended) archetype,
	* migrates entity to it via MoveEntity, then copies source into the
	* newly-available slot.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* id に登録されているコンポーネントを entity のアーキタイプ
	* ストレージへ追加し、source から size バイトをコピーする。entity の
	* 現在のアーキタイプが既に id を含んでいれば、既存のスロットをその場で
	* 上書きする。そうでなければ、新しい（コンポーネントが拡張された）
	* アーキタイプを構築し、MoveEntity 経由で entity をそこへ移行してから、
	* 新たに使用可能になったスロットへ source をコピーする。
	*/
	void World::AddComponentArchetype(Entity entity, EntityID entityID, ComponentID id, const void* source, Size size)
	{
		auto it = records_.find(entityID);
		if (it == records_.end())
		{
			return;
		}

		EntityRecord& record = it->second;
		const ComponentMetadata& meta = ComponentRegistry::Get(id);

		/// [EN] Fast path: id is already part of entity's archetype, so no migration is needed — just overwrite the existing slot in place.
		/// [JP] 高速経路: id は既に entity のアーキタイプに含まれているため、移行は不要 — 既存のスロットをその場で上書きするだけでよい。
		const DynamicArray<ComponentID>& layout = record.archetype_->Layout();
		for (Size index = 0; index < layout.size(); ++index)
		{
			if (layout[index] == id)
			{
				Uint8* base = record.chunk_->Data(index) + (meta.size_ * record.row_);
				meta.destruct_(base);
				meta.copy_(base, source);
				return;
			}
		}

		/// [EN] id isn't part of the current archetype: build the extended layout (kept sorted by ComponentID for stable archetype hashing) and migrate.
		/// [JP] id は現在のアーキタイプに含まれていない: 拡張されたレイアウトを構築し（安定したアーキタイプハッシュのため ComponentID でソート順を維持する）、移行する。
		DynamicArray<ComponentID> newLayout1 = record.archetype_->Layout();
		newLayout1.push_back(id);
		std::sort(newLayout1.begin(), newLayout1.end(),
			[](ComponentID a, ComponentID b)
			{
				return reinterpret_cast<std::uintptr_t>(a) < reinterpret_cast<std::uintptr_t>(b);
			});

		Archetype* newArchetype = ArchetypeRegistry::GetOrCreate(newLayout1);
		MoveEntity(entityID, record.archetype_, record.chunk_, newArchetype);

		/// [EN] The migrated slot for id was default-constructed by MoveEntity/Chunk::Add; overwrite it with the actual source value.
		/// [JP] id 用に移行されたスロットは MoveEntity/Chunk::Add によってデフォルト構築されている。実際の source の値で上書きする。
		EntityRecord& newRecord = records_.at(entityID);
		const DynamicArray<ComponentID>& newLayout2 = newRecord.archetype_->Layout();
		for (Size index = 0; index < newLayout2.size(); ++index)
		{
			if (newLayout2[index] == id)
			{
				Uint8* base = newRecord.chunk_->Data(index) + (meta.size_ * newRecord.row_);
				meta.destruct_(base);
				meta.copy_(base, source);
				return;
			}
		}
	}

	/**
	* [EN]
	* Overload of AddComponentArchetype that default-constructs the new
	* component instead of copying from a source buffer: if entity's
	* archetype already includes id, does nothing (Add is a no-op for
	* an already-present component); otherwise builds the extended
	* archetype and migrates entity to it (the freshly-added chunk slot
	* is already default-constructed by Chunk::Add).
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* ソースバッファからコピーする代わりに、新しいコンポーネントを
	* デフォルト構築する AddComponentArchetype のオーバーロード:
	* entity のアーキタイプが既に id を含んでいれば何もしない
	* （既に存在するコンポーネントに対する Add は無操作）。そうでなければ
	* 拡張されたアーキタイプを構築して entity をそこへ移行する
	* （新たに追加されたチャンクのスロットは Chunk::Add によって既に
	* デフォルト構築されている）。
	*/
	void World::AddComponentArchetype(Entity entity, EntityID entityID, ComponentID id)
	{
		auto it = records_.find(entityID);
		if (it == records_.end())
		{
			return;
		}

		EntityRecord& record = it->second;

		const DynamicArray<ComponentID>& layout = record.archetype_->Layout();
		for (Size index = 0; index < layout.size(); ++index)
		{
			if (layout[index] == id)
			{
				return;
			}
		}

		DynamicArray<ComponentID> newLayout = record.archetype_->Layout();
		newLayout.push_back(id);
		std::sort(newLayout.begin(), newLayout.end(),
			[](ComponentID a, ComponentID b)
			{
				return reinterpret_cast<std::uintptr_t>(a) < reinterpret_cast<std::uintptr_t>(b);
			});

		Archetype* newArchetype = ArchetypeRegistry::GetOrCreate(newLayout);
		MoveEntity(entityID, record.archetype_, record.chunk_, newArchetype);
	}

	/**
	* [EN]
	* Removes the component registered under id from entity's archetype
	* storage: if entity's archetype doesn't include id, does nothing;
	* otherwise builds the reduced layout and migrates entity to it via
	* MoveEntity.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* id に登録されているコンポーネントを entity のアーキタイプ
	* ストレージから削除する: entity のアーキタイプが id を含んでいなければ
	* 何もしない。そうでなければ、減らされたレイアウトを構築し、
	* MoveEntity 経由で entity をそこへ移行する。
	*/
	void World::RemoveComponentArchetype(Entity entity, EntityID entityID, ComponentID id)
	{
		auto it = records_.find(entityID);
		if (it == records_.end())
		{
			return;
		}

		EntityRecord& record = it->second;

		const DynamicArray<ComponentID>& layout = record.archetype_->Layout();
		Bool found = false;
		for (const ComponentID& cid : layout)
		{
			if (cid == id)
			{
				found = true;
				break;
			}
		}
		if (!found)
		{
			return;
		}

		/// [EN] Build the reduced layout by copying every component except id (already sorted, since it's a subset of the sorted source layout).
		/// [JP] id 以外の全コンポーネントをコピーして、減らされたレイアウトを構築する（ソート済みのソースレイアウトの部分集合であるため、既にソート済みになる）。
		DynamicArray<ComponentID> newLayout;
		for (const ComponentID& cid : layout)
		{
			if (cid != id)
			{
				newLayout.push_back(cid);
			}
		}

		Archetype* newArchetype = ArchetypeRegistry::GetOrCreate(newLayout);
		MoveEntity(entityID, record.archetype_, record.chunk_, newArchetype);
	}

	/**
	* [EN]
	* Returns a pointer to entityID's archetype-stored component
	* registered under id, or nullptr if entityID's archetype doesn't
	* include it.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* entityID のアーキタイプ格納コンポーネント（id に登録されている
	* もの）へのポインタを返す。entityID のアーキタイプがそれを含んで
	* いなければ nullptr を返す。
	*/
	Uint8* World::GetComponentArchetype(EntityID entityID, ComponentID id)
	{
		auto it = records_.find(entityID);
		if (it == records_.end())
		{
			return nullptr;
		}

		const EntityRecord& record = it->second;
		const DynamicArray<ComponentID>& layout = record.archetype_->Layout();

		for (Size index = 0;index < layout.size();++index)
		{
			if (layout[index] == id)
			{
				Uint8* base = record.chunk_->Data(index);
				base += record.chunk_->Length(index) * record.row_;
				return base;
			}
		}

		return nullptr;
	}

	/**
	* [EN]
	* Returns a non-full chunk of arcketype, creating a new one if every
	* existing chunk is full.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* arcketype の満杯でないチャンクを返す。既存の全チャンクが満杯で
	* あれば新しいチャンクを作成する。
	*/
	Chunk* World::GetOrCreateChunk(Archetype* arcketype)
	{
		DynamicArray<Chunk*>& chunkList = chunks_[arcketype];

		/// [EN] Reuse the first chunk with spare capacity, if any.
		/// [JP] 空き容量のある最初のチャンクがあれば、それを再利用する。
		for (Chunk* chunk : chunkList)
		{
			if (!chunk->Full())
			{
				return chunk;
			}
		}

		/// [EN] Every existing chunk is full: allocate a fresh one and register it with this archetype.
		/// [JP] 既存の全チャンクが満杯: 新しいチャンクを確保し、このアーキタイプへ登録する。
		Chunk* newChunk = arcketype->CreateChunk();
		chunkList.push_back(newChunk);
		return newChunk;
	}

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
	Uint32 World::AllocatePersistentID()
	{
		return nextPersistentId_++;
	}
}
