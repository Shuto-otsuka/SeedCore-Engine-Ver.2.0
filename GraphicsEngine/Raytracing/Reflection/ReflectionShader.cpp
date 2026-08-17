#include <GraphicsEngine/Raytracing/Reflection/ReflectionShader.h>
#include <GraphicsEngine/Shader/ShaderCache.h>
#include <GraphicsEngine/D3D12/PipelineState/RaytracingShader.h>

namespace SeedCore
{
	ReflectionShader::ReflectionShader(RootSignature& rootSignature, RaytracingStateObject& raytracingStateObject) : rootSignature_(rootSignature), raytracingStateObject_(raytracingStateObject)
	{
		/// No Code
	}

	void ReflectionShader::Create(ShaderCache& shaderCache, ID3D12Device5* device)
	{
		reflectionRootSignature_ = rootSignature_.GetOrCreate(device);

		libraryShader_ = shaderCache.GetOrCreateRaytracingShader(String("../GraphicsEngine/Raytracing/Reflection/ReflectionRT.hlsl"));

		RaytracingShader* library = shaderCache.GetRaytracingShader(libraryShader_);
		if (!library)
		{
			return;
		}

		RaytracingStateKey key{};
		key.globalRootSignature_ = rootSignature_.Get(reflectionRootSignature_)->Get();
		key.localRootSignature_ = nullptr;
		key.libraryShader_ = library->Bytecode();
		key.rayGenExportName_ = String(rayGenExportName);
		key.missExportName_ = String(missExportName);
		key.closestHitExportName_ = String(closestHitExportName);
		key.anyHitExportName_ = String(anyHitExportName);
		key.hitGroupName_ = String(hitGroupName);
		key.maxTraceRecursionDepth_ = 1;
		key.maxAttributeSizeInBytes_ = 8;
		key.maxPayloadSizeInBytes_ = 16;

		stateObjectHandle_ = raytracingStateObject_.GetOrCreate(device, key);
	}

	ID3D12StateObject* ReflectionShader::GetStateObject()const
	{
		return raytracingStateObject_.Get(stateObjectHandle_);
	}

	ID3D12RootSignature* ReflectionShader::GetRootSignature()const
	{
		return rootSignature_.Get(reflectionRootSignature_)->Get();
	}
}
