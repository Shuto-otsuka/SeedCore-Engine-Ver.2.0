#include <GraphicsEngine/PostProcess/PostEffect/AnamorphicFlare.h>
#include <GraphicsEngine/Shader/ShaderCache.h>
#include <GraphicsEngine/D3D12/PipelineState/ComputeShader.h>

namespace SeedCore
{
	namespace
	{
		constexpr const Char* blurEntryPoints[4] = { "BlurPass1", "BlurPass2", "BlurPass3", "BlurPass4" };
	}

	AnamorphicFlare::AnamorphicFlare(RootSignature& rootSignature, PipelineStateObject& pipelineStateObject) : rootSignature_(rootSignature), pipelineStateObject_(pipelineStateObject)
	{
		/// No Code
	}

	void AnamorphicFlare::Create(ShaderCache& shaderCache, ID3D12Device* device)
	{
		anamorphicFlareRootSignature_ = rootSignature_.GetOrCreate(device);

		const String filePath = String("../GraphicsEngine/PostProcess/AnamorphicFlareCS.hlsl");

		prefilterComputeShader_ = shaderCache.GetOrCreateComputeShader(filePath, String("Prefilter"));

		PipelineStateKey prefilterKey{};
		memset(&prefilterKey, 0, sizeof(prefilterKey));
		prefilterKey.rootSignature_ = rootSignature_.Get(anamorphicFlareRootSignature_)->Get();
		prefilterKey.computeShader_ = shaderCache.GetComputeShader(prefilterComputeShader_)->Bytecode();
		prefilterPipelineStateHandle_ = pipelineStateObject_.GetOrCreate(device, prefilterKey);

		for (Uint32 pass = 0; pass < blurPassCount; pass++)
		{
			blurComputeShader_[pass] = shaderCache.GetOrCreateComputeShader(filePath, String(blurEntryPoints[pass]));

			PipelineStateKey blurKey{};
			memset(&blurKey, 0, sizeof(blurKey));
			blurKey.rootSignature_ = rootSignature_.Get(anamorphicFlareRootSignature_)->Get();
			blurKey.computeShader_ = shaderCache.GetComputeShader(blurComputeShader_[pass])->Bytecode();
			blurPipelineStateHandle_[pass] = pipelineStateObject_.GetOrCreate(device, blurKey);
		}

		composeComputeShader_ = shaderCache.GetOrCreateComputeShader(filePath, String("Compose"));

		PipelineStateKey composeKey{};
		memset(&composeKey, 0, sizeof(composeKey));
		composeKey.rootSignature_ = rootSignature_.Get(anamorphicFlareRootSignature_)->Get();
		composeKey.computeShader_ = shaderCache.GetComputeShader(composeComputeShader_)->Bytecode();
		composePipelineStateHandle_ = pipelineStateObject_.GetOrCreate(device, composeKey);
	}

	ID3D12PipelineState* AnamorphicFlare::GetPrefilterPipelineState()const
	{
		return pipelineStateObject_.Get(prefilterPipelineStateHandle_);
	}

	ID3D12PipelineState* AnamorphicFlare::GetBlurPipelineState(Uint32 passIndex)const
	{
		return pipelineStateObject_.Get(blurPipelineStateHandle_[passIndex]);
	}

	ID3D12PipelineState* AnamorphicFlare::GetComposePipelineState()const
	{
		return pipelineStateObject_.Get(composePipelineStateHandle_);
	}

	ID3D12RootSignature* AnamorphicFlare::GetRootSignature()const
	{
		return rootSignature_.Get(anamorphicFlareRootSignature_)->Get();
	}
}
