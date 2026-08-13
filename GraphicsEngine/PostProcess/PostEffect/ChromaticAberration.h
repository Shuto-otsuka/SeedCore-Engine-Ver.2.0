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
	* Manages PSO creation for the chromatic-aberration pass
	* (ChromaticAberrationCS.hlsl — lateral chromatic aberration, resampling
	* the scene along the radial direction with a spectral weighting, run as
	* part of the lens stage before exposure and tone mapping). Same shape as
	* every other single-PSO compute wrapper in this engine.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 色収差パス(ChromaticAberrationCS.hlsl — 倍率色収差。シーンを半径方向へ
	* スペクトル重み付きで再サンプルする。露出とトーンマップより前のレンズ段
	* の一部として走る)の PSO 管理。このエンジンの他の単一 PSO コンピュート
	* ラッパーと同じ構成。
	*/
	class ChromaticAberration
	{
	public:
		ChromaticAberration(RootSignature& rootSignature, PipelineStateObject& pipelineStateObject);
		~ChromaticAberration() = default;

		void Create(ShaderCache& shaderCache, ID3D12Device* device);

		[[nodiscard]] ID3D12PipelineState* GetPipelineState()const;

		[[nodiscard]] ID3D12RootSignature* GetRootSignature()const;

	private:
		Handle<ComputeShader> computeShader_;
		Handle<Microsoft::WRL::ComPtr<ID3D12PipelineState>> pipelineStateHandle_;

		Handle<RootSignature> chromaticAberrationRootSignature_;

		RootSignature& rootSignature_;
		PipelineStateObject& pipelineStateObject_;
	};
}
