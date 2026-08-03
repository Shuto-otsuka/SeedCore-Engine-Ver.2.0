#pragma once
#include <FoundationEngine/Prelude.h>
#include <GraphicsEngine/Shape/Outline/OutlineShader.h>

namespace SeedCore
{
	class ShaderCache;
	class BindlessHeap;
	class D3D12CommandList;
	class FrameBuffer;

	/**
	* [EN]
	* Selection outline composite: fullscreen edge-detect over the shared
	* selection mask (see Renderer::selectionMaskFrameBuffer_), writing the
	* outline color onto whichever frame buffer the caller passes in.
	* Not tied to any one actor type — Model, Sprite, Billboard and Font all
	* draw their selected instances into the same mask beforehand; this class
	* only resolves that mask into an outline.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 選択アウトライン合成: 共有選択マスク（Renderer::selectionMaskFrameBuffer_
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

		void Draw(D3D12CommandList* cmdList, FrameBuffer* frameBuffer, ID3D12DescriptorHeap* heap, D3D12_GPU_VIRTUAL_ADDRESS constantIndex, D3D12_GPU_VIRTUAL_ADDRESS structuredIndex);

	private:
		OutlineShader outlineShader_;

		BindlessHeap* bindlessHeap_ = nullptr;
	};
}
