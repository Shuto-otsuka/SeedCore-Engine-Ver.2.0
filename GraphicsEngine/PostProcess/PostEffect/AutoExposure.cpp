#include <GraphicsEngine/PostProcess/PostEffect/AutoExposure.h>
#include <GraphicsEngine/Shader/ShaderCache.h>
#include <GraphicsEngine/D3D12/PipelineState/ComputeShader.h>

namespace SeedCore
{
	AutoExposure::AutoExposure(RootSignature& rootSignature, PipelineStateObject& pipelineStateObject) : rootSignature_(rootSignature), pipelineStateObject_(pipelineStateObject)
	{
		/// No Code
	}

	void AutoExposure::Create(ShaderCache& shaderCache, ID3D12Device* device)
	{
		autoExposureRootSignature_ = rootSignature_.GetOrCreate(device);
		ID3D12RootSignature* signature = rootSignature_.Get(autoExposureRootSignature_)->Get();

		histogramShader_ = shaderCache.GetOrCreateComputeShader(String("../GraphicsEngine/PostProcess/AutoExposureHistogramCS.hlsl"));

		PipelineStateKey histogramKey{};
		memset(&histogramKey, 0, sizeof(histogramKey));
		histogramKey.rootSignature_ = signature;
		histogramKey.computeShader_ = shaderCache.GetComputeShader(histogramShader_)->Bytecode();
		histogramPipelineStateHandle_ = pipelineStateObject_.GetOrCreate(device, histogramKey);

		averageShader_ = shaderCache.GetOrCreateComputeShader(String("../GraphicsEngine/PostProcess/AutoExposureAverageCS.hlsl"));

		PipelineStateKey averageKey{};
		memset(&averageKey, 0, sizeof(averageKey));
		averageKey.rootSignature_ = signature;
		averageKey.computeShader_ = shaderCache.GetComputeShader(averageShader_)->Bytecode();
		averagePipelineStateHandle_ = pipelineStateObject_.GetOrCreate(device, averageKey);
	}

	ID3D12PipelineState* AutoExposure::GetHistogramPipelineState()const
	{
		return pipelineStateObject_.Get(histogramPipelineStateHandle_);
	}

	ID3D12PipelineState* AutoExposure::GetAveragePipelineState()const
	{
		return pipelineStateObject_.Get(averagePipelineStateHandle_);
	}

	ID3D12RootSignature* AutoExposure::GetRootSignature()const
	{
		return rootSignature_.Get(autoExposureRootSignature_)->Get();
	}
}
