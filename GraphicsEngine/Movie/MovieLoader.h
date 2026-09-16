#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/Utility/Handle.h>
#include <FoundationEngine/Pool/StablePool.h>
#include <GraphicsEngine/Movie/Video.h>

namespace SeedCore
{
	/**
	* [EN]
	* Loads ".mp4"/".movie" assets into pooled Video objects: starts up
	* Media Foundation once, opens the decoder per asset and reads the
	* native frame size. Mirrors the TextureLoader / ModelLoader pattern:
	* the pool owns every Video, handles identify them, and Clear tears the
	* decoder and its GPU resources down before freeing the slot.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* ".mp4"/".movie" アセットを、プールされた Video へ読み込む。Media
	* Foundation を一度だけ起動し、アセットごとにデコーダを開いて元の
	* フレームサイズを読む。TextureLoader / ModelLoader と同じ流儀:
	* 全ての Video をプールが所有し、ハンドルで識別し、Clear がスロット解放前に
	* デコーダと GPU リソースを破棄する。
	*/
	class MovieLoader :public NonCopyable
	{
	public:
		MovieLoader();

		~MovieLoader();

		Handle<Video> Load(String filePath);

		Video* Get(const Handle<Video>& handle);

		void Clear(Handle<Video>& handle)noexcept;

	private:
		StablePool<Video> pool_;

		Bool ownsComInitialize_ = false;

		Bool mfStarted_ = false;
	};
}
