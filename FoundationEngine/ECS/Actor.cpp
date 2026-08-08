#include <FoundationEngine/ECS/Actor.h>
#include <FoundationEngine/ECS/World.h>
#include <FoundationEngine/ECS/TagRegistry.h>

#include <FoundationEngine/ECS/Component/Name.h>
#include <FoundationEngine/ECS/Component/Position.h>
#include <FoundationEngine/ECS/Component/Rotation.h>
#include <FoundationEngine/ECS/Component/Scale.h>
#include <FoundationEngine/ECS/Component/Velocity.h>
#include <FoundationEngine/ECS/Component/Active.h>
#include <FoundationEngine/ECS/Component/Bounds.h>

namespace SeedCore
{
	/**
	* [EN]
	* Constructs an actor in world with the given display name,
	* creating its underlying entity, physics resource, and the default
	* transform/lifecycle components (Name/Position/Rotation/Scale/
	* Velocity/Active/Bounds).
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 指定された表示名で world 内に actor を構築し、内部エンティティ、
	* 物理リソース、およびデフォルトのトランスフォーム/ライフサイクル
	* コンポーネント（Name/Position/Rotation/Scale/Velocity/Active/Bounds）を
	* 生成する。
	*/
	Actor::Actor(World& world, String name) : world_(world), physics_(*world.CreatePhysics()), entity_(world.CreateEntity())
	{
		/// [EN] Every actor gets the same baseline set of components: a display name, an identity transform, zero velocity, active-by-default, and a small default-sized bounding box (see Bounds's class comment) so it is viewport-pickable even without a renderable component.
		/// [JP] 全ての actor に共通するベースラインのコンポーネント一式を付与する: 表示名、単位トランスフォーム、ゼロ速度、デフォルトでアクティブ、そして小さいデフォルトサイズの境界ボックス（Bounds のクラスコメント参照）— 描画可能なコンポーネントが無くてもビューポートからピック選択できるようにする。
		world_.AddComponent(entity_, Name{ name });
		world_.AddComponent(entity_, Position{ 0.0f,0.0f,0.0f });
		world_.AddComponent(entity_, Rotation{ 0.0f,0.0f,0.0f });
		world_.AddComponent(entity_, Scale{ 1.0f, 1.0f, 1.0f });
		world_.AddComponent(entity_, Velocity{ 0.0f,0.0f,0.0f });
		world_.AddComponent(entity_, Active{ true });
		world_.AddComponent(entity_, Bounds{ Vector3(0.0f, 0.0f, 0.0f), Vector3(0.5f, 0.5f, 0.5f) });
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
		world_.AddComponent(entity_, id);

		const ComponentMetadata& meta = ComponentRegistry::Get(id);
		if (meta.isComponentBase_)
		{
			/// [EN] Track this as a ComponentBase-derived component so lifecycle dispatch (Awake/Start/Tick/Destroy/...) knows to visit it.
			/// [JP] これを ComponentBase 派生コンポーネントとして記録し、ライフサイクルディスパッチ（Awake/Start/Tick/Destroy/...）が巡回対象として認識できるようにする。
			componentBaseIDs_.push_back(id);

			if (meta.setupLifecycle_)
			{
				/// [EN] Bind the concrete type's lifecycle function pointers and set its display name, now that the component actually exists in storage.
				/// [JP] コンポーネントが実際にストレージ上に存在するようになった時点で、具体的な型のライフサイクル関数ポインタを束縛し、表示名を設定する。
				EntityID entityID = static_cast<Uint32>(entity_.GetHandle().index_);
				void* data = world_.GetComponent(entityID, id);
				if (data)
				{
					meta.setupLifecycle_(data, this);
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
		auto it = std::find(componentBaseIDs_.begin(), componentBaseIDs_.end(), id);
		if (it != componentBaseIDs_.end())
		{
			/// [EN] It's a ComponentBase-derived component: fire OnDestroy while its storage is still valid, before the component is actually removed.
			/// [JP] ComponentBase 派生コンポーネントである: 実際にコンポーネントが削除される前、そのストレージがまだ有効なうちに OnDestroy を発火する。
			EntityID entityID = static_cast<Uint32>(entity_.GetHandle().index_);
			void* data = world_.GetComponent(entityID, id);
			if (data)
			{
				ComponentBase* cb = static_cast<ComponentBase*>(data);
				if (cb->destroy_)
				{
					cb->destroy_(cb);
				}
			}
			componentBaseIDs_.erase(it);
		}

		world_.RemoveComponent(entity_, id);
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
		return world_.HasComponent(entity_, id);
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
		return componentBaseIDs_;
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
		return world_;
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
		return physics_;
	}

	/**
	* [EN]
	* Reparents this actor under parent (or to the scene root if
	* nullptr), updating both the old and new parent's children lists,
	* and inheriting the new parent's active state.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* この actor を parent の下へ付け替える（nullptr であればシーンの
	* ルートへ）。旧・新それぞれの親の子リストを更新し、新しい親の
	* アクティブ状態を継承する。
	*/
	void Actor::SetParent(Actor* parent)
	{
		if (parent_ == parent)
		{
			return;
		}

		/// [EN] Detach from the old parent's children list first, if any.
		/// [JP] まず、以前の親（あれば）の子リストから切り離す。
		if (parent_)
		{
			auto& siblings = parent_->children_;
			siblings.erase(std::remove(siblings.begin(), siblings.end(), this), siblings.end());
		}

		parent_ = parent;

		if (parent_)
		{
			/// [EN] Attach to the new parent's children list and inherit its active state if the new parent is currently inactive.
			/// [JP] 新しい親の子リストへ登録し、新しい親が現在非アクティブであれば、そのアクティブ状態を継承する。
			parent_->children_.push_back(this);
			if (!parent_->IsActive())
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
	* Returns this actor's current parent, or nullptr if it has none.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* この actor の現在の親を返す。親が無ければ nullptr を返す。
	*/
	Actor* Actor::GetParent()const
	{
		return parent_;
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
	const DynamicArray<Actor*>& Actor::GetChildren()const
	{
		return children_;
	}

	/**
	* [EN]
	* Repositions child (must already be a child of this Actor) so it comes
	* immediately after after in the children order. Appends to the end if
	* after is not found among the children.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* child（既にこの Actor の子であること）を、子の並び順で after の
	* 直後に来るよう再配置する。after が子の中に見つからない場合は末尾に追加する。
	*/
	void Actor::MoveChildAfter(Actor* child, Actor* after)
	{
		auto childIt = std::find(children_.begin(), children_.end(), child);
		if (childIt == children_.end())
		{
			return;
		}

		/// [EN] Remove child from its current position first, so re-inserting it relative to after doesn't shift after's own index out from under us.
		/// [JP] child を現在の位置から先に削除する。こうすることで、after を基準に再挿入する際、after 自身のインデックスがずれてしまうのを防ぐ。
		children_.erase(childIt);

		auto afterIt = std::find(children_.begin(), children_.end(), after);
		if (afterIt == children_.end())
		{
			/// [EN] after isn't among the children (or was itself just removed): fall back to appending at the end.
			/// [JP] after が子の中に見つからない（または after 自身が今削除された）場合: 末尾への追加にフォールバックする。
			children_.push_back(child);
		}
		else
		{
			children_.insert(afterIt + 1, child);
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
	Bool Actor::Descendant(const Actor* ancestor)const
	{
		const Actor* current = parent_;
		while (current)
		{
			if (current == ancestor)
			{
				return true;
			}
			current = current->parent_;
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
		return worldMatrix_;
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
		worldMatrix_ = matrix;
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
		return comp ? comp->active_ : active_;
	}

	/**
	* [EN]
	* Sets this actor's active state (updating both the cached flag and
	* its Active component if present), and propagates the same state
	* to every descendant.
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
		active_ = active;

		/// [EN] Mirror the change onto the Active component, if this actor has one, so queries reading Active see a consistent value.
		/// [JP] この actor が Active コンポーネントを持っていれば、その変更を反映する。これにより Active を読み取るクエリが一貫した値を見られるようにする。
		Active* comp = const_cast<Active*>(GetComponent<Active>());
		if (comp)
		{
			comp->active_ = active;
		}

		/// [EN] Propagate to every descendant: a child cannot be active while its parent is inactive.
		/// [JP] 全ての子孫へ伝播する: 親が非アクティブである間、子はアクティブになれない。
		for (Actor* child : children_)
		{
			child->SetActive(active);
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
		tags_.set(index);
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
			tags_.reset(index);
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
		return tags_.test(index);
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

		/// [EN] Walk every registered tag slot, skipping removed tags (their bit index may still be set from before removal) and slots this actor doesn't have set.
		/// [JP] 登録済みの全タグスロットを走査し、削除済みタグ（削除前に設定されていたビットが残っている場合がある）と、この actor で立てられていないスロットをスキップする。
		const DynamicArray<String>& names = TagRegistry::GetNames();
		for (Size index = 0; index < names.size(); ++index)
		{
			if (!TagRegistry::IsRemoved(index) && tags_.test(index))
			{
				result.push_back(names[index]);
			}
		}

		return result;
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
		fromPrefab_ = value;
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
		return fromPrefab_;
	}

	/**
	* [EN]
	* Sets the asset ID of the source prefab this actor's instance was
	* created from (root actor only).
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* この actor のインスタンスが生成された元のプレハブの、アセット ID
	* を設定する（ルート actor のみ）。
	*/
	void Actor::SetSourcePrefabAssetID(Uint32 assetID)
	{
		sourcePrefabAssetID_ = assetID;
	}

	/**
	* [EN]
	* Returns the asset ID of the source prefab this actor's instance
	* was created from, or 0 if it's not an instance root.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* この actor のインスタンスが生成された元のプレハブの、アセット ID
	* を返す。インスタンスルートでなければ 0 を返す。
	*/
	Uint32 Actor::GetSourcePrefabAssetID()const
	{
		return sourcePrefabAssetID_;
	}

	/**
	* [EN]
	* Sets this actor's persistent ID. Only World::CreateActor should
	* call this (it resolves collisions and keeps its ID counter in
	* sync); everyone else should treat GetPersistentID() as read-only.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* この actor の永続IDを設定する。衝突解決とIDカウンタの同期を行う
	* World::CreateActor のみがこれを呼ぶべきであり、それ以外は
	* GetPersistentID() を読み取り専用として扱うこと。
	*/
	void Actor::SetPersistentID(Uint32 id)
	{
		persistentId_ = id;
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
		return persistentId_;
	}
}
