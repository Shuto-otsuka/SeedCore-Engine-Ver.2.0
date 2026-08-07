#include <GraphicsEngine/Model/Collision/MeshCollisionLoader.h>
#include <GraphicsEngine/Model/Crister.h>
#include <FoundationEngine/Resource/LoaderSystem.h>

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

		try
		{
			std::ifstream ifs(path, std::ios::binary);
			cereal::BinaryInputArchive archive(ifs);
			archive(*meshCollision);
		}
		catch (const cereal::Exception&)
		{
			pool_.Destroy(handle);
			return Handle<MeshCollision>::null();
		}

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

	void MeshCollisionLoader::Bake(const Crister& crister, MeshCollisionDetail detail, String filePath)
	{
		std::filesystem::path path(filePath.c_str());

		MeshCollision meshCollision;
		if (!crister.BakeCollision(detail, meshCollision.positions_, meshCollision.indices_))
		{
			return;
		}

		std::ofstream ofs(path, std::ios::binary);
		cereal::BinaryOutputArchive archive(ofs);
		archive(meshCollision);
	}
}
