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
	* Manages PSO creation for the bloom compute passes (KawaseBloomCS.hlsl - a
	* progressive downsample chain followed by a progressive additive upsample
	* chain over 6 levels, producing the broad glow ToneMappingCS.hlsl samples
	* and adds into the HDR color before exposure). Compiles 11 PSOs from the
	* same .hlsl file at different entry points (DownsamplePrefilter,
	* Downsample1..5, Upsample4..0), the same "one file, many entry points"
	* pattern as LensFlare and GlobalIlluminationDenoiseShader.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* ブルームのコンピュートパス(KawaseBloomCS.hlsl — 6レベルにわたる段階的な
	* ダウンサンプルチェーンと、それに続く段階的な加算アップサンプル
	* チェーンで広いグローを生成する。ToneMappingCS.hlsl がその結果を
	* サンプルし、露出適用前のHDRカラーへ加算する)の PSO 管理。同じ .hlsl
	* ファイルの別エントリポイント(DownsamplePrefilter, Downsample1..5,
	* Upsample4..0)から11個のPSOをコンパイルする、LensFlare や
	* GlobalIlluminationDenoiseShader と同じ「1ファイル複数エントリ
	* ポイント」パターン。
	*/
	class KawaseBloom
	{
	public:
		KawaseBloom(RootSignature& rootSignature, PipelineStateObject& pipelineStateObject);
		~KawaseBloom() = default;

		void Create(ShaderCache& shaderCache, ID3D12Device* device);

		[[nodiscard]] ID3D12PipelineState* GetPrefilterPipelineState()const;

		/// [EN] levelIndex is 0-4 for Downsample1..5 respectively - the pass
		///      that writes bloom level levelIndex+1.
		/// [JP] levelIndex は 0-4 でそれぞれ Downsample1..5 に対応する
		///      (ブルームのレベル levelIndex+1 へ書くパス)。
		[[nodiscard]] ID3D12PipelineState* GetDownsamplePipelineState(Uint32 levelIndex)const;

		/// [EN] levelIndex is 0-4 for Upsample0..4 respectively - the pass
		///      that additively accumulates into bloom level levelIndex.
		/// [JP] levelIndex は 0-4 でそれぞれ Upsample0..4 に対応する
		///      (ブルームのレベル levelIndex へ加算で積むパス)。
		[[nodiscard]] ID3D12PipelineState* GetUpsamplePipelineState(Uint32 levelIndex)const;

		[[nodiscard]] ID3D12RootSignature* GetRootSignature()const;

	private:
		static constexpr Uint32 levelCount = 6;
		static constexpr Uint32 chainPassCount = levelCount - 1;

		Handle<ComputeShader> prefilterComputeShader_;
		Handle<Microsoft::WRL::ComPtr<ID3D12PipelineState>> prefilterPipelineStateHandle_;

		Handle<ComputeShader> downsampleComputeShader_[chainPassCount];
		Handle<Microsoft::WRL::ComPtr<ID3D12PipelineState>> downsamplePipelineStateHandle_[chainPassCount];

		Handle<ComputeShader> upsampleComputeShader_[chainPassCount];
		Handle<Microsoft::WRL::ComPtr<ID3D12PipelineState>> upsamplePipelineStateHandle_[chainPassCount];

		Handle<RootSignature> bloomRootSignature_;

		RootSignature& rootSignature_;
		PipelineStateObject& pipelineStateObject_;
	};
}
