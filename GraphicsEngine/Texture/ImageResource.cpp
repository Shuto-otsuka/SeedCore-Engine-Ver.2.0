#include <GraphicsEngine/Texture/ImageResource.h>
#include <GraphicsEngine/Texture/Texture.h>
#include <GraphicsEngine/D3D12/Descriptor/BindlessHeap.h>
#include <FoundationEngine/Resource/ResourceCache.h>
#include <FoundationEngine/Resource/LoaderSystem.h>

namespace SeedCore
{
	Handle<Texture> ImageResource::Load(LoaderSystem& loader, ID3D12Device* device, ID3D12CommandQueue* cmdQueue, BindlessHeap* heap, ResourceCache& cache, Uint32 assetId)
	{
		if (assetHandleMap_.contains(assetId))
		{
			return assetHandleMap_.at(assetId);
		}

		Asset* asset = cache.GetAsset(assetId);
		if (!asset)
		{
			return Handle<Texture>::null();
		}

		Handle<Texture> handle = loader.imageLoader_->Load(*loader.textureLoader_, device, cmdQueue, heap, asset->fullpath_);
		if (handle.empty())
		{
			return Handle<Texture>::null();
		}

		assetHandleMap_.insert({ assetId, handle });
		return handle;
	}

	Handle<Texture> ImageResource::GetHandle(Uint32 assetId)const
	{
		if (!assetHandleMap_.contains(assetId))
		{
			return Handle<Texture>::null();
		}
		return assetHandleMap_.at(assetId);
	}

	Uint32 ImageResource::GetID(LoaderSystem& loader, Uint32 assetId)const
	{
		Handle<Texture> handle = GetHandle(assetId);
		if (handle.empty())
		{
			return UINT32_MAX;
		}

		Texture* texture = loader.imageLoader_->Get(handle);
		if (!texture)
		{
			return UINT32_MAX;
		}

		return texture->textureIndex_;
	}

	Texture* ImageResource::Resolve(LoaderSystem& loader, BindlessHeap* heap, const Handle<Texture>& handle, Uint64 frame)
	{
		return loader.imageLoader_->Resolve(*loader.textureLoader_, heap, handle, frame);
	}

	Bool ImageResource::Contains(Uint32 assetId)const
	{
		return assetHandleMap_.contains(assetId);
	}

	void ImageResource::Unload(LoaderSystem& loader, Uint32 assetId, BindlessHeap* heap)
	{
		if (!assetHandleMap_.contains(assetId))
		{
			return;
		}

		Handle<Texture> handle = assetHandleMap_.at(assetId);
		loader.imageLoader_->Clear(handle, heap);
		assetHandleMap_.erase(assetId);
	}

	void ImageResource::EvictBudget(LoaderSystem& loader, BindlessHeap* heap, Uint64 currentFrame)
	{
		loader.imageLoader_->EvictBudget(heap, currentFrame);
	}
}
