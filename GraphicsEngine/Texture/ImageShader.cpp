#include <GraphicsEngine/Texture/ImageShader.h>
#include <GraphicsEngine/Shader/ShaderCache.h>
#include <GraphicsEngine/D3D12/Context/D3D12Check.h>
#include <GraphicsEngine/D3D12/PipelineState/VertexShader.h>
#include <GraphicsEngine/D3D12/PipelineState/AmplificationShader.h>
#include <GraphicsEngine/D3D12/PipelineState/MeshShader.h>
#include <GraphicsEngine/D3D12/PipelineState/PixelShader.h>
#include <GraphicsEngine/D3D12/PipelineState/RasterizerState.h>
#include <GraphicsEngine/D3D12/PipelineState/BlendState.h>
#include <GraphicsEngine/D3D12/PipelineState/DepthStencilState.h>

namespace SeedCore
{
	ImageShader::ImageShader(RootSignature& rootSignature, PipelineStateObject& pipelineStateObject) : rootSignature_(rootSignature), pipelineStateObject_(pipelineStateObject)
	{
		/// No Code
	}

	void ImageShader::Create(ShaderCache& shaderCache, ID3D12Device* device)
	{
		imageRootSignature_ = rootSignature_.GetOrCreate(device);

		{
			spritePixelShader_ = shaderCache.GetOrCreatePixelShader(String("../GraphicsEngine/Texture/ImageSpritePS.hlsl"));

			PipelineStateKey psokey{};
			memset(&psokey, 0, sizeof(psokey));
			psokey.rootSignature_ = rootSignature_.Get(imageRootSignature_)->Get();
			if (D3D12Check::GetLevel() == D3D12Level::D12_2)
			{
				spriteAmplificationShader_ = shaderCache.GetOrCreateAmplificationShader(String("../GraphicsEngine/Texture/ImageSpriteAS.hlsl"));
				spriteMeshShader_ = shaderCache.GetOrCreateMeshShader(String("../GraphicsEngine/Texture/ImageSpriteMS.hlsl"));
				psokey.amplificationShader_ = shaderCache.GetAmplificationShader(spriteAmplificationShader_)->Bytecode();
				psokey.meshShader_ = shaderCache.GetMeshShader(spriteMeshShader_)->Bytecode();
				psokey.primitiveTopologyType_ = D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED;
			}
			else
			{
				spriteVertexShader_ = shaderCache.GetOrCreateVertexShader(String("../GraphicsEngine/Texture/ImageSpriteVS.hlsl"));
				psokey.vertexShader_ = shaderCache.GetVertexShader(spriteVertexShader_)->Bytecode();
				psokey.primitiveTopologyType_ = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
			}
			psokey.pixelShader_ = shaderCache.GetPixelShader(spritePixelShader_)->Bytecode();
			psokey.rasterizerDesc_ = RasterizerState::Get(RasterizerStateType::SolidNoneLHS);
			psokey.blendDesc_ = BlendState::Get(BlendStateType::Alpha);
			psokey.depthStencilDesc_ = DepthStencilState::Get(DepthStencilStateType::DepthOff);
			psokey.renderTargetViewFormat_[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
			psokey.renderTargetViewCount_ = 1;
			psokey.depthStencilViewFormat_ = DXGI_FORMAT_D32_FLOAT;
			pipelineStateObjectSprite_ = pipelineStateObject_.GetOrCreate(device, psokey);
		}

		{
			billboardPixelShader_ = shaderCache.GetOrCreatePixelShader(String("../GraphicsEngine/Texture/ImageBillboardPS.hlsl"));

			PipelineStateKey psokey{};
			memset(&psokey, 0, sizeof(psokey));
			psokey.rootSignature_ = rootSignature_.Get(imageRootSignature_)->Get();
			if (D3D12Check::GetLevel() == D3D12Level::D12_2)
			{
				billboardAmplificationShader_ = shaderCache.GetOrCreateAmplificationShader(String("../GraphicsEngine/Texture/ImageBillboardAS.hlsl"));
				billboardMeshShader_ = shaderCache.GetOrCreateMeshShader(String("../GraphicsEngine/Texture/ImageBillboardMS.hlsl"));
				psokey.amplificationShader_ = shaderCache.GetAmplificationShader(billboardAmplificationShader_)->Bytecode();
				psokey.meshShader_ = shaderCache.GetMeshShader(billboardMeshShader_)->Bytecode();
				psokey.primitiveTopologyType_ = D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED;
			}
			else
			{
				billboardVertexShader_ = shaderCache.GetOrCreateVertexShader(String("../GraphicsEngine/Texture/ImageBillboardVS.hlsl"));
				psokey.vertexShader_ = shaderCache.GetVertexShader(billboardVertexShader_)->Bytecode();
				psokey.primitiveTopologyType_ = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
			}
			psokey.pixelShader_ = shaderCache.GetPixelShader(billboardPixelShader_)->Bytecode();
			psokey.rasterizerDesc_ = RasterizerState::Get(RasterizerStateType::SolidNoneLHS);
			psokey.blendDesc_ = BlendState::Get(BlendStateType::Alpha);
			psokey.depthStencilDesc_ = DepthStencilState::Get(DepthStencilStateType::DepthOff);
			psokey.renderTargetViewFormat_[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
			psokey.renderTargetViewCount_ = 1;
			psokey.depthStencilViewFormat_ = DXGI_FORMAT_D32_FLOAT;
			pipelineStateObjectBillboard_ = pipelineStateObject_.GetOrCreate(device, psokey);
		}

		/// [EN] Selection outline mask PSOs: depth off, same as the model mask
		///      (see ModelShader.cpp for why occlusion is intentionally ignored).
		/// [JP] 選択アウトラインマスク PSO: 深度オフ。モデル側マスクと同じ理由
		///      （遮蔽を意図的に無視する）は ModelShader.cpp のコメント参照。
		{
			selectionMaskPixelShader_ = shaderCache.GetOrCreatePixelShader(String("../GraphicsEngine/Texture/ImageSelectionMaskPS.hlsl"));

			PipelineStateKey psokey{};
			memset(&psokey, 0, sizeof(psokey));
			psokey.rootSignature_ = rootSignature_.Get(imageRootSignature_)->Get();
			if (D3D12Check::GetLevel() == D3D12Level::D12_2)
			{
				spriteSelectionAmplificationShader_ = shaderCache.GetOrCreateAmplificationShader(String("../GraphicsEngine/Texture/ImageSpriteSelectionAS.hlsl"));
				psokey.amplificationShader_ = shaderCache.GetAmplificationShader(spriteSelectionAmplificationShader_)->Bytecode();
				psokey.meshShader_ = shaderCache.GetMeshShader(spriteMeshShader_)->Bytecode();
				psokey.primitiveTopologyType_ = D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED;
			}
			else
			{
				spriteSelectionVertexShader_ = shaderCache.GetOrCreateVertexShader(String("../GraphicsEngine/Texture/ImageSpriteSelectionVS.hlsl"));
				psokey.vertexShader_ = shaderCache.GetVertexShader(spriteSelectionVertexShader_)->Bytecode();
				psokey.primitiveTopologyType_ = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
			}
			psokey.pixelShader_ = shaderCache.GetPixelShader(selectionMaskPixelShader_)->Bytecode();
			psokey.rasterizerDesc_ = RasterizerState::Get(RasterizerStateType::SolidNoneLHS);
			psokey.blendDesc_ = BlendState::Get(BlendStateType::Opaque);
			psokey.depthStencilDesc_ = DepthStencilState::Get(DepthStencilStateType::DepthOff);
			psokey.renderTargetViewFormat_[0] = DXGI_FORMAT_R8_UNORM;
			psokey.renderTargetViewCount_ = 1;
			psokey.depthStencilViewFormat_ = DXGI_FORMAT_UNKNOWN;
			pipelineStateObjectSelectionMaskSprite_ = pipelineStateObject_.GetOrCreate(device, psokey);

			if (D3D12Check::GetLevel() == D3D12Level::D12_2)
			{
				billboardSelectionAmplificationShader_ = shaderCache.GetOrCreateAmplificationShader(String("../GraphicsEngine/Texture/ImageBillboardSelectionAS.hlsl"));
				psokey.amplificationShader_ = shaderCache.GetAmplificationShader(billboardSelectionAmplificationShader_)->Bytecode();
				psokey.meshShader_ = shaderCache.GetMeshShader(billboardMeshShader_)->Bytecode();
			}
			else
			{
				billboardSelectionVertexShader_ = shaderCache.GetOrCreateVertexShader(String("../GraphicsEngine/Texture/ImageBillboardSelectionVS.hlsl"));
				psokey.vertexShader_ = shaderCache.GetVertexShader(billboardSelectionVertexShader_)->Bytecode();
			}
			pipelineStateObjectSelectionMaskBillboard_ = pipelineStateObject_.GetOrCreate(device, psokey);
		}
	}

	ID3D12PipelineState* ImageShader::GetPipelineStateSprite()const
	{
		return pipelineStateObject_.Get(pipelineStateObjectSprite_);
	}

	ID3D12PipelineState* ImageShader::GetPipelineStateBillboard()const
	{
		return pipelineStateObject_.Get(pipelineStateObjectBillboard_);
	}

	ID3D12PipelineState* ImageShader::GetPipelineStateSelectionMaskSprite()const
	{
		return pipelineStateObject_.Get(pipelineStateObjectSelectionMaskSprite_);
	}

	ID3D12PipelineState* ImageShader::GetPipelineStateSelectionMaskBillboard()const
	{
		return pipelineStateObject_.Get(pipelineStateObjectSelectionMaskBillboard_);
	}

	ID3D12RootSignature* ImageShader::GetRootSignature()const
	{
		return rootSignature_.Get(imageRootSignature_)->Get();
	}
}
