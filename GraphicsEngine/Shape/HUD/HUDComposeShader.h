#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/Utility/Handle.h>
#include <GraphicsEngine/D3D12/PipelineState/RootSignature.h>
#include <GraphicsEngine/D3D12/PipelineState/PipelineStateObject.h>

namespace SeedCore
{
	class ShaderCache;
	class MeshShader;
	class PixelShader;

	class HUDComposeShader
	{
	public:
		HUDComposeShader(RootSignature& rootSignature, PipelineStateObject& pipelineStateObject);
		~HUDComposeShader() = default;

		void Create(ShaderCache& shaderCache, ID3D12Device* device);

		[[nodiscard]] ID3D12PipelineState* GetPipelineStateComposite()const;

		[[nodiscard]] ID3D12RootSignature* GetRootSignature()const;

	private:
		Handle<MeshShader> compositeMeshShader_;
		Handle<PixelShader> compositePixelShader_;
		Handle<Microsoft::WRL::ComPtr<ID3D12PipelineState>> pipelineStateObjectComposite_;

		Handle<RootSignature> hudComposeRootSignature_;

		RootSignature& rootSignature_;
		PipelineStateObject& pipelineStateObject_;
	};
}
