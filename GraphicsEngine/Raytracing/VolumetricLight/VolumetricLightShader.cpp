#include <GraphicsEngine/Raytracing/VolumetricLight/VolumetricLightShader.h>
#include <GraphicsEngine/Shader/ShaderCache.h>
#include <GraphicsEngine/D3D12/PipelineState/ComputeShader.h>

namespace SeedCore
{
	VolumetricLightShader::VolumetricLightShader(RootSignature& rootSignature, PipelineStateObject& pipelineStateObject) : rootSignature_(rootSignature), pipelineStateObject_(pipelineStateObject)
	{
		/// No Code
	}

	void VolumetricLightShader::Create(ShaderCache& shaderCache, ID3D12Device* device)
	{
		volumetricLightRootSignature_ = rootSignature_.GetOrCreate(device);
		ID3D12RootSignature* signature = rootSignature_.Get(volumetricLightRootSignature_)->Get();

		injectionShader_ = shaderCache.GetOrCreateComputeShader(String("../GraphicsEngine/Raytracing/Fog/FogInjectionCS.hlsl"));

		PipelineStateKey injectionKey{};
		memset(&injectionKey, 0, sizeof(injectionKey));
		injectionKey.rootSignature_ = signature;
		injectionKey.computeShader_ = shaderCache.GetComputeShader(injectionShader_)->Bytecode();
		injectionPipelineStateHandle_ = pipelineStateObject_.GetOrCreate(device, injectionKey);

		scatteringShader_ = shaderCache.GetOrCreateComputeShader(String("../GraphicsEngine/Raytracing/VolumetricLight/VolumetricLightScatteringRT.hlsl"));

		PipelineStateKey scatteringKey{};
		memset(&scatteringKey, 0, sizeof(scatteringKey));
		scatteringKey.rootSignature_ = signature;
		scatteringKey.computeShader_ = shaderCache.GetComputeShader(scatteringShader_)->Bytecode();
		scatteringPipelineStateHandle_ = pipelineStateObject_.GetOrCreate(device, scatteringKey);

		integrationShader_ = shaderCache.GetOrCreateComputeShader(String("../GraphicsEngine/Raytracing/Froxel/FroxelIntegrationCS.hlsl"));

		PipelineStateKey integrationKey{};
		memset(&integrationKey, 0, sizeof(integrationKey));
		integrationKey.rootSignature_ = signature;
		integrationKey.computeShader_ = shaderCache.GetComputeShader(integrationShader_)->Bytecode();
		integrationPipelineStateHandle_ = pipelineStateObject_.GetOrCreate(device, integrationKey);
	}

	ID3D12PipelineState* VolumetricLightShader::GetInjectionPipelineState()const
	{
		return pipelineStateObject_.Get(injectionPipelineStateHandle_);
	}

	ID3D12PipelineState* VolumetricLightShader::GetScatteringPipelineState()const
	{
		return pipelineStateObject_.Get(scatteringPipelineStateHandle_);
	}

	ID3D12PipelineState* VolumetricLightShader::GetIntegrationPipelineState()const
	{
		return pipelineStateObject_.Get(integrationPipelineStateHandle_);
	}

	ID3D12RootSignature* VolumetricLightShader::GetRootSignature()const
	{
		return rootSignature_.Get(volumetricLightRootSignature_)->Get();
	}
}
