#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/Utility/Handle.h>
#include <FoundationEngine/Utility/FlatMap.h>
#include <FoundationEngine/Resource/Asset/Asset.h>
#include <GraphicsEngine/Effect/Effekseer/EffekseerLoader.h>

namespace SeedCore
{
	struct LoaderSystem;
	class ResourceCache;

	class EffekseerResource :public Asset, public NonCopyable
	{
	public:
		EffekseerResource() = default;
		~EffekseerResource() = default;

		void Load(const AssetContext& context, Uint32 assetId)override;

		void Unload(const AssetContext& context, Uint32 assetId)override;

		Handle<EffekseerEffect> Load(LoaderSystem& loader, ResourceCache& cache, Uint32 assetId);

		Handle<EffekseerEffect> GetHandle(Uint32 assetId)const;

		EffekseerEffect* Resolve(LoaderSystem& loader, const Handle<EffekseerEffect>& handle);

		Bool Contains(Uint32 assetId)const;

		void Unload(LoaderSystem& loader, Uint32 assetId);

	private:
		FlatMap<Uint32, Handle<EffekseerEffect>> assetHandleMap_;
	};
}
