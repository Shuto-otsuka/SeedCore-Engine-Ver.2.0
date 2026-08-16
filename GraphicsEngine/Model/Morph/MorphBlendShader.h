#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	class ShaderCache;

	/**
	* [EN]
	* PSO/root-signature wrapper for MorphBlendCS.hlsl — blends a SubMesh's
	* morph target deltas into its RT proxy position range. See
	* RaytracingRenderer for dispatch/orchestration.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* MorphBlendCS.hlsl の PSO/RootSignature ラッパー — SubMesh のモーフ
	* ターゲットデルタを、その RT プロキシ位置範囲へブレンドする。
	* ディスパッチ/統括は RaytracingRenderer 参照。
	*/
	class MorphBlendShader
	{
	public:
		MorphBlendShader() = default;
		~MorphBlendShader() = default;

		void Create(ShaderCache& shaderCache, ID3D12Device* device);

		[[nodiscard]] ID3D12RootSignature* GetRootSignature()const;

		[[nodiscard]] ID3D12PipelineState* GetPipelineState()const;

	private:
		Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;
	};
}
