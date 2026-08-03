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
	* Manages PSO creation for the tone-mapping display pass (ToneMappingCS.hlsl
	* — reads the HDR scene color plus the auto-exposure value, applies
	* exposure + a tone curve + the sRGB OETF, and writes the result into a
	* UNORM texture that becomes what the editor/game viewport actually shows).
	* Same shape as every other single-PSO compute wrapper in this engine.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* トーンマップ表示パス(ToneMappingCS.hlsl — HDR シーンカラーと自動露出値を
	* 読み、露出+トーンカーブ+sRGB OETF を適用して、エディタ/ゲームビューポート
	* が実際に表示する UNORM テクスチャへ書き込む)の PSO 管理。このエンジンの
	* 他の単一 PSO コンピュートラッパーと同じ構成。
	*/
	class ToneMapping
	{
	public:
		ToneMapping(RootSignature& rootSignature, PipelineStateObject& pipelineStateObject);
		~ToneMapping() = default;

		void Create(ShaderCache& shaderCache, ID3D12Device* device);

		[[nodiscard]] ID3D12PipelineState* GetPipelineState()const;

		[[nodiscard]] ID3D12RootSignature* GetRootSignature()const;

	private:
		Handle<ComputeShader> computeShader_;
		Handle<Microsoft::WRL::ComPtr<ID3D12PipelineState>> pipelineStateHandle_;

		Handle<RootSignature> toneMappingRootSignature_;

		RootSignature& rootSignature_;
		PipelineStateObject& pipelineStateObject_;
	};
}
