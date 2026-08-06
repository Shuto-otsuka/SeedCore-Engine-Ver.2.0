#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/Utility/Handle.h>
#include <GraphicsEngine/D3D12/PipelineState/RootSignature.h>
#include <GraphicsEngine/D3D12/PipelineState/PipelineStateObject.h>

namespace SeedCore
{
	class ShaderCache;
	class ComputeShader;

	/**
	* [EN]
	* Manages PSO creation for the VisibilityBuffer material pipeline: the
	* material sort (Model/MaterialClassifyCS.hlsl / MaterialPrefixSumCS.hlsl /
	* MaterialScatterCS.hlsl) followed by the material resolve compute pass
	* (Model/MaterialResolveCS.hlsl). Same shared root signature as every
	* other compute pass. Owns no resources of its own - it only reads/writes
	* the existing GeometryBuffer render targets and MaterialSortBuffer via
	* bindless indices.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* VisibilityBuffer マテリアルパイプライン全体のPSO管理: マテリアルソート
	* (Model/MaterialClassifyCS.hlsl / MaterialPrefixSumCS.hlsl /
	* MaterialScatterCS.hlsl)に続くマテリアル解決コンピュートパス
	* (Model/MaterialResolveCS.hlsl)。他の全コンピュートパスと同じ共有
	* ルートシグネチャ。自前のリソースは持たない — 既存の GeometryBuffer の
	* レンダーターゲットと MaterialSortBuffer を bindless インデックス経由で
	* 読み書きするだけ。
	*/
	class MaterialResolveShader
	{
	public:
		MaterialResolveShader(RootSignature& rootSignature, PipelineStateObject& pipelineStateObject);
		~MaterialResolveShader() = default;

		void Create(ShaderCache& shaderCache, ID3D12Device* device);

		[[nodiscard]] ID3D12PipelineState* GetPipelineState()const;

		[[nodiscard]] ID3D12PipelineState* GetClassifyPipelineState()const;

		[[nodiscard]] ID3D12PipelineState* GetPrefixSumPipelineState()const;

		[[nodiscard]] ID3D12PipelineState* GetScatterPipelineState()const;

		[[nodiscard]] ID3D12RootSignature* GetRootSignature()const;

	private:
		Handle<ComputeShader> computeShader_;
		Handle<Microsoft::WRL::ComPtr<ID3D12PipelineState>> pipelineStateObjectHandle_;

		Handle<ComputeShader> classifyComputeShader_;
		Handle<Microsoft::WRL::ComPtr<ID3D12PipelineState>> classifyPipelineStateObjectHandle_;

		Handle<ComputeShader> prefixSumComputeShader_;
		Handle<Microsoft::WRL::ComPtr<ID3D12PipelineState>> prefixSumPipelineStateObjectHandle_;

		Handle<ComputeShader> scatterComputeShader_;
		Handle<Microsoft::WRL::ComPtr<ID3D12PipelineState>> scatterPipelineStateObjectHandle_;

		Handle<RootSignature> materialResolveRootSignature_;

		RootSignature& rootSignature_;
		PipelineStateObject& pipelineStateObject_;
	};
}
