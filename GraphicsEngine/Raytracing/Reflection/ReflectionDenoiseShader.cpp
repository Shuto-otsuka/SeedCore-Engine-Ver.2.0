#include <GraphicsEngine/Raytracing/Reflection/ReflectionDenoiseShader.h>
#include <GraphicsEngine/Shader/ShaderCache.h>
#include <GraphicsEngine/D3D12/PipelineState/ComputeShader.h>

namespace SeedCore
{
	namespace
	{
		/// [EN] Entry points of the three spatial passes, in execution order -
		///      the index GetSpatialPipelineState() takes.
		/// [JP] 3 つの空間パスのエントリポイント、実行順 —
		///      GetSpatialPipelineState() が取る添字と対応。
		constexpr const Char* spatialEntryPoints[3] = { "PrePass", "Blur", "PostBlur" };
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

		historyFixComputeShader_ = shaderCache.GetOrCreateComputeShader(filePath, String("HistoryFix"));

		PipelineStateKey historyFixKey{};
		memset(&historyFixKey, 0, sizeof(historyFixKey));
		historyFixKey.rootSignature_ = rootSignature_.Get(denoiseRootSignature_)->Get();
		historyFixKey.computeShader_ = shaderCache.GetComputeShader(historyFixComputeShader_)->Bytecode();
		historyFixPipelineStateObjectHandle_ = pipelineStateObject_.GetOrCreate(device, historyFixKey);

		for (Uint32 pass = 0; pass < spatialPassCount; pass++)
		{
			spatialComputeShader_[pass] = shaderCache.GetOrCreateComputeShader(filePath, String(spatialEntryPoints[pass]));

			PipelineStateKey spatialKey{};
			memset(&spatialKey, 0, sizeof(spatialKey));
			spatialKey.rootSignature_ = rootSignature_.Get(denoiseRootSignature_)->Get();
			spatialKey.computeShader_ = shaderCache.GetComputeShader(spatialComputeShader_[pass])->Bytecode();
			spatialPipelineStateObjectHandle_[pass] = pipelineStateObject_.GetOrCreate(device, spatialKey);
		}
	}

	ID3D12PipelineState* ReflectionDenoiseShader::GetPipelineState()const
	{
		return pipelineStateObject_.Get(pipelineStateObjectHandle_);
	}

	ID3D12PipelineState* ReflectionDenoiseShader::GetHistoryFixPipelineState()const
	{
		return pipelineStateObject_.Get(historyFixPipelineStateObjectHandle_);
	}

	ID3D12PipelineState* ReflectionDenoiseShader::GetSpatialPipelineState(Uint32 passIndex)const
	{
		return pipelineStateObject_.Get(spatialPipelineStateObjectHandle_[passIndex]);
	}

	ID3D12RootSignature* ReflectionDenoiseShader::GetRootSignature()const
	{
		return rootSignature_.Get(denoiseRootSignature_)->Get();
	}
}
