#include <GraphicsEngine/PostProcess/PostEffect/ChromaticAberration.h>
#include <GraphicsEngine/Shader/ShaderCache.h>
#include <GraphicsEngine/D3D12/PipelineState/ComputeShader.h>

namespace SeedCore
{
	ChromaticAberration::ChromaticAberration(RootSignature& rootSignature, PipelineStateObject& pipelineStateObject) : rootSignature_(rootSignature), pipelineStateObject_(pipelineStateObject)
	{
		/// No Code
	}

	void ChromaticAberration::Create(ShaderCache& shaderCache, ID3D12Device* device)
	{
		chromaticAberrationRootSignature_ = rootSignature_.GetOrCreate(device);

		computeShader_ = shaderCache.GetOrCreateComputeShader(String("../GraphicsEngine/PostProcess/ChromaticAberrationCS.hlsl"));

		PipelineStateKey key{};
		memset(&key, 0, sizeof(key));
		key.rootSignature_ = rootSignature_.Get(chromaticAberrationRootSignature_)->Get();
		key.computeShader_ = shaderCache.GetComputeShader(computeShader_)->Bytecode();
		pipelineStateHandle_ = pipelineStateObject_.GetOrCreate(device, key);
	}

	ID3D12PipelineState* ChromaticAberration::GetPipelineState()const
	{
		return pipelineStateObject_.Get(pipelineStateHandle_);
	}

	ID3D12RootSignature* ChromaticAberration::GetRootSignature()const
	{
		return rootSignature_.Get(chromaticAberrationRootSignature_)->Get();
	}
}
