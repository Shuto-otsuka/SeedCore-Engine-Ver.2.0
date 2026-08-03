#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	/**
	* [EN]
	* On-disk ".skymap" cache: the decoded equirectangular HDR (pixels +
	* dimensions + format), so subsequent loads skip the DirectXTex HDR decode.
	* Mirrors the ".crister" model-cache idea: bake the expensive-to-produce
	* intermediate once, then read it straight into memory next time. The GPU
	* IBL convolution (cheap) still runs on load.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* ディスク上の ".skymap" キャッシュ: デコード済みパノラマ HDR（ピクセル +
	* 寸法 + フォーマット）。2 回目以降のロードで DirectXTex の HDR デコードを
	* 省略できる。".crister" モデルキャッシュと同じ発想で、生成コストの高い
	* 中間データを 1 度焼き、次回はそのままメモリへ読み込む。GPU の IBL 畳み込み
	* （軽い）はロード時に実行する。
	*/
	struct SkymapCacheHeader
	{
		Char magic_[4];       // 'S','K','Y','M'
		Uint32 version_;      // format version
		Uint32 format_;       // DXGI_FORMAT of the stored pixels
		Uint32 width_;        // equirect width in texels
		Uint32 height_;       // equirect height in texels
		Uint32 rowPitch_;     // bytes per row of the stored pixels
		Uint32 dataSize_;     // total byte count of the pixel payload
		Uint32 headerPadding_;
	};

	inline constexpr Char skymapCacheMagic_[4] = { 'S', 'K', 'Y', 'M' };
	inline constexpr Uint32 skymapCacheVersion_ = 1;

	/**
	* [EN] Writes an equirect ".skymap" cache. pixels points to dataSize bytes.
	*      Returns false on I/O failure.
	* [JP] equirect の ".skymap" キャッシュを書き出す。pixels は dataSize バイト。
	*      I/O 失敗時 false。
	*/
	Bool WriteSkymapCache(const String& filePath, const SkymapCacheHeader& header, const void* pixels);

	/**
	* [EN] Reads an equirect ".skymap" cache into header + pixels (resized).
	*      Returns false if the file is missing, malformed, or the wrong magic.
	* [JP] equirect の ".skymap" キャッシュを header + pixels（リサイズ）へ読む。
	*      ファイル欠落・破損・magic 不一致なら false。
	*/
	Bool ReadSkymapCache(const String& filePath, SkymapCacheHeader& header, DynamicArray<Uint8>& pixels);
}
