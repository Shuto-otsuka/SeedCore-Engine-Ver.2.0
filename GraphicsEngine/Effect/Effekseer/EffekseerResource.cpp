#include <GraphicsEngine/Effect/Effekseer/EffekseerResource.h>
#include <GraphicsEngine/Effect/Effekseer/EffekseerLoader.h>
#include <FoundationEngine/Resource/ResourceCache.h>
#include <FoundationEngine/Resource/LoaderSystem.h>

namespace SeedCore
{
	void EffekseerResource::Load(const AssetContext& context, Uint32 assetId)
	{
		Load(context.loader_, context.cache_, assetId);
	}

	void EffekseerResource::Unload(const AssetContext& context, Uint32 assetId)
	{
		Unload(context.loader_, assetId);
	}

	Handle<EffekseerEffect> EffekseerResource::Load(LoaderSystem& loader, ResourceCache& cache, Uint32 assetId)
	{
		if (assetHandleMap_.contains(assetId))
		{
			return assetHandleMap_.at(assetId);
		}

		AssetRecord* asset = cache.GetAsset(assetId);
		if (!asset)
		{
			return Handle<EffekseerEffect>::null();
		}

		Handle<EffekseerEffect> handle = loader.effekseerLoader_->Load(asset->fullpath_);
		if (handle.empty())
		{
			return Handle<EffekseerEffect>::null();
		}

		assetHandleMap_.insert({ assetId, handle });
		return handle;
	}

	Handle<EffekseerEffect> EffekseerResource::GetHandle(Uint32 assetId)const
	{
		if (!assetHandleMap_.contains(assetId))
		{
			return Handle<EffekseerEffect>::null();
		}
		return assetHandleMap_.at(assetId);
	}

	EffekseerEffect* EffekseerResource::Resolve(LoaderSystem& loader, const Handle<EffekseerEffect>& handle)
	{
		return loader.effekseerLoader_->Get(handle);
	}

	Bool EffekseerResource::Contains(Uint32 assetId)const
	{
		return assetHandleMap_.contains(assetId);
	}

	void EffekseerResource::Unload(LoaderSystem& loader, Uint32 assetId)
	{
		if (!assetHandleMap_.contains(assetId))
		{
			return;
		}

		Handle<EffekseerEffect> handle = assetHandleMap_.at(assetId);
		loader.effekseerLoader_->Clear(handle);
		assetHandleMap_.erase(assetId);
	}

	REGISTER_ASSET(AssetType::Effect, EffekseerResource);
}
