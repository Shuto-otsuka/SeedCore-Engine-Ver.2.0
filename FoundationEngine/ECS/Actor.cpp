#include <FoundationEngine/ECS/Actor.h>
#include <FoundationEngine/ECS/World.h>
#include <FoundationEngine/ECS/TagRegistry.h>
#include <FoundationEngine/ECS/LayerRegistry.h>

#include <FoundationEngine/ECS/Component/Active.h>

namespace SeedCore
{
	/**
	* [EN]
	* Constructs a handle to the actor identified by entity within world.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* world 内で entity が識別する actor へのハンドルを構築する。
	*/
	Actor::Actor(World& world, Entity entity) : world_(&world), entity_(entity)
	{
		/// No Code
	}

	/**
	* [EN]
	* Returns whether this handle refers to a live actor.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* このハンドルが生存中の actor を指しているかどうかを返す。
	*/
	Actor::operator Bool()const
	{
		return world_ != nullptr && world_->HasActor(entity_);
	}

	/**
	* [EN]
	* Type-erased overload of AddComponent: adds a default-constructed
	* instance of the component registered under id, wiring up its
	* lifecycle if it derives from ComponentBase.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* AddComponent の型消去版: id に登録されているコンポーネントの
	* デフォルト構築されたインスタンスを追加する。ComponentBase から
	* 派生していれば、そのライフサイクルを配線する。
	*/
	void Actor::AddComponent(ComponentID id)
	{
		if (!world_)
		{
			return;
		}

		world_->AddComponent(entity_, id);

		const ComponentMetadata& meta = ComponentRegistry::Get(id);
		if (meta.isComponentBase_)
		{
			/// [EN] Track this as a ComponentBase-derived component so lifecycle dispatch (Awake/Start/Tick/Destroy/...) knows to visit it.
			/// [JP] これを ComponentBase 派生コンポーネントとして記録し、ライフサイクルディスパッチ（Awake/Start/Tick/Destroy/...）が巡回対象として認識できるようにする。
			world_->GetActorRecord(entity_).componentBaseIDs_.push_back(id);

			if (meta.setupLifecycle_)
			{
				/// [EN] Bind the concrete type's lifecycle function pointers and set its display name, now that the component actually exists in storage.
				/// [JP] コンポーネントが実際にストレージ上に存在するようになった時点で、具体的な型のライフサイクル関数ポインタを束縛し、表示名を設定する。
				void* data = world_->GetComponent(entity_.GetID(), id);
				if (data)
				{
					meta.setupLifecycle_(data, world_, entity_);
					static_cast<ComponentBase*>(data)->componentName_ = ComponentRegistry::GetName(id);
				}
			}
		}
	}

	/**
	* [EN]
	* Removes this actor's component registered under id, invoking its
	* OnDestroy first if it derives from ComponentBase.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* この actor の、id に登録されているコンポーネントを削除する。
	* ComponentBase から派生していれば、先に OnDestroy を呼び出す。
	*/
	void Actor::RemoveComponent(ComponentID id)
	{
		if (!world_)
		{
			return;
		}

		DynamicArray<ComponentID>& componentBaseIDs = world_->GetActorRecord(entity_).componentBaseIDs_;
		auto it = std::ranges::find(componentBaseIDs, id);
		if (it != componentBaseIDs.end())
		{
			/// [EN] It's a ComponentBase-derived component: fire OnDestroy while its storage is still valid, before the component is actually removed.
			/// [JP] ComponentBase 派生コンポーネントである: 実際にコンポーネントが削除される前、そのストレージがまだ有効なうちに OnDestroy を発火する。
			void* data = world_->GetComponent(entity_.GetID(), id);
			if (data)
			{
				ComponentBase* cb = static_cast<ComponentBase*>(data);
				if (cb->destroy_)
				{
					cb->destroy_(cb);
				}
			}
			componentBaseIDs.erase(it);
		}

		world_->RemoveComponent(entity_, id);
	}

	/**
	* [EN]
	* Returns whether this actor currently has the component registered
	* under id.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* この actor が現在、id に登録されているコンポーネントを持っているか
	* どうかを返す。
	*/
	Bool Actor::HasComponent(ComponentID id)const
	{
		return world_ != nullptr && world_->HasComponent(entity_, id);
	}

	/**
	* [EN]
	* Returns the IDs of every ComponentBase-derived component currently
	* attached to this actor.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* この actor に現在アタッチされている、全ての ComponentBase 派生
	* コンポーネントの ID 一覧を返す。
	*/
	const DynamicArray<ComponentID>& Actor::ComponentBaseIDList()const
	{
		return world_->GetActorRecord(entity_).componentBaseIDs_;
	}

