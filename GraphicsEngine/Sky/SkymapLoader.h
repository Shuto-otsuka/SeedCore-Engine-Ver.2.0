#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/Utility/Handle.h>
#include <FoundationEngine/Pool/StablePool.h>
#include <GraphicsEngine/Sky/Skymap.h>

namespace SeedCore
{
	class BindlessHeap;
	class D3D12CommandQueue;

	/**
	* [EN]
	* Loads ".hdr" equirectangular sources and prebuilt ".skymap" caches into
	* pooled Skymap objects. Mirrors the TextureLoader / ModelLoader pattern:
	* the pool owns every Skymap, handles identify them, and Clear releases
	* both the GPU texture and the slot.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* ".hdr" の equirect ソースと、ビルド済みの ".skymap" キャッシュを、
	* プールされた Skymap へ読み込む。TextureLoader / ModelLoader と同じ流儀:
	* 全ての Skymap をプールが所有し、ハンドルで識別し、Clear が GPU
	* テクスチャとスロットの両方を解放する。
	*/
	class SkymapLoader :public NonCopyable
	{
	public:
		SkymapLoader() = default;
		~SkymapLoader() = default;

		Handle<Skymap> Load(ID3D12Device* device, D3D12CommandQueue* cmdQueue, BindlessHeap* heap, String filePath);

		Skymap* Get(const Handle<Skymap>& handle);

		void Clear(Handle<Skymap>& handle, BindlessHeap* heap)noexcept;

	private:
		StablePool<Skymap> pool_;
	};
}
