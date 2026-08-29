#include <GraphicsEngine/Model/Material/MaterialLoader.h>
#include <FoundationEngine/Resource/LoaderSystem.h>
#include <FoundationEngine/Serialization/Binary/BinaryArchive.h>

namespace SeedCore
{
	Handle<Surface> MaterialLoader::Load(LoaderSystem& loader, String filePath)
	{
		std::filesystem::path path(filePath.c_str());
		if (path.extension() != ".material" || !std::filesystem::exists(path))
		{
			return Handle<Surface>::null();
		}

		BinaryInputArchive archive;
		if (!archive.Read(filePath))
		{
			return Handle<Surface>::null();
		}

		Handle<Surface> handle = pool_.Create();
		Surface* surface = pool_.Get(handle);
		if (!surface)
		{
			return Handle<Surface>::null();
		}
		surface->Serialize(archive);

		return handle;
	}

	Surface* MaterialLoader::Get(const Handle<Surface>& handle)
	{
		return pool_.Get(handle);
	}

	void MaterialLoader::Clear(Handle<Surface>& handle)noexcept
	{
		pool_.Destroy(handle);
	}

	Bool MaterialLoader::Save(const Surface& surface, String filePath)
	{
		BinaryOutputArchive archive;
		const_cast<Surface&>(surface).Serialize(archive);
		return archive.Write(filePath);
	}
}
