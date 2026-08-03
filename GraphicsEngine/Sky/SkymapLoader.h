#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/Utility/Handle.h>
#include <FoundationEngine/Pool/StablePool.h>
#include <GraphicsEngine/Sky/Skymap.h>

namespace SeedCore
{
	class BindlessHeap;

	/**
	* [EN]
	* Loads skymap assets (".hdr" equirectangular or ".dds" cube) into pooled
	* Skymap resource bundles. Mirrors the ImageLoader / ModelLoader pattern:
	* Load returns a handle, Get resolves it, Clear releases it.
	*
	* The loader only fills the GPU resources; the IBL convolution passes that
	* turn the environment into irradiance / prefiltered cubes run later in the
	* SkyRenderer (which owns a command list).
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* スカイマップアセット（".hdr" パノラマ or ".dds" キューブ）をプールされた
	* Skymap リソース束へ読み込む。ImageLoader / ModelLoader と同じ流儀:
	* Load がハンドルを返し、Get で解決、Clear で解放。
	*
	* ローダーは GPU リソースを用意するだけ。environment を irradiance /
	* prefilter キューブへ変換する IBL 畳み込みパスは、コマンドリストを持つ
	* SkyRenderer 側で後から実行する。
	*/
	class SkymapLoader :public NonCopyable
	{
	public:
		SkymapLoader() = default;
		~SkymapLoader() = default;

		Handle<Skymap> Load(ID3D12Device* device, ID3D12CommandQueue* cmdQueue, BindlessHeap* heap, String filePath);

		Skymap* Get(const Handle<Skymap>& handle);

		void Clear(Handle<Skymap>& handle, BindlessHeap* heap)noexcept;

	private:
		StablePool<Skymap> pool_;
	};
}
