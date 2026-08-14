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
	* Manages PSO creation for the shadow SVGF chain (ShadowDenoiseCS.hlsl),
	* whose five passes all live in that one file behind different entry points
	* (the same "one file, many entry points" pattern
	* GlobalIlluminationDenoiseShader uses) and share the same root signature as
	* every other pass (see ShadowShader):
	*   main         - temporal reprojection + moment accumulation
	*   FilterMoments - spatial variance estimate for short-history pixels
	*   ATrousPass1/2/3 - variance-guided wavelet iterations at step 1/2/4
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* シャドウの SVGF チェーン(ShadowDenoiseCS.hlsl)の PSO 管理。5 つのパスは
	* すべて同一ファイル内の別エントリポイントに置かれており
	* (GlobalIlluminationDenoiseShader と同じ「1ファイル複数エントリポイント」
	* 方式)、他の全パスと同じルートシグネチャを共有する(ShadowShader 参照):
	*   main            - 時間的リプロジェクション + モーメント蓄積
	*   FilterMoments   - 履歴の短いピクセル向けの空間的分散推定
	*   ATrousPass1/2/3 - step 1/2/4 の分散誘導ウェーブレット反復
	*/
	class ShadowDenoiseShader
	{
	public:
		ShadowDenoiseShader(RootSignature& rootSignature, PipelineStateObject& pipelineStateObject);
		~ShadowDenoiseShader() = default;

		void Create(ShaderCache& shaderCache, ID3D12Device* device);

		[[nodiscard]] ID3D12PipelineState* GetPipelineState()const;

		[[nodiscard]] ID3D12PipelineState* GetFilterMomentsPipelineState()const;

		[[nodiscard]] ID3D12PipelineState* GetATrousPipelineState(Uint32 passIndex)const;

		[[nodiscard]] ID3D12RootSignature* GetRootSignature()const;

	private:
		static constexpr Uint32 atrousPassCount = 3;

		Handle<ComputeShader> computeShader_;
		Handle<Microsoft::WRL::ComPtr<ID3D12PipelineState>> pipelineStateObjectHandle_;

		Handle<ComputeShader> filterMomentsComputeShader_;
		Handle<Microsoft::WRL::ComPtr<ID3D12PipelineState>> filterMomentsPipelineStateObjectHandle_;

		Handle<ComputeShader> atrousComputeShader_[atrousPassCount];
		Handle<Microsoft::WRL::ComPtr<ID3D12PipelineState>> atrousPipelineStateObjectHandle_[atrousPassCount];

		Handle<RootSignature> denoiseRootSignature_;

		RootSignature& rootSignature_;
		PipelineStateObject& pipelineStateObject_;
	};
}
