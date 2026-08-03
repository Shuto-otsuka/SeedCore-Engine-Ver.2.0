#include <GraphicsEngine/Raytracing/AmbientOcclusion/AmbientOcclusionShader.h>
#include <GraphicsEngine/Shader/ShaderCache.h>
#include <GraphicsEngine/D3D12/PipelineState/ComputeShader.h>

namespace SeedCore
{
	AmbientOcclusionShader::AmbientOcclusionShader(RootSignature& rootSignature, PipelineStateObject& pipelineStateObject) : rootSignature_(rootSignature), pipelineStateObject_(pipelineStateObject)
	{
		/// No Code
	}

	void AmbientOcclusionShader::Create(ShaderCache& shaderCache, ID3D12Device* device)
	{
		ambientOcclusionRootSignature_ = rootSignature_.GetOrCreate(device);

		computeShader_ = shaderCache.GetOrCreateComputeShader(String("../GraphicsEngine/Raytracing/AmbientOcclusion/AmbientOcclusionRT.hlsl"));

		PipelineStateKey psokey{};
		memset(&psokey, 0, sizeof(psokey));
		psokey.rootSignature_ = rootSignature_.Get(ambientOcclusionRootSignature_)->Get();
		psokey.computeShader_ = shaderCache.GetComputeShader(computeShader_)->Bytecode();
		pipelineStateObjectHandle_ = pipelineStateObject_.GetOrCreate(device, psokey);
	}

	ID3D12PipelineState* AmbientOcclusionShader::GetPipelineState()const
	{
		return pipelineStateObject_.Get(pipelineStateObjectHandle_);
	}

	ID3D12RootSignature* AmbientOcclusionShader::GetRootSignature()const
	{
		return rootSignature_.Get(ambientOcclusionRootSignature_)->Get();
	}
}
