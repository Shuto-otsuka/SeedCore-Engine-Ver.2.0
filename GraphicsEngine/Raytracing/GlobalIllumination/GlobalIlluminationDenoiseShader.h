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
	* Manages PSO creation for the GI spatio-temporal denoise compute pass
	* (GlobalIlluminationDenoiseCS.hlsl): reprojects the previous frame's
	* accumulated radiance via the G-Buffer velocity and blends it with this
	* frame's raw noisy GlobalIlluminationRT.hlsl output. Same shared root
	* signature as every other pass (see GlobalIlluminationShader).
	*
	* Also owns the 3 A-Trous wavelet pass PSOs (ATrousPass1/2/3), compiled
	* from the SAME GlobalIlluminationDenoiseCS.hlsl file at different entry
	* points (ShaderCache::GetOrCreateComputeShader takes an entry point name,
	* so one .hlsl file can back several distinct PSOs without a root
	* signature change or a per-pass constant buffer - see that file's
	* comments). GlobalIlluminationRenderer::Dispatch runs the temporal-blend
	* PSO once, then the 3 A-Trous PSOs in order, per view per frame.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* GIの空間+時間デノイズコンピュートパス(GlobalIlluminationDenoiseCS.hlsl)
	* の PSO 管理。G-Buffer の速度バッファで前フレームの蓄積放射輝度を
	* リプロジェクションし、今フレームの GlobalIlluminationRT.hlsl の生ノイズ
	* 出力とブレンドする。他の全パスと同じ共有ルートシグネチャ
	* (GlobalIlluminationShader 参照)。
	*
	* 併せて 3 つの A-Trous ウェーブレットパス PSO(ATrousPass1/2/3)も持つ。
	* 同じ GlobalIlluminationDenoiseCS.hlsl ファイルの別エントリポイントから
	* コンパイルする(ShaderCache::GetOrCreateComputeShader はエントリポイント名を
	* 取れるので、ルートシグネチャ変更やパスごとの定数バッファ無しに1つの
	* .hlsl ファイルで複数の PSO を賄える — 詳細は同ファイルのコメント参照)。
	* GlobalIlluminationRenderer::Dispatch がビューごとに毎フレーム、時間ブレンド
	* PSO を1回、続けて A-Trous PSO を3つ順番に実行する。
	*/
	class GlobalIlluminationDenoiseShader
	{
	public:
		GlobalIlluminationDenoiseShader(RootSignature& rootSignature, PipelineStateObject& pipelineStateObject);
		~GlobalIlluminationDenoiseShader() = default;

		void Create(ShaderCache& shaderCache, ID3D12Device* device);

		[[nodiscard]] ID3D12PipelineState* GetPipelineState()const;

		/// [EN] passIndex is 0/1/2 for ATrousPass1/2/3 respectively.
		/// [JP] passIndex は 0/1/2 でそれぞれ ATrousPass1/2/3 に対応する。
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
