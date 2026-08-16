#include <GraphicsEngine/Model/Material/MaterialResolveShader.h>
#include <GraphicsEngine/Shader/ShaderCache.h>
#include <GraphicsEngine/D3D12/PipelineState/ComputeShader.h>

namespace SeedCore
{
	MaterialResolveShader::MaterialResolveShader(RootSignature& rootSignature, PipelineStateObject& pipelineStateObject) : rootSignature_(rootSignature), pipelineStateObject_(pipelineStateObject)
	{
		/// No Code
	}

	void MaterialResolveShader::Create(ShaderCache& shaderCache, ID3D12Device* device)
	{
		materialResolveRootSignature_ = rootSignature_.GetOrCreate(device);

		PipelineStateKey psokey{};

		classifyComputeShader_ = shaderCache.GetOrCreateComputeShader(String("../GraphicsEngine/Model/Material/MaterialClassifyCS.hlsl"));
		memset(&psokey, 0, sizeof(psokey));
		psokey.rootSignature_ = rootSignature_.Get(materialResolveRootSignature_)->Get();
		psokey.computeShader_ = shaderCache.GetComputeShader(classifyComputeShader_)->Bytecode();
		classifyPipelineStateObjectHandle_ = pipelineStateObject_.GetOrCreate(device, psokey);

		prefixSumComputeShader_ = shaderCache.GetOrCreateComputeShader(String("../GraphicsEngine/Model/Material/MaterialPrefixSumCS.hlsl"));
		memset(&psokey, 0, sizeof(psokey));
		psokey.rootSignature_ = rootSignature_.Get(materialResolveRootSignature_)->Get();
		psokey.computeShader_ = shaderCache.GetComputeShader(prefixSumComputeShader_)->Bytecode();
		prefixSumPipelineStateObjectHandle_ = pipelineStateObject_.GetOrCreate(device, psokey);

		scatterComputeShader_ = shaderCache.GetOrCreateComputeShader(String("../GraphicsEngine/Model/Material/MaterialScatterCS.hlsl"));
		memset(&psokey, 0, sizeof(psokey));
		psokey.rootSignature_ = rootSignature_.Get(materialResolveRootSignature_)->Get();
		psokey.computeShader_ = shaderCache.GetComputeShader(scatterComputeShader_)->Bytecode();
		scatterPipelineStateObjectHandle_ = pipelineStateObject_.GetOrCreate(device, psokey);

		computeShader_ = shaderCache.GetOrCreateComputeShader(String("../GraphicsEngine/Model/Material/MaterialResolveCS.hlsl"));
		memset(&psokey, 0, sizeof(psokey));
		psokey.rootSignature_ = rootSignature_.Get(materialResolveRootSignature_)->Get();
		psokey.computeShader_ = shaderCache.GetComputeShader(computeShader_)->Bytecode();
		pipelineStateObjectHandle_ = pipelineStateObject_.GetOrCreate(device, psokey);
	}

	ID3D12PipelineState* MaterialResolveShader::GetPipelineState()const
	{
		return pipelineStateObject_.Get(pipelineStateObjectHandle_);
	}

	ID3D12PipelineState* MaterialResolveShader::GetClassifyPipelineState()const
	{
		return pipelineStateObject_.Get(classifyPipelineStateObjectHandle_);
	}

	ID3D12PipelineState* MaterialResolveShader::GetPrefixSumPipelineState()const
	{
		return pipelineStateObject_.Get(prefixSumPipelineStateObjectHandle_);
	}

	ID3D12PipelineState* MaterialResolveShader::GetScatterPipelineState()const
	{
		return pipelineStateObject_.Get(scatterPipelineStateObjectHandle_);
	}

	ID3D12RootSignature* MaterialResolveShader::GetRootSignature()const
	{
		return rootSignature_.Get(materialResolveRootSignature_)->Get();
	}
}
