#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/Utility/Handle.h>

namespace SeedCore
{
	using ShapeHandle = Handle<JPH::Shape>;

	/**
	* [EN]
	* Owns all JPH::Shape instances and exposes them through generational handles.
	* Identical parameters return the same handle (shape deduplication),
	* so callers never touch JPH types or lifetimes directly.
	* Each slot is reference-counted; the shape is only destroyed once every
	* owner has called Release.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* すべての JPH::Shape を所有し、世代付きハンドル経由で公開するプール。
	* 同一パラメータなら同じハンドルを返す（形状の重複排除）。
	* 呼び出し側は JPH の型や寿命に直接触れない。
	* 各スロットは参照カウント制で、全ての所有者が Release を呼び終えて
	* はじめて実際に破棄される。
	*/
	class JoltShapePool
	{
	private:
		enum class ShapeKind : Uint64
		{
			Box = 1,
			Sphere = 2,
			Capsule = 3,
			Cylinder = 4, 
			Mesh = 5,
			Convex = 6,
			Rect = 7,
			Circle = 8,
		};

		static constexpr Float flatShapeHalfThickness_ = 0.01f;

	public:
		JoltShapePool() = default;
		~JoltShapePool() = default;

		JoltShapePool(const JoltShapePool&) = delete;
		JoltShapePool& operator=(const JoltShapePool&) = delete;

		ShapeHandle CreateBoxShape(const Vector3& size, const Vector3& center);

		ShapeHandle CreateSphereShape(Float radius);

		ShapeHandle CreateCapsuleShape(Float height, Float radius);

		ShapeHandle CreateCylinderShape(Float height, Float radius);

		ShapeHandle CreateRectShape(const Vector2& size, const Vector2& center);

		ShapeHandle CreateCircleShape(Float radius, const Vector2& center);

		JPH::ShapeRefC Get(ShapeHandle handle)const;

		/**
		* [EN]
		* Increments the reference count by 1. Call this before the owning
		* side holds onto a handle when multiple Colliders share the same shape.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 参照カウントを1増やす。同じ形状を複数のColliderが共有する際、
		* 所有側でハンドルを保持する前に呼ぶ。
		*/
		void AddRef(ShapeHandle handle);

		/**
		* [EN]
		* Decrements the reference count by 1, actually releasing the shape
		* once it reaches 0.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 参照カウントを1減らし、0になったら実際に解放する。
		*/
		void Release(ShapeHandle handle);

		void Clear();

	private:
		struct Slot
		{
			JPH::ShapeRefC shape_;
			Uint64 generation_ = 0;
			Uint64 cacheKey_ = 0;
			Uint32 refCount_ = 0;
		};

		ShapeHandle Register(JPH::ShapeRefC shape, Uint64 cacheKey);

		std::vector<Slot> slots_;
		std::vector<Uint64> freeIndices_;
		std::unordered_map<Uint64, ShapeHandle> cache_;
	};
}
