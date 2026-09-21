#include <PhysicsEngine/JoltPhysics/JoltShapePool.h>
#include <FoundationEngine/Math/Random/Hash.h>
#include <FoundationEngine/Log/Error.h>

namespace SeedCore
{
	/**
	* [EN]
	* Creates or reuses a box shape with the specified size and center.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 指定したサイズと中心を持つボックス形状を生成または再利用する。
	*/
	Handle<JPH::Shape> JoltShapePool::CreateBoxShape(const Vector3& size, const Vector3& center)
	{
		Uint64 key = HashCombine(0, static_cast<Uint64>(ShapeKind::Box));
		key = HashVector3(key, size);
		key = HashVector3(key, center);

		if (auto it = cache_.find(key); it != cache_.end())
		{
			slots_[it->second.index_].refCount_++;
			return it->second;
		}

		const JPH::Vec3 halfExtent{ Max(size.x * 0.5f, JPH::cDefaultConvexRadius),Max(size.y * 0.5f, JPH::cDefaultConvexRadius),Max(size.z * 0.5f, JPH::cDefaultConvexRadius) };

		JPH::ShapeRefC shape = new JPH::BoxShape(halfExtent);

		if (center != Vector3{ 0.0f, 0.0f, 0.0f })
		{
			shape = new JPH::RotatedTranslatedShape(JPH::Vec3{ center.x, center.y, center.z }, JPH::Quat::sIdentity(), shape);
		}

		return Register(std::move(shape), key);
	}

	/**
	* [EN]
	* Creates or reuses a sphere shape with the specified radius.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 指定した半径を持つ球形状を生成または再利用する。
	*/
	Handle<JPH::Shape> JoltShapePool::CreateSphereShape(Float radius)
	{
		Uint64 key = HashCombine(0, static_cast<Uint64>(ShapeKind::Sphere));
		key = HashFloat(key, radius);

		if (auto it = cache_.find(key); it != cache_.end())
		{
			slots_[it->second.index_].refCount_++;
			return it->second;
		}

		JPH::ShapeRefC shape = new JPH::SphereShape(radius);

		return Register(std::move(shape), key);
	}

	/**
	* [EN]
	* Creates or reuses a capsule shape with the specified height and radius.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 指定した高さと半径を持つカプセル形状を生成または再利用する。
	*/
	Handle<JPH::Shape> JoltShapePool::CreateCapsuleShape(Float height, Float radius)
	{
		Uint64 key = HashCombine(0, static_cast<Uint64>(ShapeKind::Capsule));
		key = HashFloat(key, height);
		key = HashFloat(key, radius);

		if (auto it = cache_.find(key); it != cache_.end())
		{
			slots_[it->second.index_].refCount_++;
			return it->second;
		}

		JPH::ShapeRefC shape = new JPH::CapsuleShape(height * 0.5f, radius);

		return Register(std::move(shape), key);
	}

	/**
	* [EN]
	* Creates or reuses a cylinder shape with the specified height and radius.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 指定した高さと半径を持つ円柱形状を生成または再利用する。
	*/
	Handle<JPH::Shape> JoltShapePool::CreateCylinderShape(Float height, Float radius)
	{
		Uint64 key = HashCombine(0, static_cast<Uint64>(ShapeKind::Cylinder));
		key = HashFloat(key, height);
		key = HashFloat(key, radius);

		if (auto it = cache_.find(key); it != cache_.end())
		{
			slots_[it->second.index_].refCount_++;
			return it->second;
		}

		JPH::ShapeRefC shape = new JPH::CylinderShape(height * 0.5f, radius);

		return Register(std::move(shape), key);
	}

