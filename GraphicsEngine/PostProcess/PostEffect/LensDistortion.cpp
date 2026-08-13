#include <GraphicsEngine/PostProcess/PostEffect/LensDistortion.h>
#include <GraphicsEngine/Shader/ShaderCache.h>
#include <GraphicsEngine/D3D12/PipelineState/ComputeShader.h>

namespace SeedCore
{
	LensDistortion::LensDistortion(RootSignature& rootSignature, PipelineStateObject& pipelineStateObject) : rootSignature_(rootSignature), pipelineStateObject_(pipelineStateObject)
	{
		/// No Code
	}

	void LensDistortion::Create(ShaderCache& shaderCache, ID3D12Device* device)
	{
		lensDistortionRootSignature_ = rootSignature_.GetOrCreate(device);

		computeShader_ = shaderCache.GetOrCreateComputeShader(String("../GraphicsEngine/PostProcess/LensDistortionCS.hlsl"));

		PipelineStateKey key{};
		memset(&key, 0, sizeof(key));
		key.rootSignature_ = rootSignature_.Get(lensDistortionRootSignature_)->Get();
		key.computeShader_ = shaderCache.GetComputeShader(computeShader_)->Bytecode();
		pipelineStateHandle_ = pipelineStateObject_.GetOrCreate(device, key);
	}

	ID3D12PipelineState* LensDistortion::GetPipelineState()const
	{
		return pipelineStateObject_.Get(pipelineStateHandle_);
	}

	ID3D12RootSignature* LensDistortion::GetRootSignature()const
	{
		return rootSignature_.Get(lensDistortionRootSignature_)->Get();
	}
}
