#include <GraphicsEngine/Model/Skeleton/SkeletonResource.h>
#include <GraphicsEngine/Model/Skeleton/SkeletonLoader.h>
#include <FoundationEngine/Resource/ResourceCache.h>
#include <FoundationEngine/Resource/LoaderSystem.h>

namespace SeedCore
{
	void SkeletonResource::Load(const AssetContext& context, Uint32 assetId)
	{
		Load(context.loader_, context.cache_, assetId);
	}

	void SkeletonResource::Unload(const AssetContext& context, Uint32 assetId)
	{
		Unload(context.loader_, assetId);
	}

	Handle<SkeletonRig> SkeletonResource::Load(LoaderSystem& loader, ResourceCache& cache, Uint32 assetId)
	{
		if (assetHandleMap_.contains(assetId))
		{
			return assetHandleMap_.at(assetId);
		}

		AssetRecord* asset = cache.GetAsset(assetId);
		if (!asset)
		{
			return Handle<SkeletonRig>::null();
		}

		Handle<SkeletonRig> handle = loader.skeletonLoader_->Load(loader, asset->fullpath_);
		if (handle.empty())
		{
			return Handle<SkeletonRig>::null();
		}

		assetHandleMap_.insert({ assetId, handle });
		return handle;
	}

	Handle<SkeletonRig> SkeletonResource::GetHandle(Uint32 assetId)const
	{
		if (!assetHandleMap_.contains(assetId))
		{
			return Handle<SkeletonRig>::null();
		}
		return assetHandleMap_.at(assetId);
	}

	SkeletonRig* SkeletonResource::Resolve(LoaderSystem& loader, const Handle<SkeletonRig>& handle)
	{
		return loader.skeletonLoader_->Get(handle);
	}

	Bool SkeletonResource::Contains(Uint32 assetId)const
	{
		return assetHandleMap_.contains(assetId);
	}

	void SkeletonResource::Unload(LoaderSystem& loader, Uint32 assetId)
	{
		if (!assetHandleMap_.contains(assetId))
		{
			return;
		}

		Handle<SkeletonRig> handle = assetHandleMap_.at(assetId);
		loader.skeletonLoader_->Clear(handle);
		assetHandleMap_.erase(assetId);
	}

	REGISTER_ASSET(AssetType::Skeleton, SkeletonResource);
}
