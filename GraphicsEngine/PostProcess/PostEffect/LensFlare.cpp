#include <GraphicsEngine/PostProcess/PostEffect/LensFlare.h>
#include <GraphicsEngine/Shader/ShaderCache.h>
#include <GraphicsEngine/D3D12/PipelineState/ComputeShader.h>

namespace SeedCore
{
	namespace
	{
		constexpr const Char* blurEntryPoints[4] = { "BlurPass1", "BlurPass2", "BlurPass3", "BlurPass4" };
	}

	LensFlare::LensFlare(RootSignature& rootSignature, PipelineStateObject& pipelineStateObject) : rootSignature_(rootSignature), pipelineStateObject_(pipelineStateObject)
	{
		/// No Code
	}

	void LensFlare::Create(ShaderCache& shaderCache, ID3D12Device* device)
	{
		lensFlareRootSignature_ = rootSignature_.GetOrCreate(device);

		const String filePath = String("../GraphicsEngine/PostProcess/LensFlareCS.hlsl");

		downsampleComputeShader_ = shaderCache.GetOrCreateComputeShader(filePath, String("Downsample"));

		PipelineStateKey downsampleKey{};
		memset(&downsampleKey, 0, sizeof(downsampleKey));
		downsampleKey.rootSignature_ = rootSignature_.Get(lensFlareRootSignature_)->Get();
		downsampleKey.computeShader_ = shaderCache.GetComputeShader(downsampleComputeShader_)->Bytecode();
		downsamplePipelineStateHandle_ = pipelineStateObject_.GetOrCreate(device, downsampleKey);

		for (Uint32 pass = 0; pass < blurPassCount; pass++)
		{
			blurComputeShader_[pass] = shaderCache.GetOrCreateComputeShader(filePath, String(blurEntryPoints[pass]));

			PipelineStateKey blurKey{};
			memset(&blurKey, 0, sizeof(blurKey));
			blurKey.rootSignature_ = rootSignature_.Get(lensFlareRootSignature_)->Get();
			blurKey.computeShader_ = shaderCache.GetComputeShader(blurComputeShader_[pass])->Bytecode();
			blurPipelineStateHandle_[pass] = pipelineStateObject_.GetOrCreate(device, blurKey);
		}

		composeComputeShader_ = shaderCache.GetOrCreateComputeShader(filePath, String("Compose"));

		PipelineStateKey composeKey{};
		memset(&composeKey, 0, sizeof(composeKey));
		composeKey.rootSignature_ = rootSignature_.Get(lensFlareRootSignature_)->Get();
		composeKey.computeShader_ = shaderCache.GetComputeShader(composeComputeShader_)->Bytecode();
		composePipelineStateHandle_ = pipelineStateObject_.GetOrCreate(device, composeKey);

		ghostComputeShader_ = shaderCache.GetOrCreateComputeShader(filePath, String("Ghost"));

		PipelineStateKey ghostKey{};
		memset(&ghostKey, 0, sizeof(ghostKey));
		ghostKey.rootSignature_ = rootSignature_.Get(lensFlareRootSignature_)->Get();
		ghostKey.computeShader_ = shaderCache.GetComputeShader(ghostComputeShader_)->Bytecode();
		ghostPipelineStateHandle_ = pipelineStateObject_.GetOrCreate(device, ghostKey);
	}

	ID3D12PipelineState* LensFlare::GetDownsamplePipelineState()const
	{
		return pipelineStateObject_.Get(downsamplePipelineStateHandle_);
	}

	ID3D12PipelineState* LensFlare::GetBlurPipelineState(Uint32 passIndex)const
	{
		return pipelineStateObject_.Get(blurPipelineStateHandle_[passIndex]);
	}

	ID3D12PipelineState* LensFlare::GetComposePipelineState()const
	{
		return pipelineStateObject_.Get(composePipelineStateHandle_);
	}

	ID3D12PipelineState* LensFlare::GetGhostPipelineState()const
	{
		return pipelineStateObject_.Get(ghostPipelineStateHandle_);
	}

	ID3D12RootSignature* LensFlare::GetRootSignature()const
	{
		return rootSignature_.Get(lensFlareRootSignature_)->Get();
	}
}
