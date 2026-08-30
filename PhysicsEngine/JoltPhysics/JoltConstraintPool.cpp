#include <PhysicsEngine/JoltPhysics/JoltConstraintPool.h>

namespace SeedCore
{
	ConstraintHandle JoltConstraintPool::Add(JPH::PhysicsSystem& system, JPH::Constraint* constraint)
	{
		if (!constraint)
		{
			return ConstraintHandle::null();
		}

		system.AddConstraint(constraint);

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

		ConstraintHandle handle{};
		handle.index_ = index;
		handle.generation_ = slot.generation_;
		return handle;
	}

	void JoltConstraintPool::Release(JPH::PhysicsSystem& system, ConstraintHandle handle)
	{
		if (handle.empty() || handle.index_ >= slots_.size())
		{
			return;
		}

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
}
