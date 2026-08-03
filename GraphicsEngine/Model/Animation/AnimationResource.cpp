#include <GraphicsEngine/Model/Animation/AnimationResource.h>
#include <GraphicsEngine/Model/Animation/AnimationLoader.h>
#include <FoundationEngine/Resource/ResourceCache.h>
#include <FoundationEngine/Resource/LoaderSystem.h>

namespace SeedCore
{
	Handle<Animation> AnimationResource::Load(LoaderSystem& loader, ResourceCache& cache, Uint32 assetId)
	{
		if (assetHandleMap_.contains(assetId))
		{
			return assetHandleMap_.at(assetId);
		}

		Asset* asset = cache.GetAsset(assetId);
		if (!asset)
		{
			return Handle<Animation>::null();
		}

		Handle<Animation> handle = loader.animationLoader_->Load(loader, asset->fullpath_);
		if (handle.empty())
		{
			return Handle<Animation>::null();
		}

		assetHandleMap_.insert({ assetId, handle });
		return handle;
	}

	Handle<Animation> AnimationResource::GetHandle(Uint32 assetId)const
	{
		if (!assetHandleMap_.contains(assetId))
		{
			return Handle<Animation>::null();
		}
		return assetHandleMap_.at(assetId);
	}

	Animation* AnimationResource::Resolve(LoaderSystem& loader, const Handle<Animation>& handle)
	{
		return loader.animationLoader_->Get(handle);
	}

	Bool AnimationResource::Contains(Uint32 assetId)const
	{
		return assetHandleMap_.contains(assetId);
	}

	void AnimationResource::Unload(LoaderSystem& loader, Uint32 assetId)
	{
		if (!assetHandleMap_.contains(assetId))
		{
			return;
		}

		Handle<Animation> handle = assetHandleMap_.at(assetId);
		loader.animationLoader_->Clear(handle);
		assetHandleMap_.erase(assetId);
	}
}
