#include <GraphicsEngine/PostProcess/PostEffect/Sharpness.h>
#include <GraphicsEngine/Shader/ShaderCache.h>
#include <GraphicsEngine/D3D12/PipelineState/ComputeShader.h>

namespace SeedCore
{
	Sharpness::Sharpness(RootSignature& rootSignature, PipelineStateObject& pipelineStateObject) : rootSignature_(rootSignature), pipelineStateObject_(pipelineStateObject)
	{
		/// No Code
	}

	void Sharpness::Create(ShaderCache& shaderCache, ID3D12Device* device)
	{
		sharpnessRootSignature_ = rootSignature_.GetOrCreate(device);

		computeShader_ = shaderCache.GetOrCreateComputeShader(String("../GraphicsEngine/PostProcess/SharpnessCS.hlsl"));

		PipelineStateKey key{};
		memset(&key, 0, sizeof(key));
		key.rootSignature_ = rootSignature_.Get(sharpnessRootSignature_)->Get();
		key.computeShader_ = shaderCache.GetComputeShader(computeShader_)->Bytecode();
		pipelineStateHandle_ = pipelineStateObject_.GetOrCreate(device, key);
	}

	ID3D12PipelineState* Sharpness::GetPipelineState()const
	{
		return pipelineStateObject_.Get(pipelineStateHandle_);
	}

	ID3D12RootSignature* Sharpness::GetRootSignature()const
	{
		return rootSignature_.Get(sharpnessRootSignature_)->Get();
	}
}
