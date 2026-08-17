#include <GraphicsEngine/Raytracing/GlobalIllumination/GlobalIlluminationShader.h>
#include <GraphicsEngine/Shader/ShaderCache.h>
#include <GraphicsEngine/D3D12/PipelineState/RaytracingShader.h>

namespace SeedCore
{
	GlobalIlluminationShader::GlobalIlluminationShader(RootSignature& rootSignature, RaytracingStateObject& raytracingStateObject) : rootSignature_(rootSignature), raytracingStateObject_(raytracingStateObject)
	{
		/// No Code
	}

	/**
	* [EN]
	* Compiles the DXR library and builds the state object. Silently leaves the
	* handles empty when the shader fails to compile — GlobalIlluminationRenderer
	* checks GetStateObject() and falls back to clearing the output, so a broken
	* shader degrades to "no indirect light" instead of crashing.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* DXR ライブラリをコンパイルしてステートオブジェクトを構築する。シェーダの
	* コンパイルに失敗した場合はハンドルを空のままにする — GlobalIllumination
	* Renderer 側が GetStateObject() を見て出力クリアへ倒すので、壊れたシェーダは
	* クラッシュではなく「間接光なし」に縮退する。
	*/
	void GlobalIlluminationShader::Create(ShaderCache& shaderCache, ID3D12Device5* device)
	{
		globalIlluminationRootSignature_ = rootSignature_.GetOrCreate(device);

		libraryShader_ = shaderCache.GetOrCreateRaytracingShader(String("../GraphicsEngine/Raytracing/GlobalIllumination/GlobalIlluminationRT.hlsl"));

		RaytracingShader* library = shaderCache.GetRaytracingShader(libraryShader_);
		if (!library)
		{
			return;
		}

		RaytracingStateKey key{};
		key.globalRootSignature_ = rootSignature_.Get(globalIlluminationRootSignature_)->Get();
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

	ID3D12StateObject* GlobalIlluminationShader::GetStateObject()const
	{
		return raytracingStateObject_.Get(stateObjectHandle_);
	}

	ID3D12RootSignature* GlobalIlluminationShader::GetRootSignature()const
	{
		return rootSignature_.Get(globalIlluminationRootSignature_)->Get();
	}
}
