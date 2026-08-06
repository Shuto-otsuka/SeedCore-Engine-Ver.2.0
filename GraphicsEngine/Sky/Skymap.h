#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/Utility/Handle.h>

namespace SeedCore
{
	class BindlessHeap;

	/**
	* [EN]
	* A loaded skymap asset: just the decoded equirectangular HDR source
	* texture. It is only a *source* of sky radiance now - the environment /
	* irradiance / prefiltered cubes and the whole IBL machinery live in the
	* SkyRenderer, so the same machinery can also be fed by other sky sources
	* (e.g. the procedural sky, which needs no skymap).
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 読み込まれたスカイマップアセット: デコード済みパノラマ HDR ソース
	* テクスチャのみ。今は空の放射輝度の *ソース* にすぎない。environment /
	* irradiance / prefilter キューブと IBL 機構一式は SkyRenderer 側に
	* あるので、同じ機構を別の空ソース（例: プロシージャル空。スカイマップ
	* 不要）でも駆動できる。
	*/
	class Skymap :public NonCopyable
	{
	public:
		static constexpr Uint invalidIndex_ = 0xFFFFFFFF;

	public:
		Skymap() = default;
		~Skymap() = default;

		Skymap(Skymap&&)noexcept = default;
		Skymap& operator=(Skymap&&)noexcept = default;

		/**
		* [EN] Loads an equirectangular ".hdr" into the equirect source texture.
		* [JP] パノラマ ".hdr" を equirect ソーステクスチャへロードする。
		*/
		Bool LoadEquirect(ID3D12Device* device, ID3D12CommandQueue* cmdQueue, BindlessHeap* heap, const String& filePath);

		/**
		* [EN] Loads a baked ".skymap" cache (decoded equirect pixels) directly.
		* [JP] ベイク済み ".skymap" キャッシュ（デコード済み equirect）を直接ロード。
		*/
		Bool LoadSkymapCache(ID3D12Device* device, ID3D12CommandQueue* cmdQueue, BindlessHeap* heap, const String& filePath);

		void Release(BindlessHeap* heap)noexcept;

	public:
		[[nodiscard]] Uint EquirectShaderResourceViewIndex()const;

		[[nodiscard]] Bool Valid()const;

		[[nodiscard]] Handle<Skymap> GetHandle()const;

		void SetHandle(const Handle<Skymap>& handle);

	private:
		Bool CreateEquirectTexture(ID3D12Device* device, ID3D12CommandQueue* cmdQueue, BindlessHeap* heap, DXGI_FORMAT format, Uint width, Uint height, Uint rowPitch, const void* pixels);

	private:
		Handle<Skymap> handle_;

		Microsoft::WRL::ComPtr<ID3D12Resource> equirectResource_;
		Uint equirectShaderResourceViewIndex_ = invalidIndex_;
	};
}
