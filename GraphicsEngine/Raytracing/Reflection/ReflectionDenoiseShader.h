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
	*/
	class ReflectionDenoiseShader
	{
	public:
		ReflectionDenoiseShader(RootSignature& rootSignature, PipelineStateObject& pipelineStateObject);
		~ReflectionDenoiseShader() = default;

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
