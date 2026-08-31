#include <FoundationEngine/ECS/CommandBuffer.h>
#include <FoundationEngine/ECS/World.h>
#include <FoundationEngine/ECS/Actor.h>
#include <FoundationEngine/ECS/Component/Position.h>
#include <FoundationEngine/Resource/ResourceCache.h>
#include <FoundationEngine/Resource/PrefabPool.h>
#include <FoundationEngine/Resource/Prefab.h>
#include <FoundationEngine/Utility/Handle.h>

namespace SeedCore
{
	/**
	* [EN]
	* Records the creation of a bare (componentless) entity and returns a
	* provisional EntityID standing in for it until Flush.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* コンポーネントを持たないエンティティの生成を記録し、Flush まで
	* それを代理する暫定 EntityID を返す。
	*/
	EntityID CommandBuffer::CreateEntity()
	{
		EntityID provisional = EntityID{ provisionalCount_++, ProvisionalGeneration };

		Command command;
		command.kind_ = Kind::CreateEntity;
		command.target_ = provisional;
		commands_.push_back(command);

		return provisional;
	}

	/**
	* [EN]
	* Records a deferred prefab instantiation at position and returns a
	* provisional EntityID for the spawned root.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* position でのプレハブのインスタンス化を遅延記録し、生成される
	* ルートの暫定 EntityID を返す。
	*/
	EntityID CommandBuffer::SpawnPrefab(Uint32 assetID, const Vector3& position, EntityID parent)
	{
		EntityID provisional = EntityID{ provisionalCount_++, ProvisionalGeneration };

		Command command;
		command.kind_ = Kind::SpawnPrefab;
		command.target_ = provisional;
		command.parent_ = parent;
		command.assetID_ = assetID;
		command.position_ = position;
		commands_.push_back(command);

		return provisional;
	}

	/**
	* [EN]
	* Records the destruction of id.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* id の破棄を記録する。
	*/
	void CommandBuffer::DestroyEntity(EntityID id)
	{
		Command command;
		command.kind_ = Kind::DestroyEntity;
		command.target_ = id;
		commands_.push_back(command);
	}

	/**
	* [EN]
	* Type-erased AddComponent: copies the component at source into blob_
	* and records the pending add.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 型消去版 AddComponent: source のコンポーネントを blob_ へコピーし、
	* 保留中の追加を記録する。
	*/
	void CommandBuffer::AddComponent(EntityID id, ComponentID componentID, const void* source)
	{
		const ComponentMetadata& meta = ComponentRegistry::Get(componentID);

		Size cellCount = (meta.size_ + sizeof(BlobCell) - 1) / sizeof(BlobCell);
		Size cellOffset = blob_.size();
		blob_.resize(blob_.size() + cellCount);
		meta.copy_(blob_.data() + cellOffset, source);

		Command command;
		command.kind_ = Kind::AddComponent;
		command.target_ = id;
		command.componentID_ = componentID;
		command.dataOffset_ = cellOffset;
		commands_.push_back(command);
	}

	/**
	* [EN]
	* Records removing id's component registered under componentID.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* componentID に登録された id のコンポーネントの削除を記録する。
	*/
	void CommandBuffer::RemoveComponent(EntityID id, ComponentID componentID)
	{
		Command command;
		command.kind_ = Kind::RemoveComponent;
		command.target_ = id;
		command.componentID_ = componentID;
		commands_.push_back(command);
	}

	/**
	* [EN]
	* Returns whether no commands are recorded.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* コマンドが1つも記録されていないかどうかを返す。
	*/
	Bool CommandBuffer::Empty()const
	{
		return commands_.empty();
	}