	/**
	* [EN]
	* Creates or reuses a rectangular shape in the XY plane.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* XY平面上の矩形形状を生成または再利用する。
	*/
	Handle<JPH::Shape> JoltShapePool::CreateRectShape(const Vector2& size, const Vector2& center)
	{
		Uint64 key = HashCombine(0, static_cast<Uint64>(ShapeKind::Rect));
		key = HashVector2(key, size);
		key = HashVector2(key, center);

		if (auto it = cache_.find(key); it != cache_.end())
		{
			slots_[it->second.index_].refCount_++;
			return it->second;
		}

		const JPH::Vec3 halfExtent{ Max(size.x * 0.5f, JPH::cDefaultConvexRadius),Max(size.y * 0.5f, JPH::cDefaultConvexRadius),flatShapeHalfThickness_ };

		JPH::ShapeRefC shape = new JPH::BoxShape(halfExtent);

		if (center.x != 0.0f || center.y != 0.0f)
		{
			shape = new JPH::RotatedTranslatedShape(JPH::Vec3{ center.x, center.y, 0.0f }, JPH::Quat::sIdentity(), shape);
		}

		return Register(std::move(shape), key);
	}

	/**
	* [EN]
	* Creates or reuses a circular shape in the XY plane.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* XY平面上の円形状を生成または再利用する。
	*/
	Handle<JPH::Shape> JoltShapePool::CreateCircleShape(Float radius, const Vector2& center)
	{
		Uint64 key = HashCombine(0, static_cast<Uint64>(ShapeKind::Circle));
		key = HashFloat(key, radius);
		key = HashVector2(key, center);

		if (auto it = cache_.find(key); it != cache_.end())
		{
			slots_[it->second.index_].refCount_++;
			return it->second;
		}

		JPH::ShapeRefC shape = new JPH::SphereShape(radius);

		if (center.x != 0.0f || center.y != 0.0f)
		{
			shape = new JPH::RotatedTranslatedShape(JPH::Vec3{ center.x, center.y, 0.0f }, JPH::Quat::sIdentity(), shape);
		}

		return Register(std::move(shape), key);
	}

	/**
	* [EN]
	* Creates or reuses a triangle mesh shape for the specified asset.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 指定したアセットの三角形メッシュ形状を生成または再利用する。
	*/
	Handle<JPH::Shape> JoltShapePool::CreateMeshShape(Uint32 assetID, const DynamicArray<Vector3>& positions, const DynamicArray<Uint32>& indices)
	{
		Uint64 key = HashCombine(0, static_cast<Uint64>(ShapeKind::Mesh));
		key = HashCombine(key, static_cast<Uint64>(assetID));

		if (auto it = cache_.find(key); it != cache_.end())
		{
			slots_[it->second.index_].refCount_++;
			return it->second;
		}

		if (positions.empty() || indices.size() < 3)
		{
			return Handle<JPH::Shape>::null();
		}

		JPH::VertexList vertices;
		vertices.reserve(positions.size());
		std::ranges::transform(positions, std::back_inserter(vertices), [](const Vector3& position) { return JPH::Float3(position.x, position.y, position.z); });

		JPH::IndexedTriangleList triangles;
		triangles.reserve(indices.size() / 3);
		for (Size cornerIndex = 0; cornerIndex + 2 < indices.size(); cornerIndex += 3)
		{
			triangles.push_back(JPH::IndexedTriangle(indices[cornerIndex], indices[cornerIndex + 1], indices[cornerIndex + 2]));
		}

		JPH::MeshShapeSettings settings(std::move(vertices), std::move(triangles));
		settings.SetEmbedded();

		JPH::ShapeSettings::ShapeResult result = settings.Create();
		if (!result.IsValid())
		{
			SC_LOG_ERROR("メッシュ形状の生成に失敗しました: %s", result.GetError().c_str());
			return Handle<JPH::Shape>::null();
		}

		JPH::ShapeRefC shape = result.Get();

		return Register(std::move(shape), key);
	}

