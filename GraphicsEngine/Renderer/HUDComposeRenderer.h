#pragma once
#include <FoundationEngine/Prelude.h>
#include <GraphicsEngine/Shape/HUD/HUDComposeShader.h>

namespace SeedCore
{
	struct RootAddresses;
	class ShaderCache;
	class BindlessHeap;
	class D3D12CommandList;

	class HUDComposeRenderer
	{
	public:
		HUDComposeRenderer(RootSignature& rootSignature, PipelineStateObject& pipelineStateObject);
		~HUDComposeRenderer() = default;

		void Create(ID3D12Device* device, BindlessHeap* bindlessHeap, ShaderCache& shaderCache);

		void Draw(D3D12CommandList* cmdList, D3D12_CPU_DESCRIPTOR_HANDLE renderTargetView, D3D12_VIEWPORT viewport, ID3D12DescriptorHeap* heap, const RootAddresses& addresses);

	private:
		HUDComposeShader hudComposeShader_;

		BindlessHeap* bindlessHeap_ = nullptr;
	};
}
