#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/Utility/Handle.h>
#include <FoundationEngine/Utility/FlatMap.h>

namespace SeedCore
{
	class Skymap;
	struct LoaderSystem;
	class ResourceCache;
	class BindlessHeap;
	class D3D12CommandQueue;

	/**
	* [EN]
	* Manages the mapping between asset IDs and loaded Skymap handles.
	* Mirrors the ModelResource pattern: Load by asset ID, retrieve handles,
	* resolve to Skymap pointers, and unload when no longer needed.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* アセット ID とロード済み Skymap ハンドル間のマッピングを管理する。
	* ModelResource パターンと同じ: アセット ID でロード、ハンドル取得、
	* Skymap ポインタへの解決、不要時のアンロード。
	*/
	class SkymapResource :public NonCopyable
	{
	public:
		SkymapResource() = default;
		~SkymapResource() = default;

		Handle<Skymap> Load(LoaderSystem& loader, ID3D12Device* device, D3D12CommandQueue* cmdQueue, BindlessHeap* heap, ResourceCache& cache, Uint32 assetId);

		Handle<Skymap> GetHandle(Uint32 assetId)const;

		Skymap* Resolve(LoaderSystem& loader, const Handle<Skymap>& handle);

		Bool Contains(Uint32 assetId)const;

		void Unload(LoaderSystem& loader, Uint32 assetId, BindlessHeap* heap);

	private:
		FlatMap<Uint32, Handle<Skymap>> assetHandleMap_;
	};
}
