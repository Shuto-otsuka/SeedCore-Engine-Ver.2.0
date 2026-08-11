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
	* Manages PSO creation for the lens-flare compute passes (LensFlareCS.hlsl
	* — a multi-pass Kawase-style directional blur that produces the aperture
	* diffraction-spike ("starburst") flare and ghost-chain ToneMappingCS.hlsl
	* samples and adds into the HDR color before exposure). Compiles 7 PSOs
	* from the same .hlsl file at different entry points (Downsample,
	* BlurPass1..4, Compose, Ghost), same "one file, many entry points"
	* pattern as GlobalIlluminationDenoiseShader.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* レンズフレアのコンピュートパス(LensFlareCS.hlsl — 多段階Kawase式
	* ディレクショナルブラーで絞りの回折スパイク(スターバースト)フレアと
	* ゴーストチェーンを生成する。ToneMappingCS.hlsl がその結果をサンプルし、
	* 露出適用前のHDRカラーへ加算する)の PSO 管理。同じ .hlsl ファイルの
	* 別エントリポイント(Downsample, BlurPass1..4, Compose, Ghost)から
	* 7個のPSOをコンパイルする、GlobalIlluminationDenoiseShader と同じ
	* 「1ファイル複数エントリポイント」パターン。
	*/
	class LensFlare
	{
	public:
		LensFlare(RootSignature& rootSignature, PipelineStateObject& pipelineStateObject);
		~LensFlare() = default;

		void Create(ShaderCache& shaderCache, ID3D12Device* device);

		[[nodiscard]] ID3D12PipelineState* GetDownsamplePipelineState()const;

		/// [EN] passIndex is 0-3 for BlurPass1..4 respectively.
		/// [JP] passIndex は 0-3 でそれぞれ BlurPass1..4 に対応する。
		[[nodiscard]] ID3D12PipelineState* GetBlurPipelineState(Uint32 passIndex)const;

		[[nodiscard]] ID3D12PipelineState* GetComposePipelineState()const;

		[[nodiscard]] ID3D12PipelineState* GetGhostPipelineState()const;

		[[nodiscard]] ID3D12RootSignature* GetRootSignature()const;

	private:
		static constexpr Uint32 blurPassCount = 4;

		Handle<ComputeShader> downsampleComputeShader_;
		Handle<Microsoft::WRL::ComPtr<ID3D12PipelineState>> downsamplePipelineStateHandle_;

		Handle<ComputeShader> blurComputeShader_[blurPassCount];
		Handle<Microsoft::WRL::ComPtr<ID3D12PipelineState>> blurPipelineStateHandle_[blurPassCount];

		Handle<ComputeShader> composeComputeShader_;
		Handle<Microsoft::WRL::ComPtr<ID3D12PipelineState>> composePipelineStateHandle_;

		Handle<ComputeShader> ghostComputeShader_;
		Handle<Microsoft::WRL::ComPtr<ID3D12PipelineState>> ghostPipelineStateHandle_;

		Handle<RootSignature> lensFlareRootSignature_;

		RootSignature& rootSignature_;
		PipelineStateObject& pipelineStateObject_;
	};
}
