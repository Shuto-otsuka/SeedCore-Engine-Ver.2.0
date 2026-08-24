#include <GraphicsEngine/Sky/SkymapLoader.h>
#include <GraphicsEngine/D3D12/Descriptor/BindlessHeap.h>

namespace SeedCore
{
	Handle<Skymap> SkymapLoader::Load(ID3D12Device* device, D3D12CommandQueue* cmdQueue, BindlessHeap* heap, String filePath)
	{
		Handle<Skymap> handle = pool_.Create();
		Skymap* skymap = pool_.Get(handle);
		if (!skymap)
		{
			return Handle<Skymap>::null();
		}

		skymap->SetHandle(handle);

		std::string extension = std::filesystem::path(filePath.str()).extension().string();
		std::transform(extension.begin(), extension.end(), extension.begin(), [](Uchar c) { return static_cast<Char>(std::tolower(c)); });

		Bool loaded = false;
		if (extension == ".skymap")
		{
			/// [EN] ".skymap" is the baked cache: read it back directly.
			/// [JP] ".skymap" はベイク済みキャッシュ: 直接読み戻す。
			loaded = skymap->LoadSkymapCache(device, cmdQueue, heap, filePath);
		}
		else
		{
			/// [EN] ".hdr" (source): always decode the HDR and (re)bake a
			///      sibling ".skymap".
			/// [JP] ".hdr"（ソース）: 常に HDR をデコードし隣に ".skymap" を
			///      （再）ベイクする。
			loaded = skymap->LoadEquirect(device, cmdQueue, heap, filePath);
		}

		if (!loaded)
		{
			skymap->Release(heap);
			pool_.Destroy(handle);
			return Handle<Skymap>::null();
		}

		return handle;
	}

	Skymap* SkymapLoader::Get(const Handle<Skymap>& handle)
	{
		return pool_.Get(handle);
	}

	void SkymapLoader::Clear(Handle<Skymap>& handle, BindlessHeap* heap)noexcept
	{
		Skymap* skymap = pool_.Get(handle);
		if (skymap)
		{
			skymap->Release(heap);
		}
		pool_.Destroy(handle);
	}
}
