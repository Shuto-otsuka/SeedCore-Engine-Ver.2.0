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
	* Also owns the 3 A-Trous wavelet pass PSOs (ATrousPass1/2/3), compiled
	* from the same ShadowDenoiseCS.hlsl file at different entry points, same
	* pattern as GlobalIlluminationDenoiseShader.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* シャドウの時間積分コンピュートパス(ShadowDenoiseCS.hlsl)の PSO 管理。
	* G-Buffer の速度バッファで前フレームの蓄積可視性をリプロジェクションし、
	* 今フレームの ShadowRT.hlsl の生ノイズ出力とブレンドする。他の全パスと
	* 同じ共有ルートシグネチャ(ShadowShader 参照)。
	*
	* 併せて 3 つの A-Trous ウェーブレットパス PSO(ATrousPass1/2/3)も持つ。
	* 同じ ShadowDenoiseCS.hlsl ファイルの別エントリポイントからコンパイルする、
	* GlobalIlluminationDenoiseShader と同じ方式。
	*/
	class ShadowDenoiseShader
	{
	public:
		ShadowDenoiseShader(RootSignature& rootSignature, PipelineStateObject& pipelineStateObject);
		~ShadowDenoiseShader() = default;

		void Create(ShaderCache& shaderCache, ID3D12Device* device);

		[[nodiscard]] ID3D12PipelineState* GetPipelineState()const;

		[[nodiscard]] ID3D12PipelineState* GetATrousPipelineState(Uint32 passIndex)const;

		[[nodiscard]] ID3D12RootSignature* GetRootSignature()const;

	private:
		static constexpr Uint32 atrousPassCount = 3;

		Handle<ComputeShader> computeShader_;
		Handle<Microsoft::WRL::ComPtr<ID3D12PipelineState>> pipelineStateObjectHandle_;

		Handle<ComputeShader> atrousComputeShader_[atrousPassCount];
		Handle<Microsoft::WRL::ComPtr<ID3D12PipelineState>> atrousPipelineStateObjectHandle_[atrousPassCount];

		Handle<RootSignature> denoiseRootSignature_;

		RootSignature& rootSignature_;
		PipelineStateObject& pipelineStateObject_;
	};
}
