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
	* Manages PSO creation for the lens-flare compute pass (LensFlareCS.hlsl —
	* bright-passes the HDR scene color and accumulates a six-armed
	* diffraction-spike ("starburst") pattern at a quarter of the native
	* resolution; ToneMappingCS.hlsl samples the result and adds it into the
	* HDR color before exposure). Same shape as every other single-PSO
	* compute wrapper in this engine.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* レンズフレアのコンピュートパス(LensFlareCS.hlsl — HDRシーンカラーを
	* ブライトパスし、ネイティブ解像度の1/4で6本腕の回折スパイク
	* (スターバースト)パターンを蓄積する。ToneMappingCS.hlsl がその結果を
	* サンプルし、露出適用前のHDRカラーへ加算する)の PSO 管理。このエンジンの
	* 他の単一 PSO コンピュートラッパーと同じ構成。
	*/
	class LensFlare
	{
	public:
		LensFlare(RootSignature& rootSignature, PipelineStateObject& pipelineStateObject);
		~LensFlare() = default;

		void Create(ShaderCache& shaderCache, ID3D12Device* device);

		[[nodiscard]] ID3D12PipelineState* GetPipelineState()const;

		[[nodiscard]] ID3D12RootSignature* GetRootSignature()const;

	private:
		Handle<ComputeShader> computeShader_;
		Handle<Microsoft::WRL::ComPtr<ID3D12PipelineState>> pipelineStateHandle_;

		Handle<RootSignature> lensFlareRootSignature_;

		RootSignature& rootSignature_;
		PipelineStateObject& pipelineStateObject_;
	};
}
