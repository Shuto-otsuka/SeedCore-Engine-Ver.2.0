#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/Utility/Handle.h>
#include <FoundationEngine/Utility/FlatMap.h>
#include <GraphicsEngine/Effect/Effekseer/EffekseerLoader.h>

namespace SeedCore
{
	struct LoaderSystem;
	class ResourceCache;

	class EffekseerResource :public NonCopyable
	{
	public:
		EffekseerResource() = default;
		~EffekseerResource() = default;

		Handle<EffekseerEffectHandle> Load(LoaderSystem& loader, ResourceCache& cache, Uint32 assetId);

		Handle<EffekseerEffectHandle> GetHandle(Uint32 assetId)const;

		Effekseer::EffectRef* Resolve(LoaderSystem& loader, const Handle<EffekseerEffectHandle>& handle);

		Bool Contains(Uint32 assetId)const;

		void Unload(LoaderSystem& loader, Uint32 assetId);

	private:
		FlatMap<Uint32, Handle<EffekseerEffectHandle>> assetHandleMap_;
	};
}
