#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/Utility/Handle.h>
#include <FoundationEngine/Utility/FlatMap.h>
#include <FoundationEngine/Resource/Asset/Asset.h>

namespace SeedCore
{
	class Video;
	class BindlessHeap;
	struct LoaderSystem;
	class ResourceCache;

	class MovieResource :public Asset, public NonCopyable
	{
	public:
		MovieResource() = default;
		~MovieResource() = default;

		void Load(const AssetContext& context, Uint32 assetId)override;

		void Unload(const AssetContext& context, Uint32 assetId)override;

		void Clear(const AssetContext& context)override;

		Handle<Video> Load(LoaderSystem& loader, ResourceCache& cache, Uint32 assetId);

		Handle<Video> GetHandle(Uint32 assetId)const;

		Video* Resolve(LoaderSystem& loader, const Handle<Video>& handle);

		Bool Contains(Uint32 assetId)const;

		void Unload(LoaderSystem& loader, Uint32 assetId);

		void Update(LoaderSystem& loader, ID3D12Device* device, ID3D12GraphicsCommandList6* cmdList, BindlessHeap* bindlessHeap);

		void Clear(LoaderSystem& loader);

	private:
		FlatMap<Uint32, Handle<Video>> assetHandleMap_;
	};
}
