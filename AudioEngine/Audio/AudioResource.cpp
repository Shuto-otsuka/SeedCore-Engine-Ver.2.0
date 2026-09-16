#include <AudioEngine/Audio/AudioResource.h>
#include <AudioEngine/Audio/AudioLoader.h>
#include <FoundationEngine/Resource/ResourceCache.h>
#include <FoundationEngine/Resource/LoaderSystem.h>

namespace SeedCore
{
	void AudioResource::Load(const AssetContext& context, Uint32 assetId)
	{
		Load(context.loader_, context.cache_, assetId);
	}

	void AudioResource::Unload(const AssetContext& context, Uint32 assetId)
	{
		Unload(context.loader_, assetId);
	}

	Handle<Sound> AudioResource::Load(LoaderSystem& loader, ResourceCache& cache, Uint32 assetId)
	{
		if (assetHandleMap_.contains(assetId))
		{
			return assetHandleMap_.at(assetId);
		}

		AssetRecord* asset = cache.GetAsset(assetId);
		if (!asset)
		{
			return Handle<Sound>::null();
		}

		Handle<Sound> handle = loader.audioLoader_->Load(loader, asset->fullpath_);
		if (handle.empty())
		{
			return Handle<Sound>::null();
		}

		assetHandleMap_.insert({ assetId, handle });
		return handle;
	}

	Handle<Sound> AudioResource::GetHandle(Uint32 assetId)const
	{
		if (!assetHandleMap_.contains(assetId))
		{
			return Handle<Sound>::null();
		}

		return assetHandleMap_.at(assetId);
	}

	Sound* AudioResource::Resolve(LoaderSystem& loader, const Handle<Sound>& handle)
	{
		return loader.audioLoader_->Get(handle);
	}

	Bool AudioResource::Contains(Uint32 assetId)const
	{
		return assetHandleMap_.contains(assetId);
	}

	void AudioResource::Unload(LoaderSystem& loader, Uint32 assetId)
	{
		if (!assetHandleMap_.contains(assetId))
		{
			return;
		}

		Handle<Sound> handle = assetHandleMap_.at(assetId);
		loader.audioLoader_->Clear(handle);
		assetHandleMap_.erase(assetId);
	}

	REGISTER_ASSET(AssetType::Audio, AudioResource);
}
