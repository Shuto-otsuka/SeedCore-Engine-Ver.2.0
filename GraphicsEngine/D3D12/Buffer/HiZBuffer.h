#pragma once
#include <FoundationEngine/Prelude.h>
#include <GraphicsEngine/D3D12/Buffer/ConstantBuffer.h>
#include <GraphicsEngine/D3D12/PipelineState/RootSignature.h>
#include <GraphicsEngine/D3D12/PipelineState/PipelineStateObject.h>
#include <GraphicsEngine/D3D12/PipelineState/ComputeShader.h>

namespace SeedCore
{
	class BindlessHeap;
	class ShaderCache;
	class D3D12CommandList;
	class GeometryBuffer;

	/**
	* [EN]
	* Hi-Z (hierarchical depth) pyramid for occlusion culling.
	*
	* Built every frame from the depth-prepass result: mip 0 is a half-resolution
	* MIN-reduction of the depth buffer and each further mip halves again down to
	* 1x1. With reverse-Z the minimum is the farthest surface, so the
	* Amplification Shader can conservatively reject meshlets whose bounding
	* sphere is entirely behind the already-rasterised scene (IsVisibleHiZ in
	* Culling.hlsli).
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* オクルージョンカリング用の Hi-Z（階層深度）ピラミッド。
	*
	* 毎フレーム、デプスプリパスの結果から構築する: ミップ 0 は深度バッファの
	* 半解像度 MIN 縮小で、以降のミップは 1x1 まで半減を繰り返す。reverse-Z では
	* 最小値が最も遠い面になるため、Amplification Shader は包囲球がラスタライズ
	* 済みシーンの完全に後ろにあるメシュレットを保守的に棄却できる
	* （Culling.hlsli の IsVisibleHiZ）。
	*/
	class HiZBuffer
	{
	private:
		struct HiZBuildConstants
		{
			Uint sourceIndex_ = 0;
			Uint destinationIndex_ = 0;
			Uint destinationWidth_ = 0;
			Uint destinationHeight_ = 0;
			Uint sourceWidth_ = 0;
			Uint sourceHeight_ = 0;
			Uint sourceIsDepth_ = 0;
			Uint hiZBuildPadding0_ = 0;
		};

	public:
		HiZBuffer() = default;
		~HiZBuffer() = default;

		void Create(ID3D12Device* device, BindlessHeap* bindlessHeap, ShaderCache& shaderCache, RootSignature& rootSignature, PipelineStateObject& pipelineStateObject, Uint32 width, Uint32 height, Uint depthShaderResourceViewIndex);

		void Destroy(BindlessHeap* bindlessHeap);

		void Resize(ID3D12Device* device, BindlessHeap* bindlessHeap, ShaderCache& shaderCache, RootSignature& rootSignature, PipelineStateObject& pipelineStateObject, Uint32 width, Uint32 height, Uint depthShaderResourceViewIndex);

		/**
		* [EN]
		* Builds the pyramid from the current depth-prepass result. Transitions
		* the depth buffer to a shader-readable state during the build and back
		* to DEPTH_WRITE afterwards, and leaves the pyramid readable by the
		* following Amplification Shader passes.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 現在のデプスプリパス結果からピラミッドを構築する。構築中はデプス
		* バッファをシェーダ読み取り可能ステートへ遷移させ、終了後 DEPTH_WRITE に
		* 戻す。ピラミッドは後続の Amplification Shader パスから読める状態で残す。
		*/
		void Build(D3D12CommandList* cmdList, GeometryBuffer& geometryBuffer, ID3D12DescriptorHeap* heap);

		[[nodiscard]] Uint ShaderResourceViewIndex()const { return shaderResourceViewIndex_; }

	private:
		static constexpr Uint maxMipCount = 16;

		Microsoft::WRL::ComPtr<ID3D12Resource> resource_;
		D3D12_RESOURCE_STATES state_ = D3D12_RESOURCE_STATE_COMMON;

		Uint mipCount_ = 0;
		Uint mipWidths_[maxMipCount] = {};
		Uint mipHeights_[maxMipCount] = {};
		Uint mipUnorderedAccessViewIndices_[maxMipCount] = {};
		Uint shaderResourceViewIndex_ = 0;

		DynamicArray<ResourcePtr<ConstantBuffer<HiZBuildConstants>>> mipConstantBuffers_;
		DynamicArray<HiZBuildConstants> mipConstants_;

		BindlessHeap* bindlessHeap_ = nullptr;
		RootSignature* rootSignature_ = nullptr;
		Handle<ComputeShader> buildShader_;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;
	};
}
