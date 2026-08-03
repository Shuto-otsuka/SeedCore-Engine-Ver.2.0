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
	* Manages PSO creation for the three froxel volumetric passes:
	* FogInjectionCS.hlsl (medium injection), VolumetricLightScatteringRT.hlsl
	* (sun occlusion via inline RayQuery + cloud lightmarch = god rays) and
	* FroxelIntegrationCS.hlsl (front-to-back scan). Same shared root
	* signature as every other pass.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* Froxel ボリューメトリクス3パスの PSO 管理:
	* FogInjectionCS.hlsl(媒質注入)、VolumetricLightScatteringRT.hlsl
	* (インライン RayQuery の太陽遮蔽+雲ライトマーチ=ゴッドレイ)、
	* FroxelIntegrationCS.hlsl(front-to-back 積分)。他の全パスと同じ共有
	* ルートシグネチャ。
	*/
	class VolumetricLightShader
	{
	public:
		VolumetricLightShader(RootSignature& rootSignature, PipelineStateObject& pipelineStateObject);
		~VolumetricLightShader() = default;

		void Create(ShaderCache& shaderCache, ID3D12Device* device);

		[[nodiscard]] ID3D12PipelineState* GetInjectionPipelineState()const;

		[[nodiscard]] ID3D12PipelineState* GetScatteringPipelineState()const;

		[[nodiscard]] ID3D12PipelineState* GetIntegrationPipelineState()const;

		[[nodiscard]] ID3D12RootSignature* GetRootSignature()const;

	private:
		Handle<ComputeShader> injectionShader_;
		Handle<Microsoft::WRL::ComPtr<ID3D12PipelineState>> injectionPipelineStateHandle_;

		Handle<ComputeShader> scatteringShader_;
		Handle<Microsoft::WRL::ComPtr<ID3D12PipelineState>> scatteringPipelineStateHandle_;

		Handle<ComputeShader> integrationShader_;
		Handle<Microsoft::WRL::ComPtr<ID3D12PipelineState>> integrationPipelineStateHandle_;

		Handle<RootSignature> volumetricLightRootSignature_;

		RootSignature& rootSignature_;
		PipelineStateObject& pipelineStateObject_;
	};
}
