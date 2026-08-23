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
	* Manages PSO creation for the reflection SVGF chain (ReflectionDenoiseCS.hlsl),
	* whose five passes all live in that one file behind different entry points
	* (the same "one file, many entry points" pattern ShadowDenoiseShader uses)
	* and share the same root signature as every other pass (see ReflectionShader):
	*   main            - dual (surface + hit-point virtual motion) temporal
	*                     reprojection + moment accumulation
	*   FilterMoments   - spatial variance estimate for short-history pixels
	*   ATrousPass1/2/3 - variance-guided wavelet iterations at step 1/2/4
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 反射の SVGF チェーン(ReflectionDenoiseCS.hlsl)の PSO 管理。5 つのパスは
	* すべて同一ファイル内の別エントリポイントに置かれており
	* (ShadowDenoiseShader と同じ「1ファイル複数エントリポイント」方式)、
	* 他の全パスと同じルートシグネチャを共有する(ReflectionShader 参照):
	*   main            - 二重(面+ヒット点仮想モーション)時間的リプロジェクション
	*                     + モーメント蓄積
	*   FilterMoments   - 履歴の短いピクセル向けの空間的分散推定
	*   ATrousPass1/2/3 - step 1/2/4 の分散誘導ウェーブレット反復
	*/
	class ReflectionDenoiseShader
	{
	public:
		ReflectionDenoiseShader(RootSignature& rootSignature, PipelineStateObject& pipelineStateObject);
		~ReflectionDenoiseShader() = default;

		void Create(ShaderCache& shaderCache, ID3D12Device* device);

		[[nodiscard]] ID3D12PipelineState* GetPipelineState()const;

		[[nodiscard]] ID3D12PipelineState* GetFilterMomentsPipelineState()const;

		[[nodiscard]] ID3D12PipelineState* GetATrousPipelineState(Uint32 passIndex)const;

		/// [EN] PSO for ReflectionReservoirSpatialCS.hlsl's ReSTIR spatial-reuse
		///      pass - a separate file (StructuredBuffer<ReflectionReservoir>
		///      reads instead of the RGBA float texture chain the rest of this
		///      class denoises) but the same shared root signature, so it lives
		///      here rather than a whole new shader class (same pattern as
		///      GlobalIlluminationDenoiseShader::GetSpatialReusePipelineState).
		/// [JP] ReflectionReservoirSpatialCS.hlsl の ReSTIR 空間的リユースパス用
		///      PSO - このクラスの他パスが扱う RGBA float テクスチャチェーンと
		///      違い StructuredBuffer<ReflectionReservoir> を読む別ファイルだが、
		///      共有ルートシグネチャは同じなので、新しいシェーダクラスを作らず
		///      ここに置く(GlobalIlluminationDenoiseShader::
		///      GetSpatialReusePipelineState と同じパターン)。
		[[nodiscard]] ID3D12PipelineState* GetSpatialReusePipelineState()const;

		[[nodiscard]] ID3D12RootSignature* GetRootSignature()const;

	private:
		static constexpr Uint32 atrousPassCount = 3;

		Handle<ComputeShader> computeShader_;
		Handle<Microsoft::WRL::ComPtr<ID3D12PipelineState>> pipelineStateObjectHandle_;

		Handle<ComputeShader> filterMomentsComputeShader_;
		Handle<Microsoft::WRL::ComPtr<ID3D12PipelineState>> filterMomentsPipelineStateObjectHandle_;

		Handle<ComputeShader> atrousComputeShader_[atrousPassCount];
		Handle<Microsoft::WRL::ComPtr<ID3D12PipelineState>> atrousPipelineStateObjectHandle_[atrousPassCount];

		Handle<ComputeShader> spatialReuseComputeShader_;
		Handle<Microsoft::WRL::ComPtr<ID3D12PipelineState>> spatialReusePipelineStateObjectHandle_;

		Handle<RootSignature> denoiseRootSignature_;

		RootSignature& rootSignature_;
		PipelineStateObject& pipelineStateObject_;
	};
}
