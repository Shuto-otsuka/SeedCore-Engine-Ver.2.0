#include <PhysicsEngine/JoltPhysics/JoltConstraintPool.h>

namespace SeedCore
{
	/**
	* [EN]
	* Registers a constraint and returns its generational handle.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 拘束を登録し、その世代付きハンドルを返す。
	*/
	Handle<JPH::Constraint> JoltConstraintPool::Add(JPH::PhysicsSystem& system, JPH::Constraint* constraint)
	{
		if (!constraint)
		{
			return Handle<JPH::Constraint>::null();
		}

		system.AddConstraint(constraint);

		/// [EN] Reuse a vacant slot before growing the pool.
		/// [JP] プールを拡張する前に空きスロットを再利用する。
		Uint64 index = 0;
		if (!freeIndices_.empty())
		{
			index = freeIndices_.back();
			freeIndices_.pop_back();
		}
		else
		{
			index = slots_.size();
			slots_.emplace_back();
		}

		Slot& slot = slots_[index];
		slot.constraint_ = constraint;

		Handle<JPH::Constraint> handle{};
		handle.index_ = index;
		handle.generation_ = slot.generation_;
		return handle;
	}

	/**
	* [EN]
	* Removes and releases the constraint referenced by a valid handle.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 有効なハンドルが参照する拘束を削除して解放する。
	*/
	void JoltConstraintPool::Release(JPH::PhysicsSystem& system, Handle<JPH::Constraint> handle)
	{
		if (handle.empty() || handle.index_ >= slots_.size())
		{
			return;
		}

		/// [EN] Reject stale generations before removing the live constraint.
		/// [JP] 有効な拘束を削除する前に、古い世代のハンドルを拒否する。
		Slot& slot = slots_[handle.index_];
		if (slot.generation_ != handle.generation_ || slot.constraint_ == nullptr)
		{
			return;
		}

		system.RemoveConstraint(slot.constraint_);
		slot.constraint_ = nullptr;
		++slot.generation_;
		freeIndices_.push_back(handle.index_);
	}

	/**
	* [EN]
	* Removes every live constraint and clears all pool storage.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* すべての有効な拘束を削除し、プールのストレージを空にする。
	*/
	void JoltConstraintPool::Clear(JPH::PhysicsSystem& system)
	{
		for (Slot& slot : slots_)
		{
			if (slot.constraint_ != nullptr)
			{
				system.RemoveConstraint(slot.constraint_);
				slot.constraint_ = nullptr;
			}
			++slot.generation_;
		}
		slots_.clear();
		freeIndices_.clear();
	}

	/**
	* [EN]
	* Enables constraints whose two bodies currently participate in the world.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 両方のボディが現在ワールドへ参加している拘束を有効にする。
	*/
	void JoltConstraintPool::Refresh()
	{
		/// [EN] Only two-body constraints depend on the participation of both attached bodies.
		/// [JP] 接続された両ボディの参加状態に依存するのは二体拘束だけ。
		for (Slot& slot : slots_)
		{
			if (slot.constraint_ == nullptr || slot.constraint_->GetType() != JPH::EConstraintType::TwoBodyConstraint)
			{
				continue;
			}

			JPH::TwoBodyConstraint* constraint = static_cast<JPH::TwoBodyConstraint*>(slot.constraint_.GetPtr());
			const JPH::Body* body1 = constraint->GetBody1();
			const JPH::Body* body2 = constraint->GetBody2();

			Bool body1InWorld = body1->GetID().IsInvalid() || body1->IsInBroadPhase();
			Bool body2InWorld = body2->GetID().IsInvalid() || body2->IsInBroadPhase();

			constraint->SetEnabled(body1InWorld && body2InWorld);
		}
	}
}
