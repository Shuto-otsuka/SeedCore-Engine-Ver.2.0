#include <GraphicsEngine/Raytracing/Reflection/ReflectionDenoiseShader.h>
#include <GraphicsEngine/Shader/ShaderCache.h>
#include <GraphicsEngine/D3D12/PipelineState/ComputeShader.h>

namespace SeedCore
{
	namespace
	{
		constexpr const Char* atrousEntryPoints[3] = { "ATrousPass1", "ATrousPass2", "ATrousPass3" };
	}

	ReflectionDenoiseShader::ReflectionDenoiseShader(RootSignature& rootSignature, PipelineStateObject& pipelineStateObject) : rootSignature_(rootSignature), pipelineStateObject_(pipelineStateObject)
	{
		/// No Code
	}

	void ReflectionDenoiseShader::Create(ShaderCache& shaderCache, ID3D12Device* device)
	{
		denoiseRootSignature_ = rootSignature_.GetOrCreate(device);

		const String filePath = String("../GraphicsEngine/Raytracing/Reflection/ReflectionDenoiseCS.hlsl");

		computeShader_ = shaderCache.GetOrCreateComputeShader(filePath);

		PipelineStateKey psokey{};
		memset(&psokey, 0, sizeof(psokey));
		psokey.rootSignature_ = rootSignature_.Get(denoiseRootSignature_)->Get();
		psokey.computeShader_ = shaderCache.GetComputeShader(computeShader_)->Bytecode();
		pipelineStateObjectHandle_ = pipelineStateObject_.GetOrCreate(device, psokey);

		filterMomentsComputeShader_ = shaderCache.GetOrCreateComputeShader(filePath, String("FilterMoments"));

		PipelineStateKey filterMomentsKey{};
		memset(&filterMomentsKey, 0, sizeof(filterMomentsKey));
		filterMomentsKey.rootSignature_ = rootSignature_.Get(denoiseRootSignature_)->Get();
		filterMomentsKey.computeShader_ = shaderCache.GetComputeShader(filterMomentsComputeShader_)->Bytecode();
		filterMomentsPipelineStateObjectHandle_ = pipelineStateObject_.GetOrCreate(device, filterMomentsKey);

		for (Uint32 pass = 0; pass < atrousPassCount; pass++)
		{
			atrousComputeShader_[pass] = shaderCache.GetOrCreateComputeShader(filePath, String(atrousEntryPoints[pass]));

			PipelineStateKey atrousKey{};
			memset(&atrousKey, 0, sizeof(atrousKey));
			atrousKey.rootSignature_ = rootSignature_.Get(denoiseRootSignature_)->Get();
			atrousKey.computeShader_ = shaderCache.GetComputeShader(atrousComputeShader_[pass])->Bytecode();
			atrousPipelineStateObjectHandle_[pass] = pipelineStateObject_.GetOrCreate(device, atrousKey);
		}
	}

	ID3D12PipelineState* ReflectionDenoiseShader::GetPipelineState()const
	{
		return pipelineStateObject_.Get(pipelineStateObjectHandle_);
	}

	ID3D12PipelineState* ReflectionDenoiseShader::GetFilterMomentsPipelineState()const
	{
		return pipelineStateObject_.Get(filterMomentsPipelineStateObjectHandle_);
	}

	ID3D12PipelineState* ReflectionDenoiseShader::GetATrousPipelineState(Uint32 passIndex)const
	{
		return pipelineStateObject_.Get(atrousPipelineStateObjectHandle_[passIndex]);
	}

	ID3D12RootSignature* ReflectionDenoiseShader::GetRootSignature()const
	{
		return rootSignature_.Get(denoiseRootSignature_)->Get();
	}
}
