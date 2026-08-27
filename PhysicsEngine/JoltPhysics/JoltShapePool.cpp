#include <PhysicsEngine/JoltPhysics/JoltShapePool.h>
#include <FoundationEngine/Math/Random/Hash.h>
#include <FoundationEngine/Log/Error.h>

namespace SeedCore
{
	ShapeHandle JoltShapePool::CreateBoxShape(const Vector3& size, const Vector3& center)
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

	ShapeHandle JoltShapePool::CreateSphereShape(Float radius)
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

	ShapeHandle JoltShapePool::CreateCapsuleShape(Float height, Float radius)
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

	ShapeHandle JoltShapePool::CreateCylinderShape(Float height, Float radius)
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

	ShapeHandle JoltShapePool::CreateRectShape(const Vector2& size, const Vector2& center)
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

	ShapeHandle JoltShapePool::CreateCircleShape(Float radius, const Vector2& center)
	{
		Uint64 key = HashCombine(0, static_cast<Uint64>(ShapeKind::Circle));
		key = HashFloat(key, radius);
		key = HashVector2(key, center);

		if (auto it = cache_.find(key); it != cache_.end())
		{
			slots_[it->second.index_].refCount_++;
			return it->second;
		}

		JPH::ShapeRefC shape = new JPH::CylinderShape(flatShapeHalfThickness_, radius);

		const JPH::Quat faceForwardRotation = JPH::Quat::sRotation(JPH::Vec3::sAxisX(), JPH::JPH_PI * 0.5f);
		shape = new JPH::RotatedTranslatedShape(JPH::Vec3{ center.x, center.y, 0.0f }, faceForwardRotation, shape);

		return Register(std::move(shape), key);
	}

	ShapeHandle JoltShapePool::CreateMeshShape(Uint32 assetID, const DynamicArray<Vector3>& positions, const DynamicArray<Uint32>& indices)
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
			return ShapeHandle::null();
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
			return ShapeHandle::null();
		}

		JPH::ShapeRefC shape = result.Get();

		return Register(std::move(shape), key);
	}

	ShapeHandle JoltShapePool::CreateConvexShape(Uint32 assetID, const DynamicArray<Vector3>& positions)
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
			return ShapeHandle::null();
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
			return ShapeHandle::null();
		}

		JPH::ShapeRefC shape = result.Get();

		return Register(std::move(shape), key);
	}

	JPH::ShapeRefC JoltShapePool::Get(ShapeHandle handle)const
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

	void JoltShapePool::AddRef(ShapeHandle handle)
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

	void JoltShapePool::Release(ShapeHandle handle)
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

	ShapeHandle JoltShapePool::Register(JPH::ShapeRefC shape, Uint64 cacheKey)
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

		ShapeHandle handle{};
		handle.index_ = index;
		handle.generation_ = slot.generation_;
		cache_.emplace(cacheKey, handle);
		return handle;
	};
}
