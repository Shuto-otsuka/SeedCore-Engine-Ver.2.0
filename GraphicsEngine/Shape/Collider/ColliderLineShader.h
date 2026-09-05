#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/Utility/Handle.h>
#include <GraphicsEngine/D3D12/PipelineState/PipelineStateObject.h>
#include <GraphicsEngine/D3D12/PipelineState/DepthStencilState.h>

namespace SeedCore
{
	class ShaderCache;
	class MeshShader;
	class PixelShader;

	/**
	* [EN]
	* Manages the root signature and PSO for collider debug-line rendering
	* (unlit, vertex-colored line list, mesh-shader based like the rest of
	* this engine's renderers). Deliberately does NOT reuse the engine's
	* shared bindless RootSignature (Renderer::rootSignature_) — this PSO
	* only needs the current view-projection (via the existing per-view
	* ConstantIndices CBV, register b0 space1) and one small, privately
	* owned constant buffer describing this frame's line-vertex buffer
	* (register b0 space0). Keeping it self-contained avoids touching the
	* shared StructuredIndices layout that every other renderer depends on.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* コライダーのデバッグライン描画（アンリット・頂点カラーのラインリスト、
	* 本エンジンの他レンダラーと同じくメッシュシェーダベース）用の
	* RootSignature と PSO を管理する。意図的にエンジン共有の bindless
	* RootSignature（Renderer::rootSignature_）は使わない — このPSOが必要と
	* するのは現在ビューの view-projection（既存の ConstantIndices CBV,
	* register b0 space1 経由）と、このフレームの line-vertex バッファを表す
	* 小さな専用定数バッファ（register b0 space0）のみ。自己完結させることで、
	* 他の全レンダラーが依存する共有 StructuredIndices のレイアウトに触れずに済む。
	*/
	class ColliderLineShader
	{
	public:
		ColliderLineShader() = default;
		~ColliderLineShader() = default;

		void Create(ShaderCache& shaderCache, ID3D12Device* device, PipelineStateObject& pipelineStateObject, DepthStencilStateType depthStencilStateType = DepthStencilStateType::DepthOnWriteOffReverseZ);

		[[nodiscard]] ID3D12PipelineState* GetPipelineState()const;

		[[nodiscard]] ID3D12RootSignature* GetRootSignature()const;

	private:
		Handle<MeshShader> lineMeshShader_;
		Handle<PixelShader> linePixelShader_;
		Handle<Microsoft::WRL::ComPtr<ID3D12PipelineState>> pipelineState_;

		Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;

		PipelineStateObject* pipelineStateObject_ = nullptr;
	};
}
