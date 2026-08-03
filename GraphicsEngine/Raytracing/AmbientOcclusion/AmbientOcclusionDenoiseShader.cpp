#include <GraphicsEngine/Raytracing/AmbientOcclusion/AmbientOcclusionDenoiseShader.h>
#include <GraphicsEngine/Shader/ShaderCache.h>
#include <GraphicsEngine/D3D12/PipelineState/ComputeShader.h>

namespace SeedCore
{
	AmbientOcclusionDenoiseShader::AmbientOcclusionDenoiseShader(RootSignature& rootSignature, PipelineStateObject& pipelineStateObject) : rootSignature_(rootSignature), pipelineStateObject_(pipelineStateObject)
	{
		/// No Code
	}

	void AmbientOcclusionDenoiseShader::Create(ShaderCache& shaderCache, ID3D12Device* device)
	{
		denoiseRootSignature_ = rootSignature_.GetOrCreate(device);

		computeShader_ = shaderCache.GetOrCreateComputeShader(String("../GraphicsEngine/Raytracing/AmbientOcclusion/AmbientOcclusionDenoiseCS.hlsl"));

		PipelineStateKey psokey{};
		memset(&psokey, 0, sizeof(psokey));
		psokey.rootSignature_ = rootSignature_.Get(denoiseRootSignature_)->Get();
		psokey.computeShader_ = shaderCache.GetComputeShader(computeShader_)->Bytecode();
		pipelineStateObjectHandle_ = pipelineStateObject_.GetOrCreate(device, psokey);
	}

	ID3D12PipelineState* AmbientOcclusionDenoiseShader::GetPipelineState()const
	{
		return pipelineStateObject_.Get(pipelineStateObjectHandle_);
	}

	ID3D12RootSignature* AmbientOcclusionDenoiseShader::GetRootSignature()const
	{
		return rootSignature_.Get(denoiseRootSignature_)->Get();
	}
}
