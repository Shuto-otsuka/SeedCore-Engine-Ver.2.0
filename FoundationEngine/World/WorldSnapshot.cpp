#include <FoundationEngine/World/WorldSnapshot.h>
#include <FoundationEngine/World/World.h>
#include <FoundationEngine/World/Actor/Actor.h>
#include <FoundationEngine/World/ECS/Component/Component.h>
#include <FoundationEngine/World/ECS/Component/ComponentRegistry.h>

namespace SeedCore
{
	/**
	* [EN]
	* Clears any existing snapshot, then captures a copy of every
	* Actor's component data currently in world.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 既存のスナップショットをクリアし、world 内の現在の全 Actor の
	* コンポーネントデータのコピーを取得する。
	*/
	void WorldSnapshot::Capture(World& world)
	{
		Clear();

		const DynamicArray<Actor>& actors = world.GetActors();

		for (Actor actor : actors)
		{
			ActorData actorData;

			Entity entity = actor.GetEntity();
			const auto& layout = world.GetLayout(entity);

			/// [EN] Copy every archetype-stored component in the actor's layout into a freshly-allocated byte buffer, via the component's own copy_ function.
			/// [JP] actor のレイアウト内の全アーキタイプ格納コンポーネントを、そのコンポーネント自身の copy_ 関数経由で、新規確保したバイトバッファへコピーする。
			for (Size index = 0; index < layout.size(); ++index)
			{
				ComponentID id = layout[index];
				const ComponentMetadata& meta = ComponentRegistry::Get(id);

				void* source = world.GetComponent(entity, id);
				if (!source)
				{
					continue;
				}

				ComponentData componentData;
				componentData.id_ = id;
				componentData.data_.resize(meta.size_);
				meta.copy_(componentData.data_.data(), source);

				actorData.components_.push_back(std::move(componentData));
			}

			/// [EN] GetLayout() only covers archetype components, so walk every registered type and keep the sparse-set ones this actor has (Rigidbody, Softbody, Weather...),
			///      so their runtime values are restored on Stop as well.
			/// [JP] GetLayout() はアーキタイプのコンポーネントしか返さないので、登録済みの全型を回り、この actor が持つスパースセットのもの(Rigidbody、Softbody、Weather など)を残す。
			///      これで実行中に変わった値も Stop で元に戻る。
			for (const auto& [componentName, componentID] : ComponentRegistry::ComponentList())
			{
				const ComponentMetadata& meta = ComponentRegistry::Get(componentID);
				if (meta.storage_ != ComponentStorage::SparseSet)
				{
					continue;
				}

				void* source = world.GetComponent(entity, componentID);
				if (!source)
				{
					continue;
				}

				ComponentData componentData;
				componentData.id_ = componentID;
				componentData.data_.resize(meta.size_);
				meta.copy_(componentData.data_.data(), source);

				actorData.components_.push_back(std::move(componentData));
			}

			/// [EN] Also remember which of those components derive from ComponentBehaviour, so Restore can reset their lifecycle state afterward.
			/// [JP] それらのコンポーネントのうちどれが ComponentBehaviour から派生しているかも記録し、Restore が後でそのライフサイクル状態をリセットできるようにする。
			actorData.componentBaseIDs_ = actor.ComponentIDList();

			actors_.push_back(std::move(actorData));
		}

		hasData_ = true;
	}

