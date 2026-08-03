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
	* Manages PSO creation for the auto-exposure compute chain: a histogram
	* build pass (AutoExposureHistogramCS.hlsl) and a single-thread
	* reduce-and-adapt pass (AutoExposureAverageCS.hlsl). Same shape as
	* VolumetricCloudScapesShader (multiple compute PSOs sharing the engine's
	* common root signature) — no RTPSO involved, this is screen-space only.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 自動露出コンピュートチェーンの PSO 管理: ヒストグラム構築パス
	* (AutoExposureHistogramCS.hlsl)と、単一スレッドで縮約+順応を行うパス
	* (AutoExposureAverageCS.hlsl)。VolumetricCloudScapesShader と同じ構成
	* (複数のコンピュート PSO がエンジン共通のルートシグネチャを共有)。
	* RTPSO は使わない — スクリーン空間のみで完結する。
	*/
	class AutoExposure
	{
	public:
		AutoExposure(RootSignature& rootSignature, PipelineStateObject& pipelineStateObject);
		~AutoExposure() = default;

		void Create(ShaderCache& shaderCache, ID3D12Device* device);

		[[nodiscard]] ID3D12PipelineState* GetHistogramPipelineState()const;

		[[nodiscard]] ID3D12PipelineState* GetAveragePipelineState()const;

		[[nodiscard]] ID3D12RootSignature* GetRootSignature()const;

	private:
		Handle<ComputeShader> histogramShader_;
		Handle<Microsoft::WRL::ComPtr<ID3D12PipelineState>> histogramPipelineStateHandle_;

		Handle<ComputeShader> averageShader_;
		Handle<Microsoft::WRL::ComPtr<ID3D12PipelineState>> averagePipelineStateHandle_;

		Handle<RootSignature> autoExposureRootSignature_;

		RootSignature& rootSignature_;
		PipelineStateObject& pipelineStateObject_;
	};
}
