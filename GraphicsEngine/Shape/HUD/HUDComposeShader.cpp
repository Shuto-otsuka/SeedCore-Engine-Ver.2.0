#include <GraphicsEngine/Shape/HUD/HUDComposeShader.h>
#include <GraphicsEngine/Shader/ShaderCache.h>
#include <GraphicsEngine/D3D12/PipelineState/MeshShader.h>
#include <GraphicsEngine/D3D12/PipelineState/PixelShader.h>
#include <GraphicsEngine/D3D12/PipelineState/RasterizerState.h>
#include <GraphicsEngine/D3D12/PipelineState/BlendState.h>
#include <GraphicsEngine/D3D12/PipelineState/DepthStencilState.h>

namespace SeedCore
{
	HUDComposeShader::HUDComposeShader(RootSignature& rootSignature, PipelineStateObject& pipelineStateObject) : rootSignature_(rootSignature), pipelineStateObject_(pipelineStateObject)
	{
		/// No Code
	}

	void HUDComposeShader::Create(ShaderCache& shaderCache, ID3D12Device* device)
	{
		hudComposeRootSignature_ = rootSignature_.GetOrCreate(device);

		compositeMeshShader_ = shaderCache.GetOrCreateMeshShader(String("../GraphicsEngine/Shape/HUD/HUDComposeMS.hlsl"));
		compositePixelShader_ = shaderCache.GetOrCreatePixelShader(String("../GraphicsEngine/Shape/HUD/HUDComposePS.hlsl"));

		PipelineStateKey psokey{};
		memset(&psokey, 0, sizeof(psokey));
		psokey.rootSignature_ = rootSignature_.Get(hudComposeRootSignature_)->Get();
		psokey.meshShader_ = shaderCache.GetMeshShader(compositeMeshShader_)->Bytecode();
		psokey.pixelShader_ = shaderCache.GetPixelShader(compositePixelShader_)->Bytecode();
		psokey.rasterizerDesc_ = RasterizerState::Get(RasterizerStateType::SolidNoneLHS);
		psokey.blendDesc_ = BlendState::Get(BlendStateType::AlphaPremultiplied);
		psokey.depthStencilDesc_ = DepthStencilState::Get(DepthStencilStateType::DepthOff);
		psokey.renderTargetViewFormat_[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		psokey.renderTargetViewCount_ = 1;
		psokey.depthStencilViewFormat_ = DXGI_FORMAT_UNKNOWN;
		psokey.primitiveTopologyType_ = D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED;
		pipelineStateObjectComposite_ = pipelineStateObject_.GetOrCreate(device, psokey);
	}

	ID3D12PipelineState* HUDComposeShader::GetPipelineStateComposite()const
	{
		return pipelineStateObject_.Get(pipelineStateObjectComposite_);
	}

	ID3D12RootSignature* HUDComposeShader::GetRootSignature()const
	{
		return rootSignature_.Get(hudComposeRootSignature_)->Get();
	}
}
