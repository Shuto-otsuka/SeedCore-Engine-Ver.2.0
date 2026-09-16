#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/Utility/Handle.h>
#include <FoundationEngine/Utility/FlatMap.h>
#include <FoundationEngine/Resource/Asset.h>

namespace SeedCore
{
	class Sound;
	struct LoaderSystem;
	class ResourceCache;

	class SEEDCORE_API AudioResource :public Asset, public NonCopyable
	{
	public:
		AudioResource() = default;
		~AudioResource() = default;

		void Load(const AssetContext& context, Uint32 assetId)override;

		void Unload(const AssetContext& context, Uint32 assetId)override;

		Handle<Sound> Load(LoaderSystem& loader, ResourceCache& cache, Uint32 assetId);

		Handle<Sound> GetHandle(Uint32 assetId)const;

		Sound* Resolve(LoaderSystem& loader, const Handle<Sound>& handle);

		Bool Contains(Uint32 assetId)const;

		void Unload(LoaderSystem& loader, Uint32 assetId);

	private:
		FlatMap<Uint32, Handle<Sound>> assetHandleMap_;
	};
}
