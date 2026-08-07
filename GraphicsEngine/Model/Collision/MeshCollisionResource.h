#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/Utility/Handle.h>
#include <FoundationEngine/Utility/FlatMap.h>
#include <GraphicsEngine/Model/Collision/MeshCollision.h>

namespace SeedCore
{
	struct LoaderSystem;
	class ResourceCache;

	class SEEDCORE_API MeshCollisionResource :public NonCopyable
	{
	public:
		MeshCollisionResource() = default;
		~MeshCollisionResource() = default;

		Handle<MeshCollision> Load(LoaderSystem& loader, ResourceCache& cache, Uint32 assetId);

		Handle<MeshCollision> GetHandle(Uint32 assetId)const;

		MeshCollision* Resolve(LoaderSystem& loader, const Handle<MeshCollision>& handle);

		Bool Contains(Uint32 assetId)const;

		void Unload(LoaderSystem& loader, Uint32 assetId);

	private:
		FlatMap<Uint32, Handle<MeshCollision>> assetHandleMap_;
	};
}
