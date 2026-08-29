#include <GraphicsEngine/Model/Material/MaterialResource.h>
#include <GraphicsEngine/Model/Material/MaterialLoader.h>
#include <FoundationEngine/Resource/ResourceCache.h>
#include <FoundationEngine/Resource/LoaderSystem.h>

namespace SeedCore
{
	Handle<Surface> MaterialResource::Load(LoaderSystem& loader, ResourceCache& cache, Uint32 assetId)
	{
		if (assetHandleMap_.contains(assetId))
		{
			return assetHandleMap_.at(assetId);
		}

		Asset* asset = cache.GetAsset(assetId);
		if (!asset)
		{
			return Handle<Surface>::null();
		}

		Handle<Surface> handle = loader.materialLoader_->Load(loader, asset->fullpath_);
		if (handle.empty())
		{
			return Handle<Surface>::null();
		}

		assetHandleMap_.insert({ assetId, handle });
		return handle;
	}

	Handle<Surface> MaterialResource::GetHandle(Uint32 assetId)const
	{
		if (!assetHandleMap_.contains(assetId))
		{
			return Handle<Surface>::null();
		}
		return assetHandleMap_.at(assetId);
	}

	Surface* MaterialResource::Resolve(LoaderSystem& loader, const Handle<Surface>& handle)
	{
		return loader.materialLoader_->Get(handle);
	}

	Bool MaterialResource::Contains(Uint32 assetId)const
	{
		return assetHandleMap_.contains(assetId);
	}

	void MaterialResource::Unload(LoaderSystem& loader, Uint32 assetId)
	{
		if (!assetHandleMap_.contains(assetId))
		{
			return;
		}

		Handle<Surface> handle = assetHandleMap_.at(assetId);
		loader.materialLoader_->Clear(handle);
		assetHandleMap_.erase(assetId);
	}

	Surface MaterialResource::Resolve(LoaderSystem& loader, const Crister& crister, Uint32 slot, const DynamicArray<Uint32>& materialIDs)
	{
		if (slot < materialIDs.size() && materialIDs[slot] != 0)
		{
			if (Surface* assigned = Resolve(loader, GetHandle(materialIDs[slot])))
			{
				return *assigned;
			}
		}

		const DynamicArray<Surface>& seed = crister.Surfaces();
		return slot < seed.size() ? seed[slot] : Surface{};
	}
}
