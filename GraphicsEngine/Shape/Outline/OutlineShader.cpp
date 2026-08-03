#include <GraphicsEngine/Shape/Outline/OutlineShader.h>
#include <GraphicsEngine/Shader/ShaderCache.h>
#include <GraphicsEngine/D3D12/PipelineState/MeshShader.h>
#include <GraphicsEngine/D3D12/PipelineState/PixelShader.h>
#include <GraphicsEngine/D3D12/PipelineState/RasterizerState.h>
#include <GraphicsEngine/D3D12/PipelineState/BlendState.h>
#include <GraphicsEngine/D3D12/PipelineState/DepthStencilState.h>

namespace SeedCore
{
	OutlineShader::OutlineShader(RootSignature& rootSignature, PipelineStateObject& pipelineStateObject) : rootSignature_(rootSignature), pipelineStateObject_(pipelineStateObject)
	{
		/// No Code
	}

	void OutlineShader::Create(ShaderCache& shaderCache, ID3D12Device* device)
	{
		outlineRootSignature_ = rootSignature_.GetOrCreate(device);

		/// [EN] Selection outline composite PSO: fullscreen, edge-detects the
		///      selection mask and writes the outline color onto the target frame
		///      buffer (non-edge pixels discard in the PS, leaving it untouched).
		/// [JP] 選択アウトライン合成 PSO: フルスクリーンで選択マスクをエッジ検出し、
		///      縁取り色を対象フレームバッファへ書く（エッジ以外は PS 内で discard
		///      し、既存内容を残す）。
		compositeMeshShader_ = shaderCache.GetOrCreateMeshShader(String("../GraphicsEngine/Shape/Outline/SelectionOutlineMS.hlsl"));
		compositePixelShader_ = shaderCache.GetOrCreatePixelShader(String("../GraphicsEngine/Shape/Outline/SelectionOutlinePS.hlsl"));

		PipelineStateKey psokey{};
		memset(&psokey, 0, sizeof(psokey));
		psokey.rootSignature_ = rootSignature_.Get(outlineRootSignature_)->Get();
		psokey.meshShader_ = shaderCache.GetMeshShader(compositeMeshShader_)->Bytecode();
		psokey.pixelShader_ = shaderCache.GetPixelShader(compositePixelShader_)->Bytecode();
		psokey.rasterizerDesc_ = RasterizerState::Get(RasterizerStateType::SolidNoneLHS);
		psokey.blendDesc_ = BlendState::Get(BlendStateType::Opaque);
		psokey.depthStencilDesc_ = DepthStencilState::Get(DepthStencilStateType::DepthOff);
		psokey.renderTargetViewFormat_[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
		psokey.renderTargetViewCount_ = 1;
		psokey.depthStencilViewFormat_ = DXGI_FORMAT_UNKNOWN;
		psokey.primitiveTopologyType_ = D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED;
		pipelineStateObjectComposite_ = pipelineStateObject_.GetOrCreate(device, psokey);
	}

	ID3D12PipelineState* OutlineShader::GetPipelineStateComposite()const
	{
		return pipelineStateObject_.Get(pipelineStateObjectComposite_);
	}

	ID3D12RootSignature* OutlineShader::GetRootSignature()const
	{
		return rootSignature_.Get(outlineRootSignature_)->Get();
	}
}
