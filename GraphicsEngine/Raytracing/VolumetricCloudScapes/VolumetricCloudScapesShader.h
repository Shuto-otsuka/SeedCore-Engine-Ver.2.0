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
	* Manages PSO creation for the volumetric cloud scapes compute pass
	* (VolumetricCloudScapesRT.hlsl — screen-space raymarch, one thread per
	* screen pixel). Uses the same shared root signature as every other pass
	* (bindless table + the engine's two indices CBVs), same as ShadowShader.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* ボリューメトリック・クラウドスケープ用コンピュートパス
	* (VolumetricCloudScapesRT.hlsl — スクリーン空間レイマーチ、1 ピクセル
	* 1 スレッド)の PSO 管理。他の全パスと同じ共有ルートシグネチャ
	* （bindless テーブル＋エンジン共通の 2 つの indices CBV）を使う。
	* ShadowShader と同じ構成。
	*/
	class VolumetricCloudScapesShader
	{
	public:
		VolumetricCloudScapesShader(RootSignature& rootSignature, PipelineStateObject& pipelineStateObject);
		~VolumetricCloudScapesShader() = default;

		void Create(ShaderCache& shaderCache, ID3D12Device* device);

		[[nodiscard]] ID3D12PipelineState* GetPipelineState()const;

		/// [EN] One-time noise bake passes (CloudNoiseShape/DetailBakeCS.hlsl).
		/// [JP] 一度だけ実行するノイズベイクパス(CloudNoiseShape/DetailBakeCS.hlsl)。
		[[nodiscard]] ID3D12PipelineState* GetShapeBakePipelineState()const;

		[[nodiscard]] ID3D12PipelineState* GetDetailBakePipelineState()const;

		[[nodiscard]] ID3D12RootSignature* GetRootSignature()const;

	private:
		Handle<ComputeShader> computeShader_;
		Handle<Microsoft::WRL::ComPtr<ID3D12PipelineState>> pipelineStateObjectHandle_;

		Handle<ComputeShader> shapeBakeShader_;
		Handle<Microsoft::WRL::ComPtr<ID3D12PipelineState>> shapeBakePipelineStateHandle_;

		Handle<ComputeShader> detailBakeShader_;
		Handle<Microsoft::WRL::ComPtr<ID3D12PipelineState>> detailBakePipelineStateHandle_;

		Handle<RootSignature> cloudRootSignature_;

		RootSignature& rootSignature_;
		PipelineStateObject& pipelineStateObject_;
	};
}
