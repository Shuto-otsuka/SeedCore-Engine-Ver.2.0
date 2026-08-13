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
	* Manages PSO creation for the vignette pass (VignetteCS.hlsl — natural
	* vignetting via the cosine-fourth law, run as part of the lens stage
	* before exposure and tone mapping). Same shape as every other single-PSO
	* compute wrapper in this engine.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* ビネットパス(VignetteCS.hlsl — コサイン4乗則による自然ビネット。
	* 露出とトーンマップより前のレンズ段の一部として走る)の PSO 管理。
	* このエンジンの他の単一 PSO コンピュートラッパーと同じ構成。
	*/
	class Vignette
	{
	public:
		Vignette(RootSignature& rootSignature, PipelineStateObject& pipelineStateObject);
		~Vignette() = default;

		void Create(ShaderCache& shaderCache, ID3D12Device* device);

		[[nodiscard]] ID3D12PipelineState* GetPipelineState()const;

		[[nodiscard]] ID3D12RootSignature* GetRootSignature()const;

	private:
		Handle<ComputeShader> computeShader_;
		Handle<Microsoft::WRL::ComPtr<ID3D12PipelineState>> pipelineStateHandle_;

		Handle<RootSignature> vignetteRootSignature_;

		RootSignature& rootSignature_;
		PipelineStateObject& pipelineStateObject_;
	};
}
