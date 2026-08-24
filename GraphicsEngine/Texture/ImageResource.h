#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/Utility/Handle.h>
#include <FoundationEngine/Utility/FlatMap.h>

namespace SeedCore
{
	struct Texture;
	struct LoaderSystem;
	class ResourceCache;
	class BindlessHeap;
	class D3D12CommandQueue;

	class SEEDCORE_API ImageResource :public NonCopyable
	{
	public:
		ImageResource() = default;
		~ImageResource() = default;

		Handle<Texture> Load(LoaderSystem& loader, ID3D12Device* device, D3D12CommandQueue* cmdQueue, BindlessHeap* heap, ResourceCache& cache, Uint32 assetId);

		Handle<Texture> GetHandle(Uint32 assetId)const;

		Uint32 GetID(LoaderSystem& loader, Uint32 assetId)const;

		Texture* Resolve(LoaderSystem& loader, BindlessHeap* heap, const Handle<Texture>& handle, Uint64 frame);

		Bool Contains(Uint32 assetId)const;

		void Unload(LoaderSystem& loader, Uint32 assetId, BindlessHeap* heap);

		void EvictBudget(LoaderSystem& loader, BindlessHeap* heap, Uint64 currentFrame);

	private:
		FlatMap<Uint32, Handle<Texture>> assetHandleMap_;
	};
}
