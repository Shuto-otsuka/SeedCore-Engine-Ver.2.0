#pragma once
#include <FoundationEngine/Prelude.h>
#include <GraphicsEngine/Shape/Outline/OutlineShader.h>

namespace SeedCore
{
	struct RootAddresses;
	class ShaderCache;
	class BindlessHeap;
	class D3D12CommandList;

	/**
	* [EN]
	* Selection outline composite: fullscreen edge-detect over the shared
	* silhouette (see Renderer::silhouetteFrameBuffer_), writing the
	* outline color onto whichever frame buffer the caller passes in.
	* Not tied to any one actor type — Model, Sprite, Billboard and Font all
	* draw their selected instances into the same mask beforehand; this class
	* only resolves that mask into an outline.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 選択アウトライン合成: 共有シルエット（Renderer::silhouetteFrameBuffer_
	* 参照）に対するフルスクリーンのエッジ検出で、呼び出し側が渡した
	* フレームバッファへ縁取り色を書く。特定のアクター種別には紐付かない —
	* Model / Sprite / Billboard / Font はいずれも選択中インスタンスを事前に
	* 同じマスクへ描き込み、このクラスはそのマスクを合成するだけ。
	*/
	class OutlineRenderer
	{
	public:
		OutlineRenderer(RootSignature& rootSignature, PipelineStateObject& pipelineStateObject);
		~OutlineRenderer() = default;

		void Create(ID3D12Device* device, BindlessHeap* bindlessHeap, ShaderCache& shaderCache);

		/// [EN] Takes a raw render target view/viewport rather than
		///      FrameBuffer*. Draw targets R16G16B16A16_FLOAT frame buffers
		///      (the canvas frame buffer); DrawDebugOverlay targets
		///      PostProcessRenderer's R8G8B8A8_UNORM post-tonemap output (see
		///      Renderer::EndEditorFrame) - each uses the PSO built for that
		///      format. No depth binding - this pass is a fullscreen composite
		///      over the precomputed silhouette, not a depth-tested draw.
		///      Caller owns renderTargetView's resource state transition (must
		///      already be RENDER_TARGET).
		/// [JP] FrameBuffer* ではなく生のレンダーターゲットビュー/ビューポートを
		///      受け取る。Draw は R16G16B16A16_FLOAT のフレームバッファ(キャンバス
		///      フレームバッファ)向け、DrawDebugOverlay は PostProcessRenderer の
		///      R8G8B8A8_UNORM のトーンマップ後出力(Renderer::EndEditorFrame参照)
		///      向けで、それぞれのフォーマット用に作った PSO を使う。深度バインド
		///      無し - このパスは事前計算済みシルエットに対するフルスクリーン
		///      合成であり、深度テストされる描画ではない。renderTargetView の
		///      リソース状態遷移(既にRENDER_TARGETであること)は呼び出し側の
		///      責任。
		void Draw(D3D12CommandList* cmdList, D3D12_CPU_DESCRIPTOR_HANDLE renderTargetView, D3D12_VIEWPORT viewport, ID3D12DescriptorHeap* heap, const RootAddresses& addresses);

		void DrawDebugOverlay(D3D12CommandList* cmdList, D3D12_CPU_DESCRIPTOR_HANDLE renderTargetView, D3D12_VIEWPORT viewport, ID3D12DescriptorHeap* heap, const RootAddresses& addresses);

	private:
		OutlineShader outlineShader_;

		BindlessHeap* bindlessHeap_ = nullptr;
	};
}
