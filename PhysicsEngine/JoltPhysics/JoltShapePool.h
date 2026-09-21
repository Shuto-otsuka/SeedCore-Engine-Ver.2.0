#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/Utility/Handle.h>

namespace SeedCore
{
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
	public:
		/**
		* [EN]
		* Constructs an empty shape pool.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 空の形状プールを構築する。
		*/
		JoltShapePool() = default;

		/**
		* [EN]
		* Destroys the shape pool.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 形状プールを破棄する。
		*/
		~JoltShapePool() = default;

		JoltShapePool(const JoltShapePool&) = delete;
		JoltShapePool& operator=(const JoltShapePool&) = delete;

		/**
		* [EN]
		* Creates or reuses a box shape with the specified size and center.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 指定したサイズと中心を持つボックス形状を生成または再利用する。
		*/
		Handle<JPH::Shape> CreateBoxShape(const Vector3& size, const Vector3& center);

		/**
		* [EN]
		* Creates or reuses a sphere shape with the specified radius.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 指定した半径を持つ球形状を生成または再利用する。
		*/
		Handle<JPH::Shape> CreateSphereShape(Float radius);

		/**
		* [EN]
		* Creates or reuses a capsule shape with the specified height and radius.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 指定した高さと半径を持つカプセル形状を生成または再利用する。
		*/
		Handle<JPH::Shape> CreateCapsuleShape(Float height, Float radius);

		/**
		* [EN]
		* Creates or reuses a cylinder shape with the specified height and radius.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 指定した高さと半径を持つ円柱形状を生成または再利用する。
		*/
		Handle<JPH::Shape> CreateCylinderShape(Float height, Float radius);

		/**
		* [EN]
		* Creates or reuses a rectangular shape in the XY plane.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* XY平面上の矩形形状を生成または再利用する。
		*/
		Handle<JPH::Shape> CreateRectShape(const Vector2& size, const Vector2& center);

		/**
		* [EN]
		* Creates or reuses a circular shape in the XY plane.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* XY平面上の円形状を生成または再利用する。
		*/
		Handle<JPH::Shape> CreateCircleShape(Float radius, const Vector2& center);

		/**
		* [EN]
		* Creates or reuses a triangle mesh shape for the specified asset.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 指定したアセットの三角形メッシュ形状を生成または再利用する。
		*/
		Handle<JPH::Shape> CreateMeshShape(Uint32 assetID, const DynamicArray<Vector3>& positions, const DynamicArray<Uint32>& indices);

		/**
		* [EN]
		* Creates or reuses a convex hull shape for the specified asset.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 指定したアセットの凸包形状を生成または再利用する。
		*/
		Handle<JPH::Shape> CreateConvexShape(Uint32 assetID, const DynamicArray<Vector3>& positions);

		/**
		* [EN]
		* Returns the shape referenced by a valid handle.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 有効なハンドルが参照する形状を返す。
		*/
		JPH::ShapeRefC Get(Handle<JPH::Shape> handle)const;

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
		void AddRef(Handle<JPH::Shape> handle);

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
		void Release(Handle<JPH::Shape> handle);

		/**
		* [EN]
		* Releases every shape and resets the pool.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* すべての形状を解放してプールを初期状態へ戻す。
		*/
		void Clear();

	private:
		/**
		* [EN]
		* Stores a shape and associates its cache key with a new handle.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 形状を格納し、キャッシュキーを新しいハンドルへ関連付ける。
		*/
		Handle<JPH::Shape> Register(JPH::ShapeRefC shape, Uint64 cacheKey);

	private:
		/// [EN] Half thickness assigned to flat two-dimensional shapes.
		/// [JP] 平面形状に与える厚みの半分。
		static constexpr Float flatShapeHalfThickness_ = 0.5f;

		/**
		* [EN]
		* Identifies the shape category included in a cache key.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* キャッシュキーに含める形状種別を識別する。
		*/
		enum class ShapeKind : Uint64
		{
			Box = 1,
			Sphere = 2,
			Capsule = 3,
			Cylinder = 4,
			Rect = 5,
			Circle = 6,
			Mesh = 7,
			Convex = 8,
		};

		/**
		* [EN]
		* Stores one pooled shape and its handle metadata.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* プール内の形状とハンドル用メタデータを保持する。
		*/
		struct Slot
		{
			/// [EN] Shape owned by this slot.
			/// [JP] このスロットが所有する形状。
			JPH::ShapeRefC shape_;

			/// [EN] Generation used to reject stale handles.
			/// [JP] 古いハンドルを無効化するための世代番号。
			Uint64 generation_ = 0;

			/// [EN] Key used to remove this shape from the cache.
			/// [JP] この形状をキャッシュから削除するためのキー。
			Uint64 cacheKey_ = 0;

			/// [EN] Number of owners currently sharing the shape.
			/// [JP] 現在この形状を共有している所有者数。
			Uint32 refCount_ = 0;
		};

		/// [EN] Storage for active and reusable shape slots.
		/// [JP] 使用中および再利用可能な形状スロットの格納領域。
		DynamicArray<Slot> slots_;

		/// [EN] Indices of slots available for reuse.
		/// [JP] 再利用可能なスロットのインデックス。
		DynamicArray<Uint64> freeIndices_;

		/// [EN] Maps shape parameters to their pooled handles.
		/// [JP] 形状パラメータをプール内ハンドルへ対応付けるキャッシュ。
		std::unordered_map<Uint64, Handle<JPH::Shape>> cache_;
	};
}
