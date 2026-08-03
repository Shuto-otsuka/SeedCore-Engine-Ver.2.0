#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/Utility/Handle.h>
#include <GraphicsEngine/D3D12/PipelineState/RootSignature.h>
#include <GraphicsEngine/D3D12/PipelineState/PipelineStateObject.h>

namespace SeedCore
{
	class ShaderCache;

	class AmplificationShader;
	class MeshShader;
	class PixelShader;

	class MovieShader
	{
	public:
		MovieShader(RootSignature& rootSignature, PipelineStateObject& pipelineStateObject);
		~MovieShader() = default;

		void Create(ShaderCache& shaderCache, ID3D12Device* device);

		[[nodiscard]] ID3D12PipelineState* GetPipelineStateSprite()const;

		[[nodiscard]] ID3D12PipelineState* GetPipelineStateBillboard()const;

		[[nodiscard]] ID3D12PipelineState* GetPipelineStateFullscreen()const;

		[[nodiscard]] ID3D12PipelineState* GetPipelineStateSelectionMaskSprite()const;

		[[nodiscard]] ID3D12PipelineState* GetPipelineStateSelectionMaskBillboard()const;

		[[nodiscard]] ID3D12RootSignature* GetRootSignature()const;

	private:
		Handle<AmplificationShader> spriteAmplificationShader_;
		Handle<MeshShader> spriteMeshShader_;
		Handle<PixelShader> spritePixelShader_;

		Handle<Microsoft::WRL::ComPtr<ID3D12PipelineState>> pipelineStateObjectSprite_;

		Handle<AmplificationShader> billboardAmplificationShader_;
		Handle<MeshShader> billboardMeshShader_;
		Handle<PixelShader> billboardPixelShader_;

		Handle<Microsoft::WRL::ComPtr<ID3D12PipelineState>> pipelineStateObjectBillboard_;

		Handle<AmplificationShader> fullscreenAmplificationShader_;
		Handle<MeshShader> fullscreenMeshShader_;
		Handle<PixelShader> fullscreenPixelShader_;

		Handle<Microsoft::WRL::ComPtr<ID3D12PipelineState>> pipelineStateObjectFullscreen_;

		Handle<AmplificationShader> spriteSelectionAmplificationShader_;
		Handle<AmplificationShader> billboardSelectionAmplificationShader_;
		Handle<PixelShader> selectionMaskPixelShader_;

		Handle<Microsoft::WRL::ComPtr<ID3D12PipelineState>> pipelineStateObjectSelectionMaskSprite_;
		Handle<Microsoft::WRL::ComPtr<ID3D12PipelineState>> pipelineStateObjectSelectionMaskBillboard_;

		Handle<RootSignature> movieRootSignature_;

		RootSignature& rootSignature_;
		PipelineStateObject& pipelineStateObject_;
	};
}
