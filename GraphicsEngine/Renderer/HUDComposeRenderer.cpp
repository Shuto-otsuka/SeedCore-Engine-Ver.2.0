#include <GraphicsEngine/Renderer/HUDComposeRenderer.h>
#include <GraphicsEngine/Profiler/ProfilerStats.h>
#include <GraphicsEngine/D3D12/Descriptor/BindlessHeap.h>
#include <GraphicsEngine/D3D12/Context/D3D12CommandList.h>

namespace SeedCore
{
	HUDComposeRenderer::HUDComposeRenderer(RootSignature& rootSignature, PipelineStateObject& pipelineStateObject) : hudComposeShader_(rootSignature, pipelineStateObject)
	{
		/// No Code
	}

	void HUDComposeRenderer::Create(ID3D12Device* device, BindlessHeap* bindlessHeap, ShaderCache& shaderCache)
	{
		bindlessHeap_ = bindlessHeap;
		hudComposeShader_.Create(shaderCache, device);
	}

	void HUDComposeRenderer::Draw(D3D12CommandList* cmdList, D3D12_CPU_DESCRIPTOR_HANDLE renderTargetView, D3D12_VIEWPORT viewport, ID3D12DescriptorHeap* heap, D3D12_GPU_VIRTUAL_ADDRESS constantIndex, D3D12_GPU_VIRTUAL_ADDRESS structuredIndex)
	{
		auto* cmd = cmdList->Get();

		cmd->OMSetRenderTargets(1, &renderTargetView, FALSE, nullptr);
		cmd->RSSetViewports(1, &viewport);
		D3D12_RECT scissorRect = { 0, 0, static_cast<LONG>(viewport.Width), static_cast<LONG>(viewport.Height) };
		cmd->RSSetScissorRects(1, &scissorRect);

		ID3D12DescriptorHeap* heaps[] = { heap };
		cmd->SetDescriptorHeaps(_countof(heaps), heaps);
		cmd->SetGraphicsRootSignature(hudComposeShader_.GetRootSignature());
		cmd->SetGraphicsRootConstantBufferView(2, constantIndex);
		cmd->SetGraphicsRootConstantBufferView(3, structuredIndex);
		cmd->SetGraphicsRootDescriptorTable(0, bindlessHeap_->GPUHandle(0));

		cmd->SetPipelineState(hudComposeShader_.GetPipelineStateComposite());
		cmd->DispatchMesh(1, 1, 1);
		ProfilerStats::AddDrawCall();
	}
}
