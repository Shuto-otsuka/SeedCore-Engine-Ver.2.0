#pragma once
#include <FoundationEngine/Prelude.h>
#include <GraphicsEngine/D3D12/FrameRing.h>

namespace SeedCore
{
	/// [EN] One instance placed into the TLAS: a BLAS reference plus its world
	///      placement (row-major 3x4) and shading identifiers.
	/// [JP] TLAS に配置する 1 インスタンス。BLAS 参照＋ワールド配置（行優先 3x4）
	///      ＋シェーディング識別子を持つ。
	struct TopLevelInstanceDesc
	{
		Float transform_[3][4] =
		{
			{ 1.0f, 0.0f, 0.0f, 0.0f },
			{ 0.0f, 1.0f, 0.0f, 0.0f },
			{ 0.0f, 0.0f, 1.0f, 0.0f },
		};

		D3D12_GPU_VIRTUAL_ADDRESS bottomLevelAddress_ = 0;
		Uint32 instanceID_ = 0;
		Uint32 instanceMask_ = 0xFF;
		Uint32 hitGroupIndex_ = 0;
	};

	/// [EN] Top-Level Acceleration Structure: the thin per-scene layer listing BLAS
	///      instances and their transforms. Rays traverse this first, then the BLAS.
	///      Rebuilt every frame (instances move), so result/scratch/instance
	///      buffers are frame-ring double-buffered (FrameRing::frameCount slots):
	///      Build() writes into the slot for the current frame while the GPU may
	///      still be reading the previous frame's slot from its ray queries.
	/// [JP] Top-Level Acceleration Structure: BLAS インスタンスと変換を並べる、
	///      シーンに 1 本の薄い層。レイはまずこれを辿り、次に BLAS を辿る。
	///      毎フレーム再構築する（インスタンスが動くため）ので、result/scratch/
	///      instance バッファはフレームリングで二重化する
	///      (FrameRing::frameCount スロット): Build() は現在フレームのスロットへ
	///      書き込み、GPU はまだ前フレームのスロットをレイクエリで読んでいる
	///      可能性がある。
	class TopLevelAccelerationStructure :public NonCopyable
	{
	public:
		TopLevelAccelerationStructure() = default;
		~TopLevelAccelerationStructure() = default;

		Bool Build(ID3D12Device5* device, ID3D12GraphicsCommandList4* commandList, const TopLevelInstanceDesc* instances, Uint32 instanceCount);

		[[nodiscard]] D3D12_GPU_VIRTUAL_ADDRESS Address()const noexcept;

		[[nodiscard]] ID3D12Resource* Resource()const noexcept;

	private:
		Microsoft::WRL::ComPtr<ID3D12Resource> result_[FrameRing::frameCount];
		Microsoft::WRL::ComPtr<ID3D12Resource> scratch_[FrameRing::frameCount];
		Microsoft::WRL::ComPtr<ID3D12Resource> instanceBuffer_[FrameRing::frameCount];

		Uint64 resultCapacity_[FrameRing::frameCount] = {};
		Uint64 scratchCapacity_[FrameRing::frameCount] = {};
		Uint64 instanceCapacity_[FrameRing::frameCount] = {};
	};
}
