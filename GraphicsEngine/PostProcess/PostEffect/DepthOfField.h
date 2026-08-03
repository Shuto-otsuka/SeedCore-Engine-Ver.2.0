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
	* Manages PSO creation for the depth-of-field compute pass
	* (DepthOfFieldCS.hlsl — a circle-of-confusion gather blur that replaces
	* the native-res HDR scene color with a blurred version; BokehCS.hlsl
	* then read-modify-writes the same buffer to add shaped highlights).
	* Same shape as every other single-PSO compute wrapper in this engine.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 被写界深度のコンピュートパス(DepthOfFieldCS.hlsl — 錯乱円ギャザーブラーで
	* ネイティブ解像度のHDRシーン色をぼかしたものへ置き換える。BokehCS.hlsl が
	* 続けて同じバッファへ形状付きハイライトを read-modify-write する)の PSO
	* 管理。このエンジンの他の単一 PSO コンピュートラッパーと同じ構成。
	*/
	class DepthOfField
	{
	public:
		DepthOfField(RootSignature& rootSignature, PipelineStateObject& pipelineStateObject);
		~DepthOfField() = default;

		void Create(ShaderCache& shaderCache, ID3D12Device* device);

		[[nodiscard]] ID3D12PipelineState* GetPipelineState()const;

		[[nodiscard]] ID3D12RootSignature* GetRootSignature()const;

	private:
		Handle<ComputeShader> computeShader_;
		Handle<Microsoft::WRL::ComPtr<ID3D12PipelineState>> pipelineStateHandle_;

		Handle<RootSignature> depthOfFieldRootSignature_;

		RootSignature& rootSignature_;
		PipelineStateObject& pipelineStateObject_;
	};
}
