#include <GraphicsEngine/PostProcess/PostEffect/KawaseBloom.h>
#include <GraphicsEngine/Shader/ShaderCache.h>
#include <GraphicsEngine/D3D12/PipelineState/ComputeShader.h>

namespace SeedCore
{
	namespace
	{
		constexpr const Char* downsampleEntryPoints[5] = { "Downsample1", "Downsample2", "Downsample3", "Downsample4", "Downsample5" };
		constexpr const Char* upsampleEntryPoints[5] = { "Upsample0", "Upsample1", "Upsample2", "Upsample3", "Upsample4" };
	}

	KawaseBloom::KawaseBloom(RootSignature& rootSignature, PipelineStateObject& pipelineStateObject) : rootSignature_(rootSignature), pipelineStateObject_(pipelineStateObject)
	{
		/// No Code
	}

	void KawaseBloom::Create(ShaderCache& shaderCache, ID3D12Device* device)
	{
		bloomRootSignature_ = rootSignature_.GetOrCreate(device);

		const String filePath = String("../GraphicsEngine/PostProcess/KawaseBloomCS.hlsl");

		prefilterComputeShader_ = shaderCache.GetOrCreateComputeShader(filePath, String("DownsamplePrefilter"));

		PipelineStateKey prefilterKey{};
		memset(&prefilterKey, 0, sizeof(prefilterKey));
		prefilterKey.rootSignature_ = rootSignature_.Get(bloomRootSignature_)->Get();
		prefilterKey.computeShader_ = shaderCache.GetComputeShader(prefilterComputeShader_)->Bytecode();
		prefilterPipelineStateHandle_ = pipelineStateObject_.GetOrCreate(device, prefilterKey);

		for (Uint32 levelIndex = 0; levelIndex < chainPassCount; levelIndex++)
		{
			downsampleComputeShader_[levelIndex] = shaderCache.GetOrCreateComputeShader(filePath, String(downsampleEntryPoints[levelIndex]));

			PipelineStateKey downsampleKey{};
			memset(&downsampleKey, 0, sizeof(downsampleKey));
			downsampleKey.rootSignature_ = rootSignature_.Get(bloomRootSignature_)->Get();
			downsampleKey.computeShader_ = shaderCache.GetComputeShader(downsampleComputeShader_[levelIndex])->Bytecode();
			downsamplePipelineStateHandle_[levelIndex] = pipelineStateObject_.GetOrCreate(device, downsampleKey);

			upsampleComputeShader_[levelIndex] = shaderCache.GetOrCreateComputeShader(filePath, String(upsampleEntryPoints[levelIndex]));

			PipelineStateKey upsampleKey{};
			memset(&upsampleKey, 0, sizeof(upsampleKey));
			upsampleKey.rootSignature_ = rootSignature_.Get(bloomRootSignature_)->Get();
			upsampleKey.computeShader_ = shaderCache.GetComputeShader(upsampleComputeShader_[levelIndex])->Bytecode();
			upsamplePipelineStateHandle_[levelIndex] = pipelineStateObject_.GetOrCreate(device, upsampleKey);
		}
	}

	ID3D12PipelineState* KawaseBloom::GetPrefilterPipelineState()const
	{
		return pipelineStateObject_.Get(prefilterPipelineStateHandle_);
	}

	ID3D12PipelineState* KawaseBloom::GetDownsamplePipelineState(Uint32 levelIndex)const
	{
		return pipelineStateObject_.Get(downsamplePipelineStateHandle_[levelIndex]);
	}

	ID3D12PipelineState* KawaseBloom::GetUpsamplePipelineState(Uint32 levelIndex)const
	{
		return pipelineStateObject_.Get(upsamplePipelineStateHandle_[levelIndex]);
	}

	ID3D12RootSignature* KawaseBloom::GetRootSignature()const
	{
		return rootSignature_.Get(bloomRootSignature_)->Get();
	}
}
