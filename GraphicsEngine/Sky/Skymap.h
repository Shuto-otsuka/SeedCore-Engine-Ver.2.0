#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/Utility/Handle.h>

namespace SeedCore
{
	class BindlessHeap;
	class D3D12CommandQueue;

	/**
	* [EN]
	* One loaded skymap: the equirectangular source texture and its
	* bindless shader-resource-view index. Filled and released by
	* SkymapLoader; holds no loading logic of its own.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 読み込み済みのスカイマップ1つ: equirect ソーステクスチャと、その
	* バインドレスなシェーダーリソースビューのインデックス。中身を詰めるのも
	* 解放するのも SkymapLoader で、自身は読み込み処理を持たない。
	*/
	class Skymap :public NonCopyable
	{
		friend class SkymapLoader;

	public:
		Skymap() = default;
		~Skymap() = default;

		Skymap(Skymap&&)noexcept = default;
		Skymap& operator=(Skymap&&)noexcept = default;

	public:
		[[nodiscard]] Uint ShaderResourceViewIndex()const;

		[[nodiscard]] Bool Valid()const;

		[[nodiscard]] Handle<Skymap> GetHandle()const;

	private:
		Handle<Skymap> handle_;

		Microsoft::WRL::ComPtr<ID3D12Resource> resource_;

		Uint shaderResourceViewIndex_ = SC_INVALID;
	};
}
