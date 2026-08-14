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
	* Manages PSO creation for the reflection ReBLUR chain
	* (ReflectionDenoiseCS.hlsl), whose five passes all live in that one file
	* behind different entry points (the same "one file, many entry points"
	* pattern GlobalIlluminationDenoiseShader uses) and share the same root
	* signature as every other pass (see ReflectionShader):
	*   PrePass    - spatial reuse before any accumulation exists
	*   main       - temporal accumulation with surface + virtual motion
	*   HistoryFix - spatial reconstruction for pixels with no history
	*   Blur       - main spatial pass
	*   PostBlur   - final, wider spatial pass; its output is both the image and
	*                next frame's history
	*
	* GetSpatialPipelineState() indexes the three spatial passes in the order
	* they run.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 反射の ReBLUR チェーン(ReflectionDenoiseCS.hlsl)の PSO 管理。5 つのパスは
	* すべて同一ファイル内の別エントリポイントに置かれており
	* (GlobalIlluminationDenoiseShader と同じ「1ファイル複数エントリポイント」
	* 方式)、他の全パスと同じルートシグネチャを共有する(ReflectionShader 参照):
	*   PrePass    - 蓄積が存在する前に行う空間再利用
	*   main       - 面モーション + 仮想モーションによる時間的蓄積
	*   HistoryFix - 履歴を持たないピクセルの空間的再構成
	*   Blur       - 主空間パス
	*   PostBlur   - より広い最終空間パス。その出力が最終画かつ次フレームの履歴
	*
	* GetSpatialPipelineState() は 3 つの空間パスを実行順で添字付けする。
	*/
	class ReflectionDenoiseShader
	{
	public:
		ReflectionDenoiseShader(RootSignature& rootSignature, PipelineStateObject& pipelineStateObject);
		~ReflectionDenoiseShader() = default;

		void Create(ShaderCache& shaderCache, ID3D12Device* device);

		[[nodiscard]] ID3D12PipelineState* GetPipelineState()const;

		[[nodiscard]] ID3D12PipelineState* GetHistoryFixPipelineState()const;

		[[nodiscard]] ID3D12PipelineState* GetSpatialPipelineState(Uint32 passIndex)const;

		[[nodiscard]] ID3D12RootSignature* GetRootSignature()const;

	private:
		/// [EN] PrePass, Blur, PostBlur - the three passes that run the shared
		///      cross-bilateral kernel, in execution order.
		/// [JP] PrePass / Blur / PostBlur — 共通のクロスバイラテラルカーネルを
		///      回す 3 パス、実行順。
		static constexpr Uint32 spatialPassCount = 3;

		Handle<ComputeShader> computeShader_;
		Handle<Microsoft::WRL::ComPtr<ID3D12PipelineState>> pipelineStateObjectHandle_;

		Handle<ComputeShader> historyFixComputeShader_;
		Handle<Microsoft::WRL::ComPtr<ID3D12PipelineState>> historyFixPipelineStateObjectHandle_;

		Handle<ComputeShader> spatialComputeShader_[spatialPassCount];
		Handle<Microsoft::WRL::ComPtr<ID3D12PipelineState>> spatialPipelineStateObjectHandle_[spatialPassCount];

		Handle<RootSignature> denoiseRootSignature_;

		RootSignature& rootSignature_;
		PipelineStateObject& pipelineStateObject_;
	};
}
