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
	* Manages PSO creation for the shadow temporal-accumulation compute pass
	* (ShadowDenoiseCS.hlsl): reprojects the previous frame's accumulated
	* visibility via the G-Buffer velocity and blends it with this frame's raw
	* noisy ShadowRT.hlsl output. Same shared root signature as every other
	* pass (see ShadowShader).
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* シャドウの時間積分コンピュートパス(ShadowDenoiseCS.hlsl)の PSO 管理。
	* G-Buffer の速度バッファで前フレームの蓄積可視性をリプロジェクションし、
	* 今フレームの ShadowRT.hlsl の生ノイズ出力とブレンドする。他の全パスと
	* 同じ共有ルートシグネチャ(ShadowShader 参照)。
	*/
	class ShadowDenoiseShader
	{
	public:
		ShadowDenoiseShader(RootSignature& rootSignature, PipelineStateObject& pipelineStateObject);
		~ShadowDenoiseShader() = default;

		void Create(ShaderCache& shaderCache, ID3D12Device* device);

		[[nodiscard]] ID3D12PipelineState* GetPipelineState()const;

		[[nodiscard]] ID3D12RootSignature* GetRootSignature()const;

	private:
		Handle<ComputeShader> computeShader_;
		Handle<Microsoft::WRL::ComPtr<ID3D12PipelineState>> pipelineStateObjectHandle_;

		Handle<RootSignature> denoiseRootSignature_;

		RootSignature& rootSignature_;
		PipelineStateObject& pipelineStateObject_;
	};
}