	/**
	* [EN]
	* Returns this actor's underlying entity handle.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* この actor の内部エンティティハンドルを返す。
	*/
	Entity Actor::GetEntity()const
	{
		return entity_;
	}

	/**
	* [EN]
	* Returns the World that owns this actor.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* この actor を所有する World を返す。
	*/
	World& Actor::GetWorld()const
	{
		return *world_;
	}

	/**
	* [EN]
	* Returns this actor's Physics resource.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* この actor の Physics リソースを返す。
	*/
	Physics& Actor::GetPhysics()const
	{
		return *world_->CreatePhysics();
	}

	/**
	* [EN]
	* Reparents this actor under parent (or to the scene root if parent
	* is invalid), updating both the old and new parent's children lists,
	* and inheriting the new parent's active state.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* この actor を parent の下へ付け替える（parent が無効であればシーンの
	* ルートへ）。旧・新それぞれの親の子リストを更新し、新しい親の
	* アクティブ状態を継承する。
	*/
	void Actor::SetParent(Actor parent)
	{
		if (!world_)
		{
			return;
		}

		Entity parentEntity = parent ? parent.GetEntity() : Entity::Null();

		ActorRecord& record = world_->GetActorRecord(entity_);
		if (record.parent_ == parentEntity)
		{
			return;
		}

		/// [EN] Detach from the old parent's children list first, if any.
		/// [JP] まず、以前の親（あれば）の子リストから切り離す。
		if (record.parent_.Exists())
		{
			std::erase(world_->GetActorRecord(record.parent_).children_, entity_);
		}

		record.parent_ = parentEntity;

		if (parentEntity.Exists())
		{
			/// [EN] Attach to the new parent's children list and inherit its active state if the new parent is currently inactive.
			/// [JP] 新しい親の子リストへ登録し、新しい親が現在非アクティブであれば、そのアクティブ状態を継承する。
			world_->GetActorRecord(parentEntity).children_.push_back(entity_);
			if (!Actor(*world_, parentEntity).IsActive())
			{
				SetActive(false);
			}
		}
		else
		{
			/// [EN] No new parent (reparented to the scene root): always become active again.
			/// [JP] 新しい親が無い（シーンのルートへ再親化された）場合: 常に再びアクティブになる。
			SetActive(true);
		}
	}

	/**
	* [EN]
	* Returns this actor's current parent, or an invalid Actor if it has
	* none.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* この actor の現在の親を返す。親が無ければ無効な Actor を返す。
	*/
	Actor Actor::GetParent()const
	{
		return Actor(*world_, world_->GetActorRecord(entity_).parent_);
	}

	/**
	* [EN]
	* Returns this actor's direct children, in their current order.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* この actor の直接の子一覧を、現在の順序で返す。
	*/
	DynamicArray<Actor> Actor::GetChildren()const
	{
		DynamicArray<Actor> result;

		const DynamicArray<Entity>& children = world_->GetActorRecord(entity_).children_;
		result.reserve(children.size());
		for (Entity child : children)
		{
			result.push_back(Actor(*world_, child));
		}

		return result;
	}

	/**
	* [EN]
	* Repositions child (must already be a child of this Actor) so it
	* comes immediately after after in the children order. An invalid
	* after moves child to the front; an after that isn't among the
	* children appends it to the end.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* child（既にこの Actor の子であること）を、子の並び順で after の
	* 直後に来るよう再配置する。after が無効なら先頭へ、after が子の
	* 中に見つからない場合は末尾に移動する。
	*/
	void Actor::MoveChild(Actor child, Actor after)
	{
		if (!world_)
		{
			return;
		}

		DynamicArray<Entity>& children = world_->GetActorRecord(entity_).children_;

		auto childIt = std::ranges::find(children, child.GetEntity());
		if (childIt == children.end())
		{
			return;
		}

		/// [EN] Remove child from its current position first, so re-inserting it relative to after doesn't shift after's own index out from under us.
		/// [JP] child を現在の位置から先に削除する。こうすることで、after を基準に再挿入する際、after 自身のインデックスがずれてしまうのを防ぐ。
		children.erase(childIt);

		if (!after)
		{
			/// [EN] An invalid anchor means "move to the front of the sibling list".
			/// [JP] アンカーが無効の場合は「兄弟リストの先頭へ移動」を意味する。
			children.insert(children.begin(), child.GetEntity());
			return;
		}

		auto afterIt = std::ranges::find(children, after.GetEntity());
		if (afterIt == children.end())
		{
			/// [EN] after isn't among the children: fall back to appending at the end.
			/// [JP] after が子の中に見つからない場合: 末尾への追加にフォールバックする。
			children.push_back(child.GetEntity());
		}
		else
		{
			children.insert(afterIt + 1, child.GetEntity());
		}
	}

