#include <GraphicsEngine/Effect/Effekseer/EffekseerResource.h>
#include <GraphicsEngine/Effect/Effekseer/EffekseerLoader.h>
#include <FoundationEngine/Resource/ResourceCache.h>
#include <FoundationEngine/Resource/LoaderSystem.h>

namespace SeedCore
{
	Handle<EffekseerEffectHandle> EffekseerResource::Load(LoaderSystem& loader, ResourceCache& cache, Uint32 assetId)
	{
		if (assetHandleMap_.contains(assetId))
		{
			return assetHandleMap_.at(assetId);
		}

		Asset* asset = cache.GetAsset(assetId);
		if (!asset)
		{
			return Handle<EffekseerEffectHandle>::null();
		}

		Handle<EffekseerEffectHandle> handle = loader.effekseerLoader_->Load(asset->fullpath_);
		if (handle.empty())
		{
			return Handle<EffekseerEffectHandle>::null();
		}

		assetHandleMap_.insert({ assetId, handle });
		return handle;
	}

	Handle<EffekseerEffectHandle> EffekseerResource::GetHandle(Uint32 assetId)const
	{
		if (!assetHandleMap_.contains(assetId))
		{
			return Handle<EffekseerEffectHandle>::null();
		}
		return assetHandleMap_.at(assetId);
	}

	Effekseer::EffectRef* EffekseerResource::Resolve(LoaderSystem& loader, const Handle<EffekseerEffectHandle>& handle)
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

		Handle<EffekseerEffectHandle> handle = assetHandleMap_.at(assetId);
		loader.effekseerLoader_->Clear(handle);
		assetHandleMap_.erase(assetId);
	}
}
