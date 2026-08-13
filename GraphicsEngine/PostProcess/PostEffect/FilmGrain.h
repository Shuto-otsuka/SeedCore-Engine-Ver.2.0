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
	* Manages PSO creation for the film-grain pass (FilmGrainCS.hlsl —
	* procedural grain with the midtone-peaked film response, run last and
	* read-modify-writing SharpnessCS.hlsl's output in place). Same shape as
	* every other single-PSO compute wrapper in this engine.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* フィルムグレインパス(FilmGrainCS.hlsl — 中間調で最大になるフィルムの
	* 応答を持つ手続き型グレイン。最後に走り、SharpnessCS.hlsl の出力を
	* その場で read-modify-write する)の PSO 管理。このエンジンの他の
	* 単一 PSO コンピュートラッパーと同じ構成。
	*/
	class FilmGrain
	{
	public:
		FilmGrain(RootSignature& rootSignature, PipelineStateObject& pipelineStateObject);
		~FilmGrain() = default;

		void Create(ShaderCache& shaderCache, ID3D12Device* device);

		[[nodiscard]] ID3D12PipelineState* GetPipelineState()const;

		[[nodiscard]] ID3D12RootSignature* GetRootSignature()const;

	private:
		Handle<ComputeShader> computeShader_;
		Handle<Microsoft::WRL::ComPtr<ID3D12PipelineState>> pipelineStateHandle_;

		Handle<RootSignature> filmGrainRootSignature_;

		RootSignature& rootSignature_;
		PipelineStateObject& pipelineStateObject_;
	};
}
