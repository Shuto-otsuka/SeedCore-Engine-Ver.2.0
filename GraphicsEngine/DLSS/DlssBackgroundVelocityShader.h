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
	* Manages PSO creation for the DLSS-RR background (sky/cloud) velocity
	* patch compute pass (DlssBackgroundVelocityCS.hlsl). Same shared root
	* signature as every other compute pass.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* DLSS-RR用の背景(空/雲)速度パッチコンピュートパス
	* (DlssBackgroundVelocityCS.hlsl)の PSO 管理。他の全コンピュートパスと
	* 同じ共有ルートシグネチャ。
	*/
	class DlssBackgroundVelocityShader
	{
	public:
		DlssBackgroundVelocityShader(RootSignature& rootSignature, PipelineStateObject& pipelineStateObject);
		~DlssBackgroundVelocityShader() = default;

		void Create(ShaderCache& shaderCache, ID3D12Device* device);

		[[nodiscard]] ID3D12PipelineState* GetPipelineState()const;

		[[nodiscard]] ID3D12RootSignature* GetRootSignature()const;

	private:
		Handle<ComputeShader> computeShader_;
		Handle<Microsoft::WRL::ComPtr<ID3D12PipelineState>> pipelineStateObjectHandle_;

		Handle<RootSignature> backgroundVelocityRootSignature_;

		RootSignature& rootSignature_;
		PipelineStateObject& pipelineStateObject_;
	};
}
