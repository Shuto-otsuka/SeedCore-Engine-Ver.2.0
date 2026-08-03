#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/Utility/Handle.h>
#include <GraphicsEngine/D3D12/PipelineState/RootSignature.h>
#include <GraphicsEngine/D3D12/PipelineState/PipelineStateObject.h>

namespace SeedCore
{
	class ShaderCache;
	class MeshShader;
	class PixelShader;

	/**
	* [EN]
	* Manages PSO creation for the selection outline composite.
	*
	* Composite PSO (fullscreen, edge-detects the shared selection mask onto
	* whichever frame buffer the caller is targeting):
	*   - SelectionOutlineMS + SelectionOutlinePS
	*
	* Not tied to any one actor type — Model, Sprite, Billboard and Font all
	* draw their selected instances into the same mask beforehand (see each
	* renderer's DrawSelectionMask); this PSO only resolves that mask.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 選択アウトライン合成用の PSO 管理。
	*
	* 合成 PSO（フルスクリーン、共有選択マスクを呼び出し側の対象フレーム
	* バッファへエッジ検出合成する）:
	*   - SelectionOutlineMS + SelectionOutlinePS
	*
	* 特定のアクター種別には紐付かない — Model / Sprite / Billboard / Font は
	* いずれも選択中インスタンスを事前に同じマスクへ描き込み（各 Renderer の
	* DrawSelectionMask 参照）、この PSO はそのマスクを合成するだけ。
	*/
	class OutlineShader
	{
	public:
		OutlineShader(RootSignature& rootSignature, PipelineStateObject& pipelineStateObject);
		~OutlineShader() = default;

		void Create(ShaderCache& shaderCache, ID3D12Device* device);

		[[nodiscard]] ID3D12PipelineState* GetPipelineStateComposite()const;

		[[nodiscard]] ID3D12RootSignature* GetRootSignature()const;

	private:
		Handle<MeshShader> compositeMeshShader_;
		Handle<PixelShader> compositePixelShader_;
		Handle<Microsoft::WRL::ComPtr<ID3D12PipelineState>> pipelineStateObjectComposite_;

		Handle<RootSignature> outlineRootSignature_;

		RootSignature& rootSignature_;
		PipelineStateObject& pipelineStateObject_;
	};
}