	/**
	* [EN]
	* Creates or reuses a convex hull shape for the specified asset.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 指定したアセットの凸包形状を生成または再利用する。
	*/
	Handle<JPH::Shape> JoltShapePool::CreateConvexShape(Uint32 assetID, const DynamicArray<Vector3>& positions)
	{
		Uint64 key = HashCombine(0, static_cast<Uint64>(ShapeKind::Convex));
		key = HashCombine(key, static_cast<Uint64>(assetID));

		if (auto it = cache_.find(key); it != cache_.end())
		{
			slots_[it->second.index_].refCount_++;
			return it->second;
		}

		if (positions.size() < 4)
		{
			return Handle<JPH::Shape>::null();
		}

		JPH::Array<JPH::Vec3> points;
		points.reserve(positions.size());
		std::ranges::transform(positions, std::back_inserter(points), [](const Vector3& position) { return JPH::Vec3(position.x, position.y, position.z); });

		JPH::ConvexHullShapeSettings settings(points, JPH::cDefaultConvexRadius);
		settings.SetEmbedded();

		JPH::ShapeSettings::ShapeResult result = settings.Create();
		if (!result.IsValid())
		{
			SC_LOG_ERROR("凸包形状の生成に失敗しました: %s", result.GetError().c_str());
			return Handle<JPH::Shape>::null();
		}

		JPH::ShapeRefC shape = result.Get();

		return Register(std::move(shape), key);
	}

	/**
	* [EN]
	* Returns the shape referenced by a valid handle.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 有効なハンドルが参照する形状を返す。
	*/
	JPH::ShapeRefC JoltShapePool::Get(Handle<JPH::Shape> handle)const
	{
		if (handle.empty() || handle.index_ >= slots_.size())
		{
			return nullptr;
		}

		const Slot& slot = slots_[handle.index_];
		if (slot.generation_ != handle.generation_)
		{
			return nullptr;
		}

		return slot.shape_;
	}

	/**
	* [EN]
	* Increments the reference count of a valid shape handle.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 有効な形状ハンドルの参照カウントを増やす。
	*/
	void JoltShapePool::AddRef(Handle<JPH::Shape> handle)
	{
		if (handle.empty() || handle.index_ >= slots_.size())
		{
			return;
		}

		Slot& slot = slots_[handle.index_];
		if (slot.generation_ != handle.generation_)
		{
			return;
		}

		slot.refCount_++;
	}

	/**
	* [EN]
	* Releases one reference and recycles the slot when no owners remain.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 参照を一つ解放し、所有者がいなくなったスロットを再利用可能にする。
	*/
	void JoltShapePool::Release(Handle<JPH::Shape> handle)
	{
		if (handle.empty() || handle.index_ >= slots_.size())
		{
			return;
		}

		Slot& slot = slots_[handle.index_];
		if (slot.generation_ != handle.generation_ || slot.refCount_ == 0)
		{
			return;
		}

		if (--slot.refCount_ > 0)
		{
			return;
		}

		cache_.erase(slot.cacheKey_);
		slot.shape_ = nullptr;
		++slot.generation_;
		freeIndices_.push_back(handle.index_);
	}

	/**
	* [EN]
	* Releases every shape and resets the pool.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* すべての形状を解放してプールを初期状態へ戻す。
	*/
	void JoltShapePool::Clear()
	{
		for (Slot& slot : slots_)
		{
			slot.shape_ = nullptr;
			++slot.generation_;
		}
		slots_.clear();
		freeIndices_.clear();
		cache_.clear();
	}

	/**
	* [EN]
	* Stores a shape and associates its cache key with a new handle.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 形状を格納し、キャッシュキーを新しいハンドルへ関連付ける。
	*/
	Handle<JPH::Shape> JoltShapePool::Register(JPH::ShapeRefC shape, Uint64 cacheKey)
	{
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
		slot.shape_ = std::move(shape);
		slot.cacheKey_ = cacheKey;
		slot.refCount_ = 1;

		Handle<JPH::Shape> handle{};
		handle.index_ = index;
		handle.generation_ = slot.generation_;
		cache_.emplace(cacheKey, handle);
		return handle;
	};
}
