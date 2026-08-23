#include <GraphicsEngine/Raytracing/GlobalIllumination/GlobalIlluminationDenoiseShader.h>
#include <GraphicsEngine/Shader/ShaderCache.h>
#include <GraphicsEngine/D3D12/PipelineState/ComputeShader.h>

namespace SeedCore
{
	namespace
	{
		constexpr const Char* atrousEntryPoints[3] = { "ATrousPass1", "ATrousPass2", "ATrousPass3" };
	}

	GlobalIlluminationDenoiseShader::GlobalIlluminationDenoiseShader(RootSignature& rootSignature, PipelineStateObject& pipelineStateObject) : rootSignature_(rootSignature), pipelineStateObject_(pipelineStateObject)
	{
		/// No Code
	}

	void GlobalIlluminationDenoiseShader::Create(ShaderCache& shaderCache, ID3D12Device* device)
	{
		denoiseRootSignature_ = rootSignature_.GetOrCreate(device);

		const String filePath = String("../GraphicsEngine/Raytracing/GlobalIllumination/GlobalIlluminationDenoiseCS.hlsl");

		computeShader_ = shaderCache.GetOrCreateComputeShader(filePath);

		PipelineStateKey psokey{};
		memset(&psokey, 0, sizeof(psokey));
		psokey.rootSignature_ = rootSignature_.Get(denoiseRootSignature_)->Get();
		psokey.computeShader_ = shaderCache.GetComputeShader(computeShader_)->Bytecode();
		pipelineStateObjectHandle_ = pipelineStateObject_.GetOrCreate(device, psokey);

		for (Uint32 pass = 0; pass < atrousPassCount; pass++)
		{
			atrousComputeShader_[pass] = shaderCache.GetOrCreateComputeShader(filePath, String(atrousEntryPoints[pass]));

			PipelineStateKey atrousKey{};
			memset(&atrousKey, 0, sizeof(atrousKey));
			atrousKey.rootSignature_ = rootSignature_.Get(denoiseRootSignature_)->Get();
			atrousKey.computeShader_ = shaderCache.GetComputeShader(atrousComputeShader_[pass])->Bytecode();
			atrousPipelineStateObjectHandle_[pass] = pipelineStateObject_.GetOrCreate(device, atrousKey);
		}

		const String spatialReuseFilePath = String("../GraphicsEngine/Raytracing/GlobalIllumination/GlobalIlluminationReservoirSpatialCS.hlsl");

		spatialReuseComputeShader_ = shaderCache.GetOrCreateComputeShader(spatialReuseFilePath);

		PipelineStateKey spatialReuseKey{};
		memset(&spatialReuseKey, 0, sizeof(spatialReuseKey));
		spatialReuseKey.rootSignature_ = rootSignature_.Get(denoiseRootSignature_)->Get();
		spatialReuseKey.computeShader_ = shaderCache.GetComputeShader(spatialReuseComputeShader_)->Bytecode();
		spatialReusePipelineStateObjectHandle_ = pipelineStateObject_.GetOrCreate(device, spatialReuseKey);
	}

	ID3D12PipelineState* GlobalIlluminationDenoiseShader::GetPipelineState()const
	{
		return pipelineStateObject_.Get(pipelineStateObjectHandle_);
	}

	ID3D12PipelineState* GlobalIlluminationDenoiseShader::GetATrousPipelineState(Uint32 passIndex)const
	{
		return pipelineStateObject_.Get(atrousPipelineStateObjectHandle_[passIndex]);
	}

	ID3D12PipelineState* GlobalIlluminationDenoiseShader::GetSpatialReusePipelineState()const
	{
		return pipelineStateObject_.Get(spatialReusePipelineStateObjectHandle_);
	}

	ID3D12RootSignature* GlobalIlluminationDenoiseShader::GetRootSignature()const
	{
		return rootSignature_.Get(denoiseRootSignature_)->Get();
	}
}
