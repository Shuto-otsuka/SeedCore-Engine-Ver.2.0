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
	* Manages PSO creation for the bokeh-highlight compute pass (BokehCS.hlsl
	* — runs after DepthOfFieldCS.hlsl and read-modify-writes the same
	* native-res UAV, bright-passing the original scene color at points
	* traced along a blade-sided polygon and adding the result on top so
	* out-of-focus bright points keep a distinct shaped highlight instead of
	* being averaged away by the plain gather blur). Same shape as every
	* other single-PSO compute wrapper in this engine.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* ボケハイライトのコンピュートパス(BokehCS.hlsl — DepthOfFieldCS.hlsl の
	* 後に走り、同じネイティブ解像度 UAV を read-modify-write する。角形の
	* 輪郭に沿った点で元のシーン色をブライトパスし、その結果を加算すること
	* で、ピントの外れた明るい点が単純なギャザーブラーで平均化されて消える
	* ことなく、形のあるハイライトとして残る)の PSO 管理。このエンジンの
	* 他の単一 PSO コンピュートラッパーと同じ構成。
	*/
	class Bokeh
	{
	public:
		Bokeh(RootSignature& rootSignature, PipelineStateObject& pipelineStateObject);
		~Bokeh() = default;

		void Create(ShaderCache& shaderCache, ID3D12Device* device);

		[[nodiscard]] ID3D12PipelineState* GetPipelineState()const;

		[[nodiscard]] ID3D12RootSignature* GetRootSignature()const;

	private:
		Handle<ComputeShader> computeShader_;
		Handle<Microsoft::WRL::ComPtr<ID3D12PipelineState>> pipelineStateHandle_;

		Handle<RootSignature> bokehRootSignature_;

		RootSignature& rootSignature_;
		PipelineStateObject& pipelineStateObject_;
	};
}
