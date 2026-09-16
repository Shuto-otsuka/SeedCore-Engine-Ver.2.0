#include <GraphicsEngine/Movie/MovieResource.h>
#include <GraphicsEngine/Movie/MovieLoader.h>
#include <FoundationEngine/Resource/ResourceCache.h>
#include <FoundationEngine/Resource/LoaderSystem.h>

namespace SeedCore
{
	void MovieResource::Load(const AssetContext& context, Uint32 assetId)
	{
		Load(context.loader_, context.cache_, assetId);
	}

	void MovieResource::Unload(const AssetContext& context, Uint32 assetId)
	{
		Unload(context.loader_, assetId);
	}

	void MovieResource::Clear(const AssetContext& context)
	{
		Clear(context.loader_);
	}

	Handle<Video> MovieResource::Load(LoaderSystem& loader, ResourceCache& cache, Uint32 assetId)
	{
		if (assetHandleMap_.contains(assetId))
		{
			return assetHandleMap_.at(assetId);
		}

		AssetRecord* asset = cache.GetAsset(assetId);
		if (!asset)
		{
			return Handle<Video>::null();
		}

		Handle<Video> handle = loader.movieLoader_->Load(asset->fullpath_);
		if (handle.empty())
		{
			return Handle<Video>::null();
		}

		assetHandleMap_.insert({ assetId, handle });
		return handle;
	}

	Handle<Video> MovieResource::GetHandle(Uint32 assetId)const
	{
		if (!assetHandleMap_.contains(assetId))
		{
			return Handle<Video>::null();
		}

		return assetHandleMap_.at(assetId);
	}

	Video* MovieResource::Resolve(LoaderSystem& loader, const Handle<Video>& handle)
	{
		return loader.movieLoader_->Get(handle);
	}

	Bool MovieResource::Contains(Uint32 assetId)const
	{
		return assetHandleMap_.contains(assetId);
	}

	void MovieResource::Unload(LoaderSystem& loader, Uint32 assetId)
	{
		if (!assetHandleMap_.contains(assetId))
		{
			return;
		}

		Handle<Video> handle = assetHandleMap_.at(assetId);
		loader.movieLoader_->Clear(handle);
		assetHandleMap_.erase(assetId);
	}

	void MovieResource::Update(LoaderSystem& loader, ID3D12Device* device, ID3D12GraphicsCommandList6* cmdList, BindlessHeap* bindlessHeap)
	{
		for (const Handle<Video>& handle : assetHandleMap_ | std::ranges::views::values)
		{
			Video* video = loader.movieLoader_->Get(handle);
			if (!video)
			{
				continue;
			}

			video->UploadFrame(device, cmdList, bindlessHeap);
		}
	}

	void MovieResource::Clear(LoaderSystem& loader)
	{
		for (Handle<Video>& handle : assetHandleMap_ | std::ranges::views::values)
		{
			loader.movieLoader_->Clear(handle);
		}

		assetHandleMap_.clear();
	}

	REGISTER_ASSET(AssetType::Movie, MovieResource);
}