	/**
	* [EN]
	* If a snapshot has been captured, copies its component data back
	* onto world's actors (matched by index) and resets each
	* ComponentBehaviour's lifecycle state, destroys any actor beyond the
	* captured count (i.e. created after the snapshot, such as by
	* SpawnerSystem during Play), then clears the snapshot. Does nothing
	* if no snapshot exists.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* スナップショットが取得済みであれば、そのコンポーネントデータを
	* （インデックスで対応付けた）world の actor 群へコピーし戻し、各
	* ComponentBehaviour のライフサイクル状態をリセットし、取得済み数を超える
	* actor（スナップショット後に生成されたもの、例えば Play 中の
	* SpawnerSystem による生成）を破棄した上で、スナップショットを
	* クリアする。スナップショットが存在しなければ何もしない。
	*/
	void WorldSnapshot::Restore(World& world)
	{
		if (!hasData_)
		{
			return;
		}

		const DynamicArray<Actor>& actors = world.GetActors();

		/// [EN] Restore only pairs up to whichever list is shorter, in case actors were created/destroyed since the snapshot was captured.
		/// [JP] スナップショット取得後に actor が生成/破棄されている可能性があるため、どちらか短い方のリストの長さまでのみ対応付けて復元する。
		Size count = (actors_.size() < actors.size()) ? actors_.size() : actors.size();

		for (Size actorIndex = 0; actorIndex < count; ++actorIndex)
		{
			const ActorData& actorData = actors_[actorIndex];
			Actor actor = actors[actorIndex];
			Entity entity = actor.GetEntity();

			/// [EN] Dispatch OnDestroy to every ComponentBehaviour-derived component first, while its live data (e.g. a Rigidbody's JPH body/shape handles) is still intact, so play-mode-only resources are released before the snapshot bytes overwrite it.
			/// [JP] ComponentBehaviour 派生の全コンポーネントへ、まず OnDestroy をディスパッチする。生きたデータ（Rigidbody の JPH ボディ/シェイプハンドルなど）がまだ有効なうちに、プレイモード限定のリソースをスナップショットのバイト列で上書きする前に解放するため。
			for (ComponentID id : actorData.componentBaseIDs_)
			{
				void* data = world.GetComponent(entity, id);
				if (data)
				{
					static_cast<ComponentBehaviour*>(data)->DispatchDestroy();
				}
			}

			/// [EN] Overwrite each current component in place with the captured bytes: destruct the live value first, then copy-construct from the snapshot.
			/// [JP] 現在の各コンポーネントを、取得済みのバイト列でその場で上書きする: まず現在の値を破棄し、その後スナップショットからコピー構築する。
			for (const ComponentData& componentData : actorData.components_)
			{
				const ComponentMetadata& meta = ComponentRegistry::Get(componentData.id_);
				void* dest = world.GetComponent(entity, componentData.id_);
				if (!dest)
				{
					continue;
				}

				meta.destruct_(dest);
				meta.copy_(dest, componentData.data_.data());
			}

			/// [EN] Reset Awake/Start flags on every ComponentBehaviour-derived component, so they fire again on the next frame as if freshly attached.
			/// [JP] 全ての ComponentBehaviour 派生コンポーネントの Awake/Start フラグをリセットし、次のフレームで新規アタッチされたかのように再び発火するようにする。
			for (ComponentID id : actorData.componentBaseIDs_)
			{
				EntityID entityID = entity.GetID();
				void* data = world.GetComponent(entityID, id);
				if (data)
				{
					ComponentBehaviour* component = static_cast<ComponentBehaviour*>(data);
					component->ResetLifecycle();
				}
			}
		}

		/// [EN] Actors beyond the captured count were created during Play (e.g. by SpawnerSystem); destroy them so Stop fully restores the world.
		///      Go from the end backward, since DestroyActor erases from actors_ at once and would shift later indices.
		/// [JP] 取得数を超える actor は Play 中に生まれたもの(例: SpawnerSystem)なので、Stop で world を完全に戻すため破棄する。
		///      DestroyActor は actors_ から即座に消して後ろの番号をずらすので、末尾から逆順に消す。
		for (Size actorIndex = actors.size(); actorIndex > count; --actorIndex)
		{
			world.DestroyActor(actors[actorIndex - 1]);
		}

		Clear();
	}

	/**
	* [EN]
	* Returns whether a snapshot is currently held.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* スナップショットを現在保持しているかどうかを返す。
	*/
	Bool WorldSnapshot::HasData()const
	{
		return hasData_;
	}

	/**
	* [EN]
	* Destructs and discards all captured component data.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 取得済みの全コンポーネントデータを破棄・削除する。
	*/
	void WorldSnapshot::Clear()
	{
		/// [EN] Destruct every captured component's live representation in its byte buffer before releasing the buffers themselves, to avoid leaking any owned resources (e.g. Strings) inside them.
		/// [JP] バッファ自体を解放する前に、取得済みの各コンポーネントのバイトバッファ内にある有効な表現を破棄する。中に保持されているリソース（String など）のリークを防ぐため。
		for (ActorData& actorData : actors_)
		{
			for (ComponentData& compData : actorData.components_)
			{
				const ComponentMetadata& meta = ComponentRegistry::Get(compData.id_);
				meta.destruct_(compData.data_.data());
			}
		}
		actors_.clear();
		hasData_ = false;
	}
}
