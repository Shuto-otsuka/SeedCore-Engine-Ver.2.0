#include <PhysicsEngine/JoltPhysics/JoltShapePool.h>
#include <FoundationEngine/Math/Random/Hash.h>

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

		const JPH::Vec3 halfExtent{
			std::max(size.x * 0.5f, JPH::cDefaultConvexRadius),
			std::max(size.y * 0.5f, JPH::cDefaultConvexRadius),
			std::max(size.z * 0.5f, JPH::cDefaultConvexRadius) };

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

		const JPH::Vec3 halfExtent{
			std::max(size.x * 0.5f, JPH::cDefaultConvexRadius),
			std::max(size.y * 0.5f, JPH::cDefaultConvexRadius),
			flatShapeHalfThickness_ };

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
