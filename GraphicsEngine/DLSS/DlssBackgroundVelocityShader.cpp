#include <GraphicsEngine/DLSS/DlssBackgroundVelocityShader.h>
#include <GraphicsEngine/Shader/ShaderCache.h>
#include <GraphicsEngine/D3D12/PipelineState/ComputeShader.h>

namespace SeedCore
{
	DlssBackgroundVelocityShader::DlssBackgroundVelocityShader(RootSignature& rootSignature, PipelineStateObject& pipelineStateObject) : rootSignature_(rootSignature), pipelineStateObject_(pipelineStateObject)
	{
		/// No Code
	}

	void DlssBackgroundVelocityShader::Create(ShaderCache& shaderCache, ID3D12Device* device)
	{
		backgroundVelocityRootSignature_ = rootSignature_.GetOrCreate(device);

		computeShader_ = shaderCache.GetOrCreateComputeShader(String("../GraphicsEngine/DLSS/DlssBackgroundVelocityCS.hlsl"));

		PipelineStateKey psokey{};
		memset(&psokey, 0, sizeof(psokey));
		psokey.rootSignature_ = rootSignature_.Get(backgroundVelocityRootSignature_)->Get();
		psokey.computeShader_ = shaderCache.GetComputeShader(computeShader_)->Bytecode();
		pipelineStateObjectHandle_ = pipelineStateObject_.GetOrCreate(device, psokey);
	}

	ID3D12PipelineState* DlssBackgroundVelocityShader::GetPipelineState()const
	{
		return pipelineStateObject_.Get(pipelineStateObjectHandle_);
	}

	ID3D12RootSignature* DlssBackgroundVelocityShader::GetRootSignature()const
	{
		return rootSignature_.Get(backgroundVelocityRootSignature_)->Get();
	}
}
