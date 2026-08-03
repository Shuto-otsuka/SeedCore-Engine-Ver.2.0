#include <GraphicsEngine/Renderer/OutlineRenderer.h>
#include <GraphicsEngine/Profiler/ProfilerStats.h>
#include <GraphicsEngine/D3D12/Descriptor/BindlessHeap.h>
#include <GraphicsEngine/D3D12/Context/D3D12CommandList.h>
#include <GraphicsEngine/D3D12/Buffer/FrameBuffer.h>

namespace SeedCore
{
	OutlineRenderer::OutlineRenderer(RootSignature& rootSignature, PipelineStateObject& pipelineStateObject) : outlineShader_(rootSignature, pipelineStateObject)
	{
		/// No Code
	}

	void OutlineRenderer::Create(ID3D12Device* device, BindlessHeap* bindlessHeap, ShaderCache& shaderCache)
	{
		bindlessHeap_ = bindlessHeap;
		outlineShader_.Create(shaderCache, device);
	}

	void OutlineRenderer::Draw(D3D12CommandList* cmdList, FrameBuffer* frameBuffer, ID3D12DescriptorHeap* heap, D3D12_GPU_VIRTUAL_ADDRESS constantIndex, D3D12_GPU_VIRTUAL_ADDRESS structuredIndex)
	{
		/// [JP] マスクが全て0でもエッジ検出PS側が全ピクセルdiscardするだけで安全
		///      なので、選択の有無に関わらず常に実行し frameBuffer を確実に
		///      Rebind する（呼び出し元がこの直後に別の描画をこの frameBuffer へ
		///      続けるため）。
		frameBuffer->Rebind(cmdList);

		auto* cmd = cmdList->Get();

		ID3D12DescriptorHeap* heaps[] = { heap };
		cmd->SetDescriptorHeaps(_countof(heaps), heaps);
		cmd->SetGraphicsRootSignature(outlineShader_.GetRootSignature());
		cmd->SetGraphicsRootConstantBufferView(2, constantIndex);
		cmd->SetGraphicsRootConstantBufferView(3, structuredIndex);
		cmd->SetGraphicsRootDescriptorTable(0, bindlessHeap_->GPUHandle(0));

		cmd->SetPipelineState(outlineShader_.GetPipelineStateComposite());
		cmd->DispatchMesh(1, 1, 1);
		ProfilerStats::AddDrawCall();
	}
}
