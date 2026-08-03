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
	* Manages PSO creation for the ray-traced shadow compute pass
	* (ShadowRT.hlsl — inline RayQuery, one thread per screen pixel). Uses the
	* same shared root signature as every other pass (bindless table + the
	* engine's two indices CBVs), same as LightSystem's ClusterAssignCS.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* レイトレシャドウ用コンピュートパス(ShadowRT.hlsl — インライン RayQuery、
	* 1 ピクセル 1 スレッド)の PSO 管理。他の全パスと同じ共有ルートシグネチャ
	* （bindless テーブル＋エンジン共通の 2 つの indices CBV）を使う。
	* LightSystem の ClusterAssignCS と同じ構成。
	*/
	class ShadowShader
	{
	public:
		ShadowShader(RootSignature& rootSignature, PipelineStateObject& pipelineStateObject);
		~ShadowShader() = default;

		void Create(ShaderCache& shaderCache, ID3D12Device* device);

		[[nodiscard]] ID3D12PipelineState* GetPipelineState()const;

		[[nodiscard]] ID3D12RootSignature* GetRootSignature()const;

	private:
		Handle<ComputeShader> computeShader_;
		Handle<Microsoft::WRL::ComPtr<ID3D12PipelineState>> pipelineStateObjectHandle_;

		Handle<RootSignature> shadowRootSignature_;

		RootSignature& rootSignature_;
		PipelineStateObject& pipelineStateObject_;
	};
}
