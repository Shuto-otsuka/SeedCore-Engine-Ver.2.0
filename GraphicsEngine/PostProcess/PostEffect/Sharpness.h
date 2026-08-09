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
	* Manages PSO creation for the final display sharpen pass
	* (SharpnessCS.hlsl — a 5-tap cross unsharp mask over ToneMappingCS.hlsl's
	* output, producing the new final display texture). Same shape as every
	* other single-PSO compute wrapper in this engine.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 最終表示シャープパス(SharpnessCS.hlsl — ToneMappingCS.hlsl の出力に
	* 対する5タップ十字アンシャープマスク、新しい最終表示テクスチャを生成する)
	* の PSO 管理。このエンジンの他の単一 PSO コンピュートラッパーと同じ構成。
	*/
	class Sharpness
	{
	public:
		Sharpness(RootSignature& rootSignature, PipelineStateObject& pipelineStateObject);
		~Sharpness() = default;

		void Create(ShaderCache& shaderCache, ID3D12Device* device);

		[[nodiscard]] ID3D12PipelineState* GetPipelineState()const;

		[[nodiscard]] ID3D12RootSignature* GetRootSignature()const;

	private:
		Handle<ComputeShader> computeShader_;
		Handle<Microsoft::WRL::ComPtr<ID3D12PipelineState>> pipelineStateHandle_;

		Handle<RootSignature> sharpnessRootSignature_;

		RootSignature& rootSignature_;
		PipelineStateObject& pipelineStateObject_;
	};
}
