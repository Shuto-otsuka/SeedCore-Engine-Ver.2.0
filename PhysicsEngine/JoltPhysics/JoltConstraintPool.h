#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/Utility/Handle.h>

namespace SeedCore
{
	using ConstraintHandle = Handle<JPH::Constraint>;

	class JoltConstraintPool
	{
	public:
		JoltConstraintPool() = default;
		~JoltConstraintPool() = default;

		JoltConstraintPool(const JoltConstraintPool&) = delete;
		JoltConstraintPool& operator=(const JoltConstraintPool&) = delete;

		ConstraintHandle Add(JPH::PhysicsSystem& system, JPH::Constraint* constraint);

		void Release(JPH::PhysicsSystem& system, ConstraintHandle handle);

		void Clear(JPH::PhysicsSystem& system);

	private:
		struct Slot
		{
			JPH::Ref<JPH::Constraint> constraint_;
			Uint64 generation_ = 0;
		};

	private:
		DynamicArray<Slot> slots_;

		DynamicArray<Uint64> freeIndices_;
	};
}
