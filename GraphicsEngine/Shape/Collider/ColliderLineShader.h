#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/Utility/Handle.h>
#include <GraphicsEngine/D3D12/PipelineState/RootSignature.h>
#include <GraphicsEngine/D3D12/PipelineState/PipelineStateObject.h>
#include <GraphicsEngine/D3D12/PipelineState/DepthStencilState.h>

namespace SeedCore
{
	class ShaderCache;
	class VertexShader;
	class MeshShader;
	class PixelShader;

	/**
	* [EN]
	* Manages the PSO for collider debug-line rendering (unlit, vertex-colored
	* line list). Uses the engine's shared bindless RootSignature like every
	* other renderer: the current view comes from ConstantIndices (b2 space1),
	* and this frame's ColliderConstantBuffer is reached through
	* ConstantIndices::collider_index_. Builds a Mesh Shader PSO on D12_2
	* devices and a Vertex Shader line-list PSO otherwise.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* コライダーのデバッグライン描画（アンリット・頂点カラーのラインリスト）用の
	* PSO を管理する。他の全レンダラーと同じくエンジン共有の bindless
	* RootSignature を使う — 現在ビューは ConstantIndices（b2 space1）から、
	* このフレームの ColliderConstantBuffer は ConstantIndices::collider_index_
	* 経由で引く。D12_2 のデバイスではメッシュシェーダの PSO、それ以外では
	* 頂点シェーダのラインリスト PSO を構築する。
	*/
	class ColliderLineShader
	{
	public:
		ColliderLineShader(RootSignature& rootSignature, PipelineStateObject& pipelineStateObject);
		~ColliderLineShader() = default;

		void Create(ShaderCache& shaderCache, ID3D12Device* device, DepthStencilStateType depthStencilStateType = DepthStencilStateType::DepthOnWriteOffReverseZ);

		[[nodiscard]] ID3D12PipelineState* GetPipelineState()const;

		[[nodiscard]] ID3D12RootSignature* GetRootSignature()const;

	private:
		Handle<VertexShader> lineVertexShader_;
		Handle<MeshShader> lineMeshShader_;
		Handle<PixelShader> linePixelShader_;
		Handle<Microsoft::WRL::ComPtr<ID3D12PipelineState>> pipelineState_;

		Handle<RootSignature> lineRootSignature_;

		RootSignature& rootSignature_;
		PipelineStateObject& pipelineStateObject_;
	};
}
