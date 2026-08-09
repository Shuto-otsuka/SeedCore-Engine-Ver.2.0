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
	* Manages PSO creation for the reflection spatio-temporal denoise compute
	* pass (ReflectionDenoiseCS.hlsl): reprojects the previous frame's
	* accumulated reflection radiance via the G-Buffer velocity and blends it
	* with this frame's raw noisy ReflectionRT.hlsl output. Same shared root
	* signature as every other pass (see ReflectionShader).
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 反射の空間+時間デノイズコンピュートパス(ReflectionDenoiseCS.hlsl)の
	* PSO 管理。G-Buffer の速度バッファで前フレームの蓄積反射放射輝度を
	* リプロジェクションし、今フレームの ReflectionRT.hlsl の生ノイズ出力と
	* ブレンドする。他の全パスと同じ共有ルートシグネチャ(ReflectionShader 参照)。
	*
	* 併せて 3 つの A-Trous ウェーブレットパス PSO(ATrousPass1/2/3)も持つ。
	* 同じ ReflectionDenoiseCS.hlsl ファイルの別エントリポイントからコンパイル
	* する、GlobalIlluminationDenoiseShader と同じ方式。
	*/
	class ReflectionDenoiseShader
	{
	public:
		ReflectionDenoiseShader(RootSignature& rootSignature, PipelineStateObject& pipelineStateObject);
		~ReflectionDenoiseShader() = default;

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
