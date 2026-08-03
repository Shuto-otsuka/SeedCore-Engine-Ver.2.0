#include <GraphicsEngine/Model/ModelResource.h>
#include <GraphicsEngine/Model/ModelLoader.h>
#include <GraphicsEngine/D3D12/Descriptor/BindlessHeap.h>
#include <FoundationEngine/Resource/ResourceCache.h>
#include <FoundationEngine/Resource/LoaderSystem.h>

namespace SeedCore
{
	Handle<Crister> ModelResource::Load(LoaderSystem& loader, ID3D12Device* device, ID3D12CommandQueue* cmdQueue, BindlessHeap* heap, BC7CompressShader& bc7Shader, ResourceCache& cache, Uint32 assetId)
	{
		if (assetHandleMap_.contains(assetId))
		{
			return assetHandleMap_.at(assetId);
		}

		Asset* asset = cache.GetAsset(assetId);
		if (!asset)
		{
			return Handle<Crister>::null();
		}

		AxisConvention axisConvention = cache.ReadAxisConvention(assetId);
		Handle<Crister> handle = loader.modelLoader_->Load(loader, device, cmdQueue, heap, bc7Shader, asset->fullpath_, axisConvention);
		if (handle.empty())
		{
			return Handle<Crister>::null();
		}

		assetHandleMap_.insert({ assetId, handle });
		return handle;
	}

	Handle<Crister> ModelResource::GetHandle(Uint32 assetId)const
	{
		if (!assetHandleMap_.contains(assetId))
		{
			return Handle<Crister>::null();
		}
		return assetHandleMap_.at(assetId);
	}

	Crister* ModelResource::Resolve(LoaderSystem& loader, const Handle<Crister>& handle)
	{
		return loader.modelLoader_->Get(handle);
	}

	Bool ModelResource::Contains(Uint32 assetId)const
	{
		return assetHandleMap_.contains(assetId);
	}

	void ModelResource::Unload(LoaderSystem& loader, Uint32 assetId, BindlessHeap* heap)
	{
		if (!assetHandleMap_.contains(assetId))
		{
			return;
		}

		Handle<Crister> handle = assetHandleMap_.at(assetId);
		loader.modelLoader_->Clear(handle, heap);
		assetHandleMap_.erase(assetId);
	}
}
