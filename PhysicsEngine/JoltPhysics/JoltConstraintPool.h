#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/Utility/Handle.h>

namespace SeedCore
{
	/**
	* [EN]
	* Owns Jolt constraints in reusable generational slots and keeps their
	* registration with a Jolt physics system synchronized.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* Jolt 拘束を再利用可能な世代付きスロットで所有し、Jolt 物理システムへの
	* 登録状態を同期する。
	*/
	class JoltConstraintPool
	{
	public:
		/**
		* [EN]
		* Creates an empty constraint pool.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 空の拘束プールを生成する。
		*/
		JoltConstraintPool() = default;

		/**
		* [EN]
		* Destroys the pool storage.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* プールのストレージを破棄する。
		*/
		~JoltConstraintPool() = default;

		/**
		* [EN]
		* Disables copying so constraint ownership remains unique.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 拘束の所有権を一意に保つためコピーを禁止する。
		*/
		JoltConstraintPool(const JoltConstraintPool&) = delete;

		/**
		* [EN]
		* Disables copy assignment so constraint ownership remains unique.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 拘束の所有権を一意に保つためコピー代入を禁止する。
		*/
		JoltConstraintPool& operator=(const JoltConstraintPool&) = delete;

		/**
		* [EN]
		* Registers a constraint and returns its generational handle.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 拘束を登録し、その世代付きハンドルを返す。
		*/
		Handle<JPH::Constraint> Add(JPH::PhysicsSystem& system, JPH::Constraint* constraint);

		/**
		* [EN]
		* Removes and releases the constraint referenced by a valid handle.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 有効なハンドルが参照する拘束を削除して解放する。
		*/
		void Release(JPH::PhysicsSystem& system, Handle<JPH::Constraint> handle);

		/**
		* [EN]
		* Removes every live constraint and clears all pool storage.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* すべての有効な拘束を削除し、プールのストレージを空にする。
		*/
		void Clear(JPH::PhysicsSystem& system);

		/**
		* [EN]
		* Enables constraints whose two bodies currently participate in the world.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 両方のボディが現在ワールドへ参加している拘束を有効にする。
		*/
		void Refresh();

	private:
		/**
		* [EN]
		* Stores one constraint reference and the generation of its slot.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 1つの拘束参照と、そのスロットの世代を保持する。
		*/
		struct Slot
		{
			/// [EN] Reference-counted constraint occupying this slot.
			/// [JP] このスロットを使用している参照カウント付き拘束。
			JPH::Ref<JPH::Constraint> constraint_;

			/// [EN] Generation used to reject stale handles.
			/// [JP] 古いハンドルを拒否するための世代。
			Uint64 generation_ = 0;
		};

		/// [EN] Constraint slots indexed by generational handles.
		/// [JP] 世代付きハンドルのインデックスで参照する拘束スロット。
		DynamicArray<Slot> slots_;

		/// [EN] Vacant slot indices available for reuse.
		/// [JP] 再利用可能な空きスロットのインデックス。
		DynamicArray<Uint64> freeIndices_;
	};
}
