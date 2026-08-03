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
	* Manages PSO creation for the volumetric star compute pass
	* (VolumetricStarRT.hlsl - screen-space, one thread per screen pixel). Uses
	* the same shared root signature as every other pass (bindless table + the
	* engine's two indices CBVs), same as VolumetricCloudScapesShader.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* ボリューメトリック・スター用コンピュートパス(VolumetricStarRT.hlsl -
	* スクリーン空間、1 ピクセル 1 スレッド)の PSO 管理。他の全パスと同じ
	* 共有ルートシグネチャ(bindless テーブル+エンジン共通の2つの indices
	* CBV)を使う。VolumetricCloudScapesShader と同じ構成。
	*/
	class VolumetricStarShader
	{
	public:
		VolumetricStarShader(RootSignature& rootSignature, PipelineStateObject& pipelineStateObject);
		~VolumetricStarShader() = default;

		void Create(ShaderCache& shaderCache, ID3D12Device* device);

		[[nodiscard]] ID3D12PipelineState* GetPipelineState()const;

		[[nodiscard]] ID3D12RootSignature* GetRootSignature()const;

	private:
		Handle<ComputeShader> computeShader_;
		Handle<Microsoft::WRL::ComPtr<ID3D12PipelineState>> pipelineStateObjectHandle_;

		Handle<RootSignature> starRootSignature_;

		RootSignature& rootSignature_;
		PipelineStateObject& pipelineStateObject_;
	};
}
