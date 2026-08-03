#include <GraphicsEngine/Raytracing/SubsurfaceScattering/SubsurfaceScatteringShader.h>
#include <GraphicsEngine/Shader/ShaderCache.h>
#include <GraphicsEngine/D3D12/PipelineState/ComputeShader.h>

namespace SeedCore
{
	SubsurfaceScatteringShader::SubsurfaceScatteringShader(RootSignature& rootSignature, PipelineStateObject& pipelineStateObject) : rootSignature_(rootSignature), pipelineStateObject_(pipelineStateObject)
	{
		/// No Code
	}

	void SubsurfaceScatteringShader::Create(ShaderCache& shaderCache, ID3D12Device* device)
	{
		subsurfaceScatteringRootSignature_ = rootSignature_.GetOrCreate(device);

		computeShader_ = shaderCache.GetOrCreateComputeShader(String("../GraphicsEngine/Raytracing/SubsurfaceScattering/SubsurfaceScatteringRT.hlsl"));

		PipelineStateKey psokey{};
		memset(&psokey, 0, sizeof(psokey));
		psokey.rootSignature_ = rootSignature_.Get(subsurfaceScatteringRootSignature_)->Get();
		psokey.computeShader_ = shaderCache.GetComputeShader(computeShader_)->Bytecode();
		pipelineStateObjectHandle_ = pipelineStateObject_.GetOrCreate(device, psokey);
	}

	ID3D12PipelineState* SubsurfaceScatteringShader::GetPipelineState()const
	{
		return pipelineStateObject_.Get(pipelineStateObjectHandle_);
	}

	ID3D12RootSignature* SubsurfaceScatteringShader::GetRootSignature()const
	{
		return rootSignature_.Get(subsurfaceScatteringRootSignature_)->Get();
	}
}
