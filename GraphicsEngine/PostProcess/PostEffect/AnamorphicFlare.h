#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/Utility/Handle.h>
#include <GraphicsEngine/D3D12/PipelineState/RootSignature.h>
#include <GraphicsEngine/D3D12/PipelineState/PipelineStateObject.h>

namespace SeedCore
{
	class ShaderCache;
	class ComputeShader;

	/**
	* [EN]
	* Manages PSO creation for the anamorphic-flare compute passes
	* (AnamorphicFlareCS.hlsl - a multi-pass horizontal Kawase streak run
	* inside a 2:1 horizontally squeezed buffer, producing the long cine
	* streak ToneMappingCS.hlsl samples and adds into the HDR color before
	* exposure). Compiles 6 PSOs from the same .hlsl file at different entry
	* points (Prefilter, BlurPass1..4, Compose), the same "one file, many
	* entry points" pattern as LensFlare and KawaseBloom.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* アナモルフィックフレアのコンピュートパス(AnamorphicFlareCS.hlsl —
	* 2:1 に横圧縮したバッファの中で多段階の横方向Kawaseストリークを走らせ、
	* シネマ調の長い筋を生成する。ToneMappingCS.hlsl がその結果をサンプル
	* し、露出適用前のHDRカラーへ加算する)の PSO 管理。同じ .hlsl ファイルの
	* 別エントリポイント(Prefilter, BlurPass1..4, Compose)から6個のPSOを
	* コンパイルする、LensFlare や KawaseBloom と同じ「1ファイル複数エントリ
	* ポイント」パターン。
	*/
	class AnamorphicFlare
	{
	public:
		AnamorphicFlare(RootSignature& rootSignature, PipelineStateObject& pipelineStateObject);
		~AnamorphicFlare() = default;

		void Create(ShaderCache& shaderCache, ID3D12Device* device);

		[[nodiscard]] ID3D12PipelineState* GetPrefilterPipelineState()const;

		/// [EN] passIndex is 0-3 for BlurPass1..4 respectively.
		/// [JP] passIndex は 0-3 でそれぞれ BlurPass1..4 に対応する。
		[[nodiscard]] ID3D12PipelineState* GetBlurPipelineState(Uint32 passIndex)const;

		[[nodiscard]] ID3D12PipelineState* GetComposePipelineState()const;

		[[nodiscard]] ID3D12RootSignature* GetRootSignature()const;

	private:
		static constexpr Uint32 blurPassCount = 4;

		Handle<ComputeShader> prefilterComputeShader_;
		Handle<Microsoft::WRL::ComPtr<ID3D12PipelineState>> prefilterPipelineStateHandle_;

		Handle<ComputeShader> blurComputeShader_[blurPassCount];
		Handle<Microsoft::WRL::ComPtr<ID3D12PipelineState>> blurPipelineStateHandle_[blurPassCount];

		Handle<ComputeShader> composeComputeShader_;
		Handle<Microsoft::WRL::ComPtr<ID3D12PipelineState>> composePipelineStateHandle_;

		Handle<RootSignature> anamorphicFlareRootSignature_;

		RootSignature& rootSignature_;
		PipelineStateObject& pipelineStateObject_;
	};
}
