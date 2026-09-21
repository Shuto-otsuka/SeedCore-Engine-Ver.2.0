#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/Utility/Handle.h>
#include <FoundationEngine/Utility/FlatMap.h>
#include <FoundationEngine/Resource/Asset/Asset.h>
#include <GraphicsEngine/Model/Animation/Animation.h>

namespace SeedCore
{
	struct LoaderSystem;
	class ResourceCache;

	class SEEDCORE_API AnimationResource :public Asset, public NonCopyable
	{
	public:
		AnimationResource() = default;
		~AnimationResource() = default;

		void Load(const AssetContext& context, Uint32 assetId)override;

		void Unload(const AssetContext& context, Uint32 assetId)override;

		Handle<Animation> Load(LoaderSystem& loader, ResourceCache& cache, Uint32 assetId);

		Handle<Animation> GetHandle(Uint32 assetId)const;

		Animation* Resolve(LoaderSystem& loader, const Handle<Animation>& handle);

		Bool Contains(Uint32 assetId)const;

		void Unload(LoaderSystem& loader, Uint32 assetId);

	private:
		FlatMap<Uint32, Handle<Animation>> assetHandleMap_;
	};
}
