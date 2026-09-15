#include <GraphicsEngine/Shape/Collider/ColliderLineShader.h>
#include <GraphicsEngine/Shader/ShaderCache.h>
#include <GraphicsEngine/D3D12/Context/D3D12Check.h>
#include <GraphicsEngine/D3D12/PipelineState/VertexShader.h>
#include <GraphicsEngine/D3D12/PipelineState/MeshShader.h>
#include <GraphicsEngine/D3D12/PipelineState/PixelShader.h>
#include <GraphicsEngine/D3D12/PipelineState/RasterizerState.h>
#include <GraphicsEngine/D3D12/PipelineState/BlendState.h>

namespace SeedCore
{
	ColliderLineShader::ColliderLineShader(RootSignature& rootSignature, PipelineStateObject& pipelineStateObject) : rootSignature_(rootSignature), pipelineStateObject_(pipelineStateObject)
	{
		/// No Code
	}

	void ColliderLineShader::Create(ShaderCache& shaderCache, ID3D12Device* device, DepthStencilStateType depthStencilStateType)
	{
		lineRootSignature_ = rootSignature_.GetOrCreate(device);

		linePixelShader_ = shaderCache.GetOrCreatePixelShader(String("../GraphicsEngine/Shape/Collider/ColliderLinePS.hlsl"));

		PipelineStateKey psoKey{};
		memset(&psoKey, 0, sizeof(psoKey));
		psoKey.rootSignature_ = rootSignature_.Get(lineRootSignature_)->Get();
		if (D3D12Check::GetLevel() == D3D12Level::D12_2)
		{
			lineMeshShader_ = shaderCache.GetOrCreateMeshShader(String("../GraphicsEngine/Shape/Collider/ColliderLineMS.hlsl"));
			psoKey.meshShader_ = shaderCache.GetMeshShader(lineMeshShader_)->Bytecode();
			psoKey.primitiveTopologyType_ = D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED;
		}
		else
		{
			lineVertexShader_ = shaderCache.GetOrCreateVertexShader(String("../GraphicsEngine/Shape/Collider/ColliderLineVS.hlsl"));
			psoKey.vertexShader_ = shaderCache.GetVertexShader(lineVertexShader_)->Bytecode();
			psoKey.primitiveTopologyType_ = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
		}
		psoKey.pixelShader_ = shaderCache.GetPixelShader(linePixelShader_)->Bytecode();
		psoKey.rasterizerDesc_ = RasterizerState::Get(RasterizerStateType::SolidNoneLHS);
		psoKey.blendDesc_ = BlendState::Get(BlendStateType::Opaque);
		psoKey.depthStencilDesc_ = DepthStencilState::Get(depthStencilStateType);
		psoKey.renderTargetViewFormat_[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
		psoKey.renderTargetViewCount_ = 1;
		psoKey.depthStencilViewFormat_ = DXGI_FORMAT_D32_FLOAT;
		pipelineState_ = pipelineStateObject_.GetOrCreate(device, psoKey);
	}

	ID3D12PipelineState* ColliderLineShader::GetPipelineState()const
	{
		return pipelineStateObject_.Get(pipelineState_);
	}

	ID3D12RootSignature* ColliderLineShader::GetRootSignature()const
	{
		return rootSignature_.Get(lineRootSignature_)->Get();
	}
}