	/**
	* [EN]
	* Returns whether ancestor is somewhere in this actor's parent chain.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* ancestor がこの actor の親チェーンのどこかに存在するかどうかを
	* 返す。
	*/
	Bool Actor::Descendant(Actor ancestor)const
	{
		if (!world_ || !ancestor)
		{
			return false;
		}

		Entity ancestorEntity = ancestor.GetEntity();
		Entity current = world_->GetActorRecord(entity_).parent_;
		while (current.Exists())
		{
			if (current == ancestorEntity)
			{
				return true;
			}
			current = world_->GetActorRecord(current).parent_;
		}
		return false;
	}

	/**
	* [EN]
	* Returns this actor's cached world-space transform matrix.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* この actor の、キャッシュされたワールド空間変換行列を返す。
	*/
	const Matrix& Actor::GetWorldMatrix()const
	{
		return world_->GetActorRecord(entity_).worldMatrix_;
	}

	/**
	* [EN]
	* Sets this actor's cached world-space transform matrix.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* この actor の、キャッシュされたワールド空間変換行列を設定する。
	*/
	void Actor::SetWorldMatrix(const Matrix& matrix)
	{
		world_->GetActorRecord(entity_).worldMatrix_ = matrix;
	}

	/**
	* [EN]
	* Returns whether this actor is currently active (preferring its
	* Active component's value if present, otherwise the cached flag).
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* この actor が現在アクティブかどうかを返す（Active コンポーネントが
	* 存在すればその値を優先し、無ければキャッシュされたフラグを使う）。
	*/
	Bool Actor::IsActive()const
	{
		const Active* comp = GetComponent<Active>();
		return comp ? comp->active_ : world_->GetActorRecord(entity_).active_;
	}

	/**
	* [EN]
	* Sets this actor's active state (updating both the cached flag and
	* its Active component if present), and propagates the same state to
	* every descendant.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* この actor のアクティブ状態を設定する（キャッシュされたフラグと、
	* 存在すれば Active コンポーネントの両方を更新する）。同じ状態を
	* 全ての子孫へ伝播する。
	*/
	void Actor::SetActive(Bool active)
	{
		if (!world_)
		{
			return;
		}

		world_->GetActorRecord(entity_).active_ = active;

		/// [EN] Mirror the change onto the Active component, if this actor has one, so queries reading Active see a consistent value.
		/// [JP] この actor が Active コンポーネントを持っていれば、その変更を反映する。これにより Active を読み取るクエリが一貫した値を見られるようにする。
		Active* comp = const_cast<Active*>(GetComponent<Active>());
		if (comp)
		{
			comp->active_ = active;
		}

		/// [EN] Propagate to every descendant: a child cannot be active while its parent is inactive.
		/// [JP] 全ての子孫へ伝播する: 親が非アクティブである間、子はアクティブになれない。
		DynamicArray<Entity> children = world_->GetActorRecord(entity_).children_;
		for (Entity child : children)
		{
			Actor(*world_, child).SetActive(active);
		}
	}

	/**
	* [EN]
	* Adds tag to this actor's tag set, registering it in TagRegistry if
	* it's new.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* tag をこの actor のタグ集合へ追加する。新規のタグであれば
	* TagRegistry へ登録する。
	*/
	void Actor::AddTag(String tag)
	{
		Size index = TagRegistry::GetOrCreate(tag);
		world_->GetActorRecord(entity_).tags_.set(index);
	}

	/**
	* [EN]
	* Removes tag from this actor's tag set, if present.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* tag をこの actor のタグ集合から、存在すれば削除する。
	*/
	void Actor::RemoveTag(String tag)
	{
		Size index = TagRegistry::Find(tag);
		if (index != TagRegistry::InvalidIndex)
		{
			world_->GetActorRecord(entity_).tags_.reset(index);
		}
	}

	/**
	* [EN]
	* Returns whether this actor currently has tag.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* この actor が現在 tag を持っているかどうかを返す。
	*/
	Bool Actor::HasTag(String tag)const
	{
		Size index = TagRegistry::Find(tag);
		if (index == TagRegistry::InvalidIndex)
		{
			return false;
		}
		return world_->GetActorRecord(entity_).tags_.test(index);
	}

