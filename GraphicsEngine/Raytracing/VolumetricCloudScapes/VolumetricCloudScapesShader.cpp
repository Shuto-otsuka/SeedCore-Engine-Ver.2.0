#include <GraphicsEngine/Raytracing/VolumetricCloudScapes/VolumetricCloudScapesShader.h>
#include <GraphicsEngine/Shader/ShaderCache.h>
#include <GraphicsEngine/D3D12/PipelineState/ComputeShader.h>

namespace SeedCore
{
	VolumetricCloudScapesShader::VolumetricCloudScapesShader(RootSignature& rootSignature, PipelineStateObject& pipelineStateObject) : rootSignature_(rootSignature), pipelineStateObject_(pipelineStateObject)
	{
		/// No Code
	}

	void VolumetricCloudScapesShader::Create(ShaderCache& shaderCache, ID3D12Device* device)
	{
		cloudRootSignature_ = rootSignature_.GetOrCreate(device);
		ID3D12RootSignature* signature = rootSignature_.Get(cloudRootSignature_)->Get();

		computeShader_ = shaderCache.GetOrCreateComputeShader(String("../GraphicsEngine/Raytracing/VolumetricCloudScapes/VolumetricCloudScapesRT.hlsl"));

		PipelineStateKey psokey{};
		memset(&psokey, 0, sizeof(psokey));
		psokey.rootSignature_ = signature;
		psokey.computeShader_ = shaderCache.GetComputeShader(computeShader_)->Bytecode();
		pipelineStateObjectHandle_ = pipelineStateObject_.GetOrCreate(device, psokey);

		shapeBakeShader_ = shaderCache.GetOrCreateComputeShader(String("../GraphicsEngine/Raytracing/VolumetricCloudScapes/CloudNoiseShapeBakeCS.hlsl"));

		PipelineStateKey shapeKey{};
		memset(&shapeKey, 0, sizeof(shapeKey));
		shapeKey.rootSignature_ = signature;
		shapeKey.computeShader_ = shaderCache.GetComputeShader(shapeBakeShader_)->Bytecode();
		shapeBakePipelineStateHandle_ = pipelineStateObject_.GetOrCreate(device, shapeKey);

		detailBakeShader_ = shaderCache.GetOrCreateComputeShader(String("../GraphicsEngine/Raytracing/VolumetricCloudScapes/CloudNoiseDetailBakeCS.hlsl"));

		PipelineStateKey detailKey{};
		memset(&detailKey, 0, sizeof(detailKey));
		detailKey.rootSignature_ = signature;
		detailKey.computeShader_ = shaderCache.GetComputeShader(detailBakeShader_)->Bytecode();
		detailBakePipelineStateHandle_ = pipelineStateObject_.GetOrCreate(device, detailKey);
	}

	ID3D12PipelineState* VolumetricCloudScapesShader::GetPipelineState()const
	{
		return pipelineStateObject_.Get(pipelineStateObjectHandle_);
	}

	ID3D12PipelineState* VolumetricCloudScapesShader::GetShapeBakePipelineState()const
	{
		return pipelineStateObject_.Get(shapeBakePipelineStateHandle_);
	}

	ID3D12PipelineState* VolumetricCloudScapesShader::GetDetailBakePipelineState()const
	{
		return pipelineStateObject_.Get(detailBakePipelineStateHandle_);
	}

	ID3D12RootSignature* VolumetricCloudScapesShader::GetRootSignature()const
	{
		return rootSignature_.Get(cloudRootSignature_)->Get();
	}
}
