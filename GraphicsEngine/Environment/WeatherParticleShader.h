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
	* Manages PSOs for the weather particle system: one compute PSO
	* (WeatherParticleSimulateCS.hlsl - advances/respawns particles, shared by
	* both the rain and snow buffers) and one AS+MS+PS graphics PSO
	* (WeatherParticleAS/MS/PS.hlsl - frustum-culls then expands each particle
	* into a camera-facing quad, stretched along velocity for rain). The
	* graphics PSO mirrors ImageShader's billboard PSO (same AS+MS+PS shape),
	* but with additive blending and depth TEST-only (no write) so particles
	* are correctly occluded by the existing G-buffer depth - that per-pixel
	* hardware depth test is what makes particles "collide" with models.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 天候パーティクル系の PSO 管理: コンピュート PSO 1つ
	* (WeatherParticleSimulateCS.hlsl - パーティクルの進行/再スポーン、雨/雪
	* 両バッファ共有)と、AS+MS+PS のグラフィックス PSO
	* (WeatherParticleAS/MS/PS.hlsl - フラスタムカリング後、各パーティクルを
	* カメラ向きクアッドへ展開、雨は速度方向へストレッチ)。グラフィックス
	* PSO は ImageShader のビルボード PSO と同じ形(AS+MS+PS)だが、加算合成 +
	* 深度テストのみ(書込み無し)にしている点が異なる - この画素単位の
	* ハードウェア深度テストが、パーティクルとモデルの「衝突」を実現する。
	*/
	class WeatherParticleShader
	{
	public:
		WeatherParticleShader(RootSignature& rootSignature, PipelineStateObject& pipelineStateObject);
		~WeatherParticleShader() = default;

		void Create(ShaderCache& shaderCache, ID3D12Device* device);

		[[nodiscard]] ID3D12PipelineState* GetSimulatePipelineState()const;

		[[nodiscard]] ID3D12PipelineState* GetDrawPipelineState()const;

		[[nodiscard]] ID3D12RootSignature* GetRootSignature()const;

	private:
		Handle<ComputeShader> simulateShader_;
		Handle<Microsoft::WRL::ComPtr<ID3D12PipelineState>> simulatePipelineStateHandle_;

		Handle<Microsoft::WRL::ComPtr<ID3D12PipelineState>> drawPipelineStateHandle_;

		Handle<RootSignature> particleRootSignature_;

		RootSignature& rootSignature_;
		PipelineStateObject& pipelineStateObject_;
	};
}
