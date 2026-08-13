#include <GraphicsEngine/PostProcess/PostEffect/Vignette.h>
#include <GraphicsEngine/Shader/ShaderCache.h>
#include <GraphicsEngine/D3D12/PipelineState/ComputeShader.h>

namespace SeedCore
{
	Vignette::Vignette(RootSignature& rootSignature, PipelineStateObject& pipelineStateObject) : rootSignature_(rootSignature), pipelineStateObject_(pipelineStateObject)
	{
		/// No Code
	}

	void Vignette::Create(ShaderCache& shaderCache, ID3D12Device* device)
	{
		vignetteRootSignature_ = rootSignature_.GetOrCreate(device);

		computeShader_ = shaderCache.GetOrCreateComputeShader(String("../GraphicsEngine/PostProcess/VignetteCS.hlsl"));

		PipelineStateKey key{};
		memset(&key, 0, sizeof(key));
		key.rootSignature_ = rootSignature_.Get(vignetteRootSignature_)->Get();
		key.computeShader_ = shaderCache.GetComputeShader(computeShader_)->Bytecode();
		pipelineStateHandle_ = pipelineStateObject_.GetOrCreate(device, key);
	}

	ID3D12PipelineState* Vignette::GetPipelineState()const
	{
		return pipelineStateObject_.Get(pipelineStateHandle_);
	}

	ID3D12RootSignature* Vignette::GetRootSignature()const
	{
		return rootSignature_.Get(vignetteRootSignature_)->Get();
	}
}
