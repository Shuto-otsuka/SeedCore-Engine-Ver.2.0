#include <GraphicsEngine/Model/ModelResource.h>
#include <GraphicsEngine/Model/ModelLoader.h>
#include <GraphicsEngine/D3D12/Descriptor/BindlessHeap.h>
#include <FoundationEngine/Resource/ResourceCache.h>
#include <FoundationEngine/Resource/LoaderSystem.h>

namespace SeedCore
{
	Handle<Crister> ModelResource::Load(LoaderSystem& loader, ID3D12Device* device, D3D12CommandQueue* cmdQueue, BindlessHeap* heap, BC7CompressShader& bc7Shader, ResourceCache& cache, Uint32 assetId)
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

	Bool ModelResource::GenerateCollision(LoaderSystem& loader, ID3D12Device* device, D3D12CommandQueue* cmdQueue, BindlessHeap* heap, BC7CompressShader& bc7Shader, ResourceCache& cache, Uint32 assetId, MeshCollisionDetail detail)
	{
		Asset* asset = cache.GetAsset(assetId);
		if (!asset)
		{
			return false;
		}

		/// [EN] The bake reads CPU-resident cluster data off the Crister, so the model has to be loaded first - Load() is a no-op when it already is.
		/// [JP] ベイクは Crister の CPU 常駐クラスタデータを読むため、先にモデルをロードしておく必要がある - 既にロード済みなら Load() は何もしない。
		Handle<Crister> handle = Load(loader, device, cmdQueue, heap, bc7Shader, cache, assetId);
		Crister* crister = loader.modelLoader_->Get(handle);
		if (!crister)
		{
			return false;
		}

		std::filesystem::path collisionPath(asset->fullpath_.c_str());
		collisionPath.replace_extension("");
		collisionPath += (detail == MeshCollisionDetail::Exact) ? ".exact.collision" : ".proxy.collision";

		return loader.meshCollisionLoader_->Bake(*crister, detail, String(collisionPath.string()));
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
