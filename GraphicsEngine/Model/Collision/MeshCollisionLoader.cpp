#include <GraphicsEngine/Model/Collision/MeshCollisionLoader.h>
#include <GraphicsEngine/Model/Crister.h>
#include <FoundationEngine/Resource/LoaderSystem.h>
#include <FoundationEngine/Serialization/Binary/BinaryArchive.h>

namespace SeedCore
{
	Handle<MeshCollision> MeshCollisionLoader::Load(LoaderSystem& loader, String filePath)
	{
		std::filesystem::path path(filePath.c_str());
		if (path.extension() != ".collision" || !std::filesystem::exists(path))
		{
			return Handle<MeshCollision>::null();
		}

		Handle<MeshCollision> handle = pool_.Create();
		MeshCollision* meshCollision = pool_.Get(handle);
		if (!meshCollision)
		{
			return Handle<MeshCollision>::null();
		}

		BinaryInputArchive archive;
		if (!archive.Read(filePath))
		{
			pool_.Destroy(handle);
			return Handle<MeshCollision>::null();
		}
		meshCollision->Serialize(archive);

		return handle;
	}

	MeshCollision* MeshCollisionLoader::Get(const Handle<MeshCollision>& handle)
	{
		return pool_.Get(handle);
	}

	void MeshCollisionLoader::Clear(Handle<MeshCollision>& handle)noexcept
	{
		pool_.Destroy(handle);
	}

	Bool MeshCollisionLoader::Bake(const Crister& crister, MeshCollisionDetail detail, String filePath)
	{
		MeshCollision meshCollision;
		if (!crister.BakeCollision(detail, meshCollision.positions_, meshCollision.indices_))
		{
			return false;
		}

		BinaryOutputArchive archive;
		meshCollision.Serialize(archive);
		return archive.Write(filePath);
	}
}
