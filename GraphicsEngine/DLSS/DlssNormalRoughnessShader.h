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
	* Manages PSO creation for the DLSS-RR normal+roughness synthesis compute
	* pass (DlssNormalRoughnessCS.hlsl): decodes the G-Buffer's packed RT1
	* (octahedral normal + roughness) into the plain RGB=normal/A=roughness
	* buffer NVIDIA DLSS Ray Reconstruction expects. Same shared root
	* signature as every other compute pass.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* DLSS-RR用の法線+ラフネス合成コンピュートパス(DlssNormalRoughnessCS.hlsl)
	* の PSO 管理。G-Buffer のパック済み RT1(八面体法線+ラフネス)を、
	* NVIDIA DLSS Ray Reconstruction が要求する素の RGB=法線/A=ラフネス
	* バッファへデコードする。他の全コンピュートパスと同じ共有ルート
	* シグネチャ。
	*/
	class DlssNormalRoughnessShader
	{
	public:
		DlssNormalRoughnessShader(RootSignature& rootSignature, PipelineStateObject& pipelineStateObject);
		~DlssNormalRoughnessShader() = default;

		void Create(ShaderCache& shaderCache, ID3D12Device* device);

		[[nodiscard]] ID3D12PipelineState* GetPipelineState()const;

		[[nodiscard]] ID3D12RootSignature* GetRootSignature()const;

	private:
		Handle<ComputeShader> computeShader_;
		Handle<Microsoft::WRL::ComPtr<ID3D12PipelineState>> pipelineStateObjectHandle_;

		Handle<RootSignature> normalRoughnessRootSignature_;

		RootSignature& rootSignature_;
		PipelineStateObject& pipelineStateObject_;
	};
}
