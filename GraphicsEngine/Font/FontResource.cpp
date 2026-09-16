#include <GraphicsEngine/Font/FontResource.h>
#include <GraphicsEngine/Font/FontLoader.h>
#include <FoundationEngine/Resource/ResourceCache.h>
#include <FoundationEngine/Resource/LoaderSystem.h>

namespace SeedCore
{
	void FontResource::Load(const AssetContext& context, Uint32 assetId)
	{
		Load(context.loader_, context.cache_, assetId, 48.0f);
	}

	void FontResource::Unload(const AssetContext& context, Uint32 assetId)
	{
		Unload(context.loader_, assetId);
	}

	void FontResource::Clear(const AssetContext& context)
	{
		Clear(context.loader_);
	}

	Handle<Font> FontResource::Load(LoaderSystem& loader, ResourceCache& cache, Uint32 assetId, Float fontSize)
	{
		if (assetHandleMap_.contains(assetId))
		{
			return assetHandleMap_.at(assetId);
		}

		AssetRecord* asset = cache.GetAsset(assetId);
		if (!asset)
		{
			return Handle<Font>::null();
		}

		Handle<Font> handle = loader.fontLoader_->Load(asset->fullpath_, fontSize);
		if (handle.empty())
		{
			return Handle<Font>::null();
		}

		assetHandleMap_.insert({ assetId, handle });
		return handle;
	}

	Handle<Font> FontResource::GetHandle(Uint32 assetId)const
	{
		if (!assetHandleMap_.contains(assetId))
		{
			return Handle<Font>::null();
		}

		return assetHandleMap_.at(assetId);
	}

	Font* FontResource::Resolve(LoaderSystem& loader, const Handle<Font>& handle)
	{
		return loader.fontLoader_->Get(handle);
	}

	Bool FontResource::Contains(Uint32 assetId)const
	{
		return assetHandleMap_.contains(assetId);
	}

	void FontResource::Unload(LoaderSystem& loader, Uint32 assetId)
	{
		if (!assetHandleMap_.contains(assetId))
		{
			return;
		}

		Handle<Font> handle = assetHandleMap_.at(assetId);
		loader.fontLoader_->Clear(handle);
		assetHandleMap_.erase(assetId);
	}

	void FontResource::Update(LoaderSystem& loader, ID3D12Device* device, ID3D12CommandQueue* cmdQueue, BindlessHeap* bindlessHeap)
	{
		for (const Handle<Font>& handle : assetHandleMap_ | std::ranges::views::values)
		{
			Font* font = loader.fontLoader_->Get(handle);
			if (!font)
			{
				continue;
			}

			font->Update();
			font->UploadAtlas(device, cmdQueue, bindlessHeap);
		}
	}

	void FontResource::Clear(LoaderSystem& loader)
	{
		for (Handle<Font>& handle : assetHandleMap_ | std::ranges::views::values)
		{
			loader.fontLoader_->Clear(handle);
		}

		assetHandleMap_.clear();
	}

	REGISTER_ASSET(AssetType::Font, FontResource);
}
