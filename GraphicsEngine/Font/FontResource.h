#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/Utility/Handle.h>
#include <FoundationEngine/Utility/FlatMap.h>
#include <FoundationEngine/Resource/Asset/Asset.h>

namespace SeedCore
{
	class Font;
	class BindlessHeap;
	struct LoaderSystem;
	class ResourceCache;

	class FontResource :public Asset, public NonCopyable
	{
	public:
		FontResource() = default;
		~FontResource() = default;

		void Load(const AssetContext& context, Uint32 assetId)override;

		void Unload(const AssetContext& context, Uint32 assetId)override;

		void Clear(const AssetContext& context)override;

		Handle<Font> Load(LoaderSystem& loader, ResourceCache& cache, Uint32 assetId, Float fontSize);

		Handle<Font> GetHandle(Uint32 assetId)const;

		Font* Resolve(LoaderSystem& loader, const Handle<Font>& handle);

		Bool Contains(Uint32 assetId)const;

		void Unload(LoaderSystem& loader, Uint32 assetId);

		void Update(LoaderSystem& loader, ID3D12Device* device, ID3D12CommandQueue* cmdQueue, BindlessHeap* bindlessHeap);

		void Clear(LoaderSystem& loader);

	private:
		FlatMap<Uint32, Handle<Font>> assetHandleMap_;
	};
}
