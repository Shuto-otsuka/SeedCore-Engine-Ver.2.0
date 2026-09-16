#include <GraphicsEngine/Texture/TextureResource.h>
#include <GraphicsEngine/Texture/Texture.h>
#include <GraphicsEngine/D3D12/Descriptor/BindlessHeap.h>
#include <FoundationEngine/Resource/ResourceCache.h>
#include <FoundationEngine/Resource/LoaderSystem.h>

namespace SeedCore
{
	void TextureResource::Load(const AssetContext& context, Uint32 assetId)
	{
		Load(context.loader_, context.device_, context.cmdQueue_, context.heap_, context.cache_, assetId);
	}

	void TextureResource::Unload(const AssetContext& context, Uint32 assetId)
	{
		Unload(context.loader_, assetId, context.heap_);
	}

	Handle<Texture> TextureResource::Load(LoaderSystem& loader, ID3D12Device* device, D3D12CommandQueue* cmdQueue, BindlessHeap* heap, ResourceCache& cache, Uint32 assetId)
	{
		if (assetHandleMap_.contains(assetId))
		{
			return assetHandleMap_.at(assetId);
		}

		AssetRecord* asset = cache.GetAsset(assetId);
		if (!asset)
		{
			return Handle<Texture>::null();
		}

		Handle<Texture> handle = loader.textureLoader_->Load(device, cmdQueue, heap, asset->fullpath_);
		if (handle.empty())
		{
			return Handle<Texture>::null();
		}

		assetHandleMap_.insert({ assetId, handle });
		return handle;
	}

	Handle<Texture> TextureResource::GetHandle(Uint32 assetId)const
	{
		if (!assetHandleMap_.contains(assetId))
		{
			return Handle<Texture>::null();
		}
		return assetHandleMap_.at(assetId);
	}

	Uint32 TextureResource::GetID(LoaderSystem& loader, Uint32 assetId)const
	{
		Handle<Texture> handle = GetHandle(assetId);
		if (handle.empty())
		{
			return UINT32_MAX;
		}

		Texture* texture = loader.textureLoader_->Get(handle);
		if (!texture)
		{
			return UINT32_MAX;
		}

		return texture->ShaderResourceViewIndex();
	}

	Texture* TextureResource::Resolve(LoaderSystem& loader, BindlessHeap* heap, const Handle<Texture>& handle, Uint64 frame)
	{
		return loader.textureLoader_->Resolve(heap, handle, frame);
	}

	Bool TextureResource::Contains(Uint32 assetId)const
	{
		return assetHandleMap_.contains(assetId);
	}

	void TextureResource::Unload(LoaderSystem& loader, Uint32 assetId, BindlessHeap* heap)
	{
		if (!assetHandleMap_.contains(assetId))
		{
			return;
		}

		Handle<Texture> handle = assetHandleMap_.at(assetId);
		loader.textureLoader_->Clear(handle, heap);
		assetHandleMap_.erase(assetId);
	}

	void TextureResource::EvictBudget(LoaderSystem& loader, BindlessHeap* heap, Uint64 currentFrame)
	{
		loader.textureLoader_->EvictBudget(heap, currentFrame);
	}

	REGISTER_ASSET(AssetType::Texture, TextureResource);
}
