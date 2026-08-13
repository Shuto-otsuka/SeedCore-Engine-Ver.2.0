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
	* Manages PSO creation for the lens-distortion pass
	* (LensDistortionCS.hlsl — the radial half of the Brown-Conrady model,
	* run as the first stage of the lens chain before exposure and tone
	* mapping). Same shape as every other single-PSO compute wrapper in this
	* engine.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* レンズ歪曲パス(LensDistortionCS.hlsl — Brown-Conrady モデルの半径方向の
	* 項。露出とトーンマップより前、レンズ連鎖の最初の段として走る)の
	* PSO 管理。このエンジンの他の単一 PSO コンピュートラッパーと同じ構成。
	*/
	class LensDistortion
	{
	public:
		LensDistortion(RootSignature& rootSignature, PipelineStateObject& pipelineStateObject);
		~LensDistortion() = default;

		void Create(ShaderCache& shaderCache, ID3D12Device* device);

		[[nodiscard]] ID3D12PipelineState* GetPipelineState()const;

		[[nodiscard]] ID3D12RootSignature* GetRootSignature()const;

	private:
		Handle<ComputeShader> computeShader_;
		Handle<Microsoft::WRL::ComPtr<ID3D12PipelineState>> pipelineStateHandle_;

		Handle<RootSignature> lensDistortionRootSignature_;

		RootSignature& rootSignature_;
		PipelineStateObject& pipelineStateObject_;
	};
}
