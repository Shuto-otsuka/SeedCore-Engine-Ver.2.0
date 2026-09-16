#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/Utility/Handle.h>
#include <FoundationEngine/Pool/StablePool.h>
#include <GraphicsEngine/Font/Font.h>

namespace SeedCore
{
	/**
	* [EN]
	* Loads ".ttf"/".otf"/".ttc" font files into pooled Font objects: opens
	* the FreeType face, the HarfBuzz font and the msdfgen handle, and sets
	* up the dynamic MTSDF atlas. Mirrors the TextureLoader / ModelLoader
	* pattern: the pool owns every Font, handles identify them, and Clear
	* tears down every third-party handle before freeing the slot.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* ".ttf"/".otf"/".ttc" フォントファイルを、プールされた Font へ読み込む。
	* FreeType の face、HarfBuzz のフォント、msdfgen のハンドルを開き、動的
	* MTSDF アトラスを用意する。TextureLoader / ModelLoader と同じ流儀:
	* 全ての Font をプールが所有し、ハンドルで識別し、Clear がスロット解放前に
	* 各サードパーティのハンドルを破棄する。
	*/
	class FontLoader :public NonCopyable
	{
	public:
		FontLoader() = default;
		~FontLoader() = default;

		Handle<Font> Load(String filePath, Float fontSize);

		Font* Get(const Handle<Font>& handle);

		void Clear(Handle<Font>& handle)noexcept;

	private:
		StablePool<Font> pool_;
	};
}