	/**
	* [EN]
	* Replays every recorded command against world in record order, then
	* clears the command list while keeping the provisional-to-real map.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 記録された全コマンドを world に対して記録順で再生し、暫定→実際の
	* 対応表を残したままコマンドリストをクリアする。
	*/
	void CommandBuffer::Flush(World& world, ResourceCache& cache)
	{
		/// [EN] resolved_ is not cleared here: a system that spawned last frame reads it on its next Execute, which runs before this frame's Flush. Provisional indices restart at 0 every Flush, so a reused index simply overwrites its stale entry; Clear/Reset wipe the map fully.
		/// [JP] resolved_ はここではクリアしない: 前フレームに生成したシステムは、このフレームの Flush より前に走る次の Execute でこれを読む。暫定インデックスは Flush ごとに 0 から振り直されるため、再利用されたインデックスは古いエントリを上書きするだけ。完全な消去は Clear/Reset で行う。
		for (const Command& command : commands_)
		{
			EntityID target = command.target_;
			if (IsProvisional(target))
			{
				auto it = resolved_.find(target.index_);
				target = (it != resolved_.end()) ? it->second : EntityID{};
			}

			switch (command.kind_)
			{
			case Kind::CreateEntity:
			{
				Entity entity = world.CreateEntity();
				resolved_[command.target_.index_] = entity.GetID();
				break;
			}
			case Kind::SpawnPrefab:
			{
				EntityID parentID = command.parent_;
				if (IsProvisional(parentID))
				{
					auto it = resolved_.find(parentID.index_);
					parentID = (it != resolved_.end()) ? it->second : EntityID{};
				}

				Actor parent = (parentID != EntityID{}) ? world.GetActor(parentID) : Actor();

				Handle<Prefab> handle = cache.GetPrefabPool().Load(command.assetID_, cache);
				Prefab* prefab = cache.GetPrefabPool().Get(handle);
				if (prefab == nullptr)
				{
					break;
				}

				Actor spawned = prefab->Instantiate(world, cache, parent, command.assetID_);
				if (!spawned)
				{
					break;
				}

				resolved_[command.target_.index_] = spawned.GetEntity().GetID();

				Position* position = world.GetComponent<Position>(spawned.GetEntity());
				if (position != nullptr)
				{
					position->x_ = command.position_.x;
					position->y_ = command.position_.y;
					position->z_ = command.position_.z;
				}
				break;
			}
			case Kind::DestroyEntity:
			{
				if (target == EntityID{})
				{
					break;
				}

				Actor actor = world.GetActor(target);
				if (actor)
				{
					world.DestroyActor(actor);
				}
				else
				{
					world.DestroyEntity(Entity(Handle<Entity>{ target.index_, target.generation_ }));
				}
				break;
			}
			case Kind::AddComponent:
			{
				if (target == EntityID{})
				{
					break;
				}

				const ComponentMetadata& meta = ComponentRegistry::Get(command.componentID_);
				Entity entity(Handle<Entity>{ target.index_, target.generation_ });

				world.AddComponent(entity, command.componentID_);

				void* destination = world.GetComponent(target, command.componentID_);
				if (destination != nullptr)
				{
					void* recorded = blob_.data() + command.dataOffset_;
					meta.destruct_(destination);
					meta.copy_(destination, recorded);
				}
				break;
			}
			case Kind::RemoveComponent:
			{
				if (target == EntityID{})
				{
					break;
				}

				world.RemoveComponent(Entity(Handle<Entity>{ target.index_, target.generation_ }), command.componentID_);
				break;
			}
			}
		}

		/// [EN] Destruct the recorded component copies now that they have been replayed into the World.
		/// [JP] 記録されたコンポーネントのコピーを、World へ再生し終えたこの時点で破棄する。
		for (const Command& command : commands_)
		{
			if (command.kind_ == Kind::AddComponent)
			{
				const ComponentMetadata& meta = ComponentRegistry::Get(command.componentID_);
				meta.destruct_(blob_.data() + command.dataOffset_);
			}
		}

		commands_.clear();
		blob_.clear();
		provisionalCount_ = 0;
	}

	/**
	* [EN]
	* Returns the real EntityID the provisional ID resolved to in the
	* last Flush, or an invalid EntityID.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 暫定 ID が直近の Flush で解決された実際の EntityID を返す。無ければ
	* 無効な EntityID を返す。
	*/
	EntityID CommandBuffer::Resolved(EntityID provisional)const
	{
		if (!IsProvisional(provisional))
		{
			return EntityID{};
		}

		auto it = resolved_.find(provisional.index_);
		return (it != resolved_.end()) ? it->second : EntityID{};
	}

	/**
	* [EN]
	* Drops every recorded command and the provisional-to-real map.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 記録された全コマンドと、暫定→実際の対応表を破棄する。
	*/
	void CommandBuffer::Clear()
	{
		commands_.clear();
		blob_.clear();
		resolved_.clear();
		provisionalCount_ = 0;
	}

	/**
	* [EN]
	* Returns whether id is a provisional ID handed out by a CommandBuffer.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* id が CommandBuffer が発行した暫定 ID かどうかを返す。
	*/
	Bool CommandBuffer::IsProvisional(EntityID id)
	{
		return id.generation_ == ProvisionalGeneration;
	}
}
