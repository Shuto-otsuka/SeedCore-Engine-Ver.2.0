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
	* Manages PSO creation for the colour grading pass (ColorGradingCS.hlsl —
	* Unreal-style four-range grading, run in scene-referred linear space
	* after exposure and before the tone curve). Same shape as every other
	* single-PSO compute wrapper in this engine.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* カラーグレーディングパス(ColorGradingCS.hlsl — Unreal 方式の4階調域
	* グレーディング。露出の後・トーンカーブの前、シーン参照リニア空間で
	* 走る)の PSO 管理。このエンジンの他の単一 PSO コンピュートラッパーと
	* 同じ構成。
	*/
	class ColorGrading
	{
	public:
		ColorGrading(RootSignature& rootSignature, PipelineStateObject& pipelineStateObject);
		~ColorGrading() = default;

		void Create(ShaderCache& shaderCache, ID3D12Device* device);

		[[nodiscard]] ID3D12PipelineState* GetPipelineState()const;

		[[nodiscard]] ID3D12RootSignature* GetRootSignature()const;

	private:
		Handle<ComputeShader> computeShader_;
		Handle<Microsoft::WRL::ComPtr<ID3D12PipelineState>> pipelineStateHandle_;

		Handle<RootSignature> colorGradingRootSignature_;

		RootSignature& rootSignature_;
		PipelineStateObject& pipelineStateObject_;
	};
}
