#include <GraphicsEngine/Raytracing/Refraction/RefractionShader.h>
#include <GraphicsEngine/Shader/ShaderCache.h>
#include <GraphicsEngine/D3D12/PipelineState/RaytracingShader.h>

namespace SeedCore
{
	RefractionShader::RefractionShader(RootSignature& rootSignature, RaytracingStateObject& raytracingStateObject) : rootSignature_(rootSignature), raytracingStateObject_(raytracingStateObject)
	{
		/// No Code
	}

	void RefractionShader::Create(ShaderCache& shaderCache, ID3D12Device5* device)
	{
		refractionRootSignature_ = rootSignature_.GetOrCreate(device);

		libraryShader_ = shaderCache.GetOrCreateRaytracingShader(String("../GraphicsEngine/Raytracing/Refraction/RefractionRT.hlsl"));

		RaytracingShader* library = shaderCache.GetRaytracingShader(libraryShader_);
		if (!library)
		{
			return;
		}

		RaytracingStateKey key{};
		key.globalRootSignature_ = rootSignature_.Get(refractionRootSignature_)->Get();
		key.localRootSignature_ = nullptr;
		key.libraryShader_ = library->Bytecode();
		key.rayGenExportName_ = String(rayGenExportName);
		key.maxTraceRecursionDepth_ = 1;
		key.maxAttributeSizeInBytes_ = 8;
		key.maxPayloadSizeInBytes_ = 16;

		stateObjectHandle_ = raytracingStateObject_.GetOrCreate(device, key);
	}

	ID3D12StateObject* RefractionShader::GetStateObject()const
	{
		return raytracingStateObject_.Get(stateObjectHandle_);
	}

	ID3D12RootSignature* RefractionShader::GetRootSignature()const
	{
		return rootSignature_.Get(refractionRootSignature_)->Get();
	}
}
