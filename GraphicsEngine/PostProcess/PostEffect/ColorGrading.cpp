#include <GraphicsEngine/PostProcess/PostEffect/ColorGrading.h>
#include <GraphicsEngine/Shader/ShaderCache.h>
#include <GraphicsEngine/D3D12/PipelineState/ComputeShader.h>

namespace SeedCore
{
	ColorGrading::ColorGrading(RootSignature& rootSignature, PipelineStateObject& pipelineStateObject) : rootSignature_(rootSignature), pipelineStateObject_(pipelineStateObject)
	{
		/// No Code
	}

	void ColorGrading::Create(ShaderCache& shaderCache, ID3D12Device* device)
	{
		colorGradingRootSignature_ = rootSignature_.GetOrCreate(device);

		computeShader_ = shaderCache.GetOrCreateComputeShader(String("../GraphicsEngine/PostProcess/ColorGradingCS.hlsl"));

		PipelineStateKey key{};
		memset(&key, 0, sizeof(key));
		key.rootSignature_ = rootSignature_.Get(colorGradingRootSignature_)->Get();
		key.computeShader_ = shaderCache.GetComputeShader(computeShader_)->Bytecode();
		pipelineStateHandle_ = pipelineStateObject_.GetOrCreate(device, key);
	}

	ID3D12PipelineState* ColorGrading::GetPipelineState()const
	{
		return pipelineStateObject_.Get(pipelineStateHandle_);
	}

	ID3D12RootSignature* ColorGrading::GetRootSignature()const
	{
		return rootSignature_.Get(colorGradingRootSignature_)->Get();
	}
}
