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
	* Manages PSO creation for the AO temporal-accumulation compute pass
	* (AmbientOcclusionDenoiseCS.hlsl): reprojects the previous frame's
	* accumulated openness via the G-Buffer velocity and blends it with this
	* frame's raw noisy AmbientOcclusionRT.hlsl output. Same shared root
	* signature as every other pass (see AmbientOcclusionShader).
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* AOの時間積分コンピュートパス(AmbientOcclusionDenoiseCS.hlsl)の PSO 管理。
	* G-Buffer の速度バッファで前フレームの蓄積開放度をリプロジェクションし、
	* 今フレームの AmbientOcclusionRT.hlsl の生ノイズ出力とブレンドする。
	* 他の全パスと同じ共有ルートシグネチャ(AmbientOcclusionShader 参照)。
	*/
	class AmbientOcclusionDenoiseShader
	{
	public:
		AmbientOcclusionDenoiseShader(RootSignature& rootSignature, PipelineStateObject& pipelineStateObject);
		~AmbientOcclusionDenoiseShader() = default;

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
