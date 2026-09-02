#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/Utility/Handle.h>
#include <FoundationEngine/Utility/FlatMap.h>
#include <GraphicsEngine/Model/Skeleton/Skeleton.h>

namespace SeedCore
{
	struct LoaderSystem;
	class ResourceCache;

	/**
	* [EN]
	* Manages the mapping between asset IDs and loaded ".skeleton" handles.
	* Mirrors MaterialResource: Load by asset ID, retrieve handles, resolve
	* to SkeletonRig pointers, unload when no longer needed.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* アセット ID とロード済み ".skeleton" ハンドル間のマッピングを管理する。
	* MaterialResource と同型: アセット ID でロード、ハンドル取得、
	* SkeletonRig ポインタへの解決、不要時のアンロード。
	*/
	class SEEDCORE_API SkeletonResource :public NonCopyable
	{
	public:
		SkeletonResource() = default;
		~SkeletonResource() = default;

		Handle<SkeletonRig> Load(LoaderSystem& loader, ResourceCache& cache, Uint32 assetId);

		Handle<SkeletonRig> GetHandle(Uint32 assetId)const;

		SkeletonRig* Resolve(LoaderSystem& loader, const Handle<SkeletonRig>& handle);

		Bool Contains(Uint32 assetId)const;

		void Unload(LoaderSystem& loader, Uint32 assetId);

	private:
		FlatMap<Uint32, Handle<SkeletonRig>> assetHandleMap_;
	};
}
