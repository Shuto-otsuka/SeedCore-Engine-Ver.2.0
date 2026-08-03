#include <GraphicsEngine/Sky/SkymapResource.h>
#include <GraphicsEngine/Sky/SkymapLoader.h>
#include <GraphicsEngine/D3D12/Descriptor/BindlessHeap.h>
#include <FoundationEngine/Resource/ResourceCache.h>
#include <FoundationEngine/Resource/LoaderSystem.h>

namespace SeedCore
{
	Handle<Skymap> SkymapResource::Load(LoaderSystem& loader, ID3D12Device* device, ID3D12CommandQueue* cmdQueue, BindlessHeap* heap, ResourceCache& cache, Uint32 assetId)
	{
		if (assetHandleMap_.contains(assetId))
		{
			return assetHandleMap_.at(assetId);
		}

		Asset* asset = cache.GetAsset(assetId);
		if (!asset)
		{
			return Handle<Skymap>::null();
		}

		Handle<Skymap> handle = loader.skymapLoader_->Load(device, cmdQueue, heap, asset->fullpath_);
		if (handle.empty())
		{
			return Handle<Skymap>::null();
		}

		assetHandleMap_.insert({ assetId, handle });
		return handle;
	}

	Handle<Skymap> SkymapResource::GetHandle(Uint32 assetId)const
	{
		if (!assetHandleMap_.contains(assetId))
		{
			return Handle<Skymap>::null();
		}
		return assetHandleMap_.at(assetId);
	}

	Skymap* SkymapResource::Resolve(LoaderSystem& loader, const Handle<Skymap>& handle)
	{
		return loader.skymapLoader_->Get(handle);
	}

	Bool SkymapResource::Contains(Uint32 assetId)const
	{
		return assetHandleMap_.contains(assetId);
	}

	void SkymapResource::Unload(LoaderSystem& loader, Uint32 assetId, BindlessHeap* heap)
	{
		if (!assetHandleMap_.contains(assetId))
		{
			return;
		}

		Handle<Skymap> handle = assetHandleMap_.at(assetId);
		loader.skymapLoader_->Clear(handle, heap);
		assetHandleMap_.erase(assetId);
	}
}
