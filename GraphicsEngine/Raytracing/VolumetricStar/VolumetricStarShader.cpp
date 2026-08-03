#include <GraphicsEngine/Raytracing/VolumetricStar/VolumetricStarShader.h>
#include <GraphicsEngine/Shader/ShaderCache.h>
#include <GraphicsEngine/D3D12/PipelineState/ComputeShader.h>

namespace SeedCore
{
	VolumetricStarShader::VolumetricStarShader(RootSignature& rootSignature, PipelineStateObject& pipelineStateObject) : rootSignature_(rootSignature), pipelineStateObject_(pipelineStateObject)
	{
		/// No Code
	}

	void VolumetricStarShader::Create(ShaderCache& shaderCache, ID3D12Device* device)
	{
		starRootSignature_ = rootSignature_.GetOrCreate(device);
		ID3D12RootSignature* signature = rootSignature_.Get(starRootSignature_)->Get();

		computeShader_ = shaderCache.GetOrCreateComputeShader(String("../GraphicsEngine/Raytracing/VolumetricStar/VolumetricStarRT.hlsl"));

		PipelineStateKey psokey{};
		memset(&psokey, 0, sizeof(psokey));
		psokey.rootSignature_ = signature;
		psokey.computeShader_ = shaderCache.GetComputeShader(computeShader_)->Bytecode();
		pipelineStateObjectHandle_ = pipelineStateObject_.GetOrCreate(device, psokey);
	}

	ID3D12PipelineState* VolumetricStarShader::GetPipelineState()const
	{
		return pipelineStateObject_.Get(pipelineStateObjectHandle_);
	}

	ID3D12RootSignature* VolumetricStarShader::GetRootSignature()const
	{
		return rootSignature_.Get(starRootSignature_)->Get();
	}
}
