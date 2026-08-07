#include <GraphicsEngine/Model/Collision/MeshCollisionResource.h>
#include <GraphicsEngine/Model/Collision/MeshCollisionLoader.h>
#include <FoundationEngine/Resource/ResourceCache.h>
#include <FoundationEngine/Resource/LoaderSystem.h>

namespace SeedCore
{
	Handle<MeshCollision> MeshCollisionResource::Load(LoaderSystem& loader, ResourceCache& cache, Uint32 assetId)
	{
		if (assetHandleMap_.contains(assetId))
		{
			return assetHandleMap_.at(assetId);
		}

		Asset* asset = cache.GetAsset(assetId);
		if (!asset)
		{
			return Handle<MeshCollision>::null();
		}

		Handle<MeshCollision> handle = loader.meshCollisionLoader_->Load(loader, asset->fullpath_);
		if (handle.empty())
		{
			return Handle<MeshCollision>::null();
		}

		assetHandleMap_.insert({ assetId, handle });
		return handle;
	}

	Handle<MeshCollision> MeshCollisionResource::GetHandle(Uint32 assetId)const
	{
		if (!assetHandleMap_.contains(assetId))
		{
			return Handle<MeshCollision>::null();
		}
		return assetHandleMap_.at(assetId);
	}

	MeshCollision* MeshCollisionResource::Resolve(LoaderSystem& loader, const Handle<MeshCollision>& handle)
	{
		return loader.meshCollisionLoader_->Get(handle);
	}

	Bool MeshCollisionResource::Contains(Uint32 assetId)const
	{
		return assetHandleMap_.contains(assetId);
	}

	void MeshCollisionResource::Unload(LoaderSystem& loader, Uint32 assetId)
	{
		if (!assetHandleMap_.contains(assetId))
		{
			return;
		}

		Handle<MeshCollision> handle = assetHandleMap_.at(assetId);
		loader.meshCollisionLoader_->Clear(handle);
		assetHandleMap_.erase(assetId);
	}
}
