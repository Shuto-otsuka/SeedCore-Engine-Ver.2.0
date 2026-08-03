#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	/**
	* [EN]
	* Global frame ring for frames-in-flight.
	*
	* The CPU records frame N while the GPU still executes frame N-1, so every
	* upload-heap resource the CPU writes per frame must exist frameCount times.
	* ConstantBuffer / ReadOnlyStructuredBuffer switch their internal resource on
	* Index(). D3D12Context::EndFrame advances the ring after submitting.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* フレームインフライト用のグローバルフレームリング。
	*
	* GPU がフレーム N-1 を実行している間に CPU はフレーム N を記録するため、
	* CPU が毎フレーム書き込むアップロードヒープリソースは frameCount 個
	* 必要になる。ConstantBuffer / ReadOnlyStructuredBuffer は Index() で内部
	* リソースを切り替える。D3D12Context::EndFrame が送信後にリングを進める。
	*/
	class FrameRing
	{
	public:
		static constexpr Uint frameCount = 2;

		[[nodiscard]] static Uint Index() { return index_; }

		static void Advance() { index_ = (index_ + 1) % frameCount; }

	private:
		static inline Uint index_ = 0;
	};
}