	/**
	* [EN]
	* Returns the names of every tag currently set on this actor.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* この actor に現在設定されている全タグの名前一覧を返す。
	*/
	DynamicArray<String> Actor::GetTagList()const
	{
		DynamicArray<String> result;

		const Bitset& tags = world_->GetActorRecord(entity_).tags_;

		/// [EN] Walk every registered tag slot, skipping removed tags and slots this actor doesn't have set.
		/// [JP] 登録済みの全タグスロットを走査し、削除済みタグと、この actor で立てられていないスロットをスキップする。
		const DynamicArray<String>& names = TagRegistry::GetNames();
		for (Size index = 0; index < names.size(); ++index)
		{
			if (!TagRegistry::IsRemoved(index) && tags.test(index))
			{
				result.push_back(names[index]);
			}
		}

		return result;
	}

	/**
	* [EN]
	* Sets this actor's layer to the LayerRegistry slot at index (clamped
	* to a valid slot if out of range).
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* この actor のレイヤーを、index の LayerRegistry スロットに設定する
	* （範囲外なら有効なスロットへクランプする）。
	*/
	void Actor::SetLayer(Size index)
	{
		world_->GetActorRecord(entity_).layer_ = Min(index, LayerRegistry::LayerCount - 1);
	}

	/**
	* [EN]
	* Sets this actor's layer by name via LayerRegistry::Find, falling
	* back to LayerRegistry::DefaultLayer if name isn't registered.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* LayerRegistry::Find 経由で、名前でこの actor のレイヤーを設定する。
	* name が未登録であれば LayerRegistry::DefaultLayer にフォールバック
	* する。
	*/
	void Actor::SetLayer(const String& name)
	{
		Size index = LayerRegistry::Find(name);
		world_->GetActorRecord(entity_).layer_ = (index != LayerRegistry::InvalidIndex) ? index : LayerRegistry::DefaultLayer;
	}

	/**
	* [EN]
	* Returns this actor's current layer index.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* この actor の現在のレイヤーインデックスを返す。
	*/
	Size Actor::GetLayer()const
	{
		return world_->GetActorRecord(entity_).layer_;
	}

	/**
	* [EN]
	* Returns this actor's current layer's name, via LayerRegistry::GetName.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* LayerRegistry::GetName 経由で、この actor の現在のレイヤー名を返す。
	*/
	const String& Actor::GetLayerName()const
	{
		return LayerRegistry::GetName(world_->GetActorRecord(entity_).layer_);
	}

	/**
	* [EN]
	* Marks whether this actor was instantiated from a prefab.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* この actor がプレハブからインスタンス化されたかどうかをマークする。
	*/
	void Actor::SetPrefabInstance(Bool value)
	{
		world_->GetActorRecord(entity_).fromPrefab_ = value;
	}

	/**
	* [EN]
	* Returns whether this actor was instantiated from a prefab.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* この actor がプレハブからインスタンス化されたかどうかを返す。
	*/
	Bool Actor::IsPrefabInstance()const
	{
		return world_->GetActorRecord(entity_).fromPrefab_;
	}

	/**
	* [EN]
	* Sets the asset ID of the source prefab this actor's instance was
	* created from (root actor only).
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* この actor のインスタンスが生成された元のプレハブの、アセット ID を
	* 設定する（ルート actor のみ）。
	*/
	void Actor::SetSourcePrefabAssetID(Uint32 assetID)
	{
		world_->GetActorRecord(entity_).sourcePrefabAssetID_ = assetID;
	}

	/**
	* [EN]
	* Returns the asset ID of the source prefab this actor's instance was
	* created from, or 0 if it's not an instance root.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* この actor のインスタンスが生成された元のプレハブの、アセット ID を
	* 返す。インスタンスルートでなければ 0 を返す。
	*/
	Uint32 Actor::GetSourcePrefabAssetID()const
	{
		return world_->GetActorRecord(entity_).sourcePrefabAssetID_;
	}

	/**
	* [EN]
	* Sets this actor's persistent ID. Only World::CreateActor should
	* call this.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* この actor の永続IDを設定する。World::CreateActor のみがこれを
	* 呼ぶべき。
	*/
	void Actor::SetPersistentID(Uint32 id)
	{
		world_->GetActorRecord(entity_).persistentId_ = id;
	}

	/**
	* [EN]
	* Returns this actor's persistent ID: a World-wide unique identifier
	* that round-trips through Scene/Prefab serialization. 0 means unassigned.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* この actor の永続IDを返す: World 全体で一意な識別子であり、
	* Scene/Prefab のシリアライズを往復する。0 は未割り当てを意味する。
	*/
	Uint32 Actor::GetPersistentID()const
	{
		return world_->GetActorRecord(entity_).persistentId_;
	}

	/**
	* [EN]
	* Two actor handles are equal iff they refer to the same World and Entity.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 2つの actor ハンドルは、同じ World と Entity を指す場合にのみ等しい。
	*/
	Bool Actor::operator==(const Actor& other)const
	{
		return world_ == other.world_ && entity_ == other.entity_;
	}
}
