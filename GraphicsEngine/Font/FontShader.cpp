#include <GraphicsEngine/Font/FontShader.h>
#include <GraphicsEngine/Shader/ShaderCache.h>
#include <GraphicsEngine/D3D12/PipelineState/AmplificationShader.h>
#include <GraphicsEngine/D3D12/PipelineState/MeshShader.h>
#include <GraphicsEngine/D3D12/PipelineState/PixelShader.h>
#include <GraphicsEngine/D3D12/PipelineState/RasterizerState.h>
#include <GraphicsEngine/D3D12/PipelineState/BlendState.h>
#include <GraphicsEngine/D3D12/PipelineState/DepthStencilState.h>

namespace SeedCore
{
	FontShader::FontShader(RootSignature& rootSignature, PipelineStateObject& pipelineStateObject) : rootSignature_(rootSignature), pipelineStateObject_(pipelineStateObject)
	{
		/// No Code
	}

	void FontShader::Create(ShaderCache& shaderCache, ID3D12Device* device)
	{
		fontRootSignature_ = rootSignature_.GetOrCreate(device);

		{
			spriteAmplificationShader_ = shaderCache.GetOrCreateAmplificationShader(String("../GraphicsEngine/Font/FontSpriteAS.hlsl"));
			spriteMeshShader_ = shaderCache.GetOrCreateMeshShader(String("../GraphicsEngine/Font/FontSpriteMS.hlsl"));
			spritePixelShader_ = shaderCache.GetOrCreatePixelShader(String("../GraphicsEngine/Font/FontSpritePS.hlsl"));

			PipelineStateKey psokey{};
			memset(&psokey, 0, sizeof(psokey));
			psokey.rootSignature_ = rootSignature_.Get(fontRootSignature_)->Get();
			psokey.amplificationShader_ = shaderCache.GetAmplificationShader(spriteAmplificationShader_)->Bytecode();
			psokey.meshShader_ = shaderCache.GetMeshShader(spriteMeshShader_)->Bytecode();
			psokey.pixelShader_ = shaderCache.GetPixelShader(spritePixelShader_)->Bytecode();
			psokey.rasterizerDesc_ = RasterizerState::Get(RasterizerStateType::SolidNoneLHS);
			psokey.blendDesc_ = BlendState::Get(BlendStateType::Alpha);
			psokey.depthStencilDesc_ = DepthStencilState::Get(DepthStencilStateType::DepthOff);
			psokey.renderTargetViewFormat_[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
			psokey.renderTargetViewCount_ = 1;
			psokey.depthStencilViewFormat_ = DXGI_FORMAT_D32_FLOAT;
			psokey.primitiveTopologyType_ = D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED;
			pipelineStateObjectSprite_ = pipelineStateObject_.GetOrCreate(device, psokey);
		}

		{
			billboardAmplificationShader_ = shaderCache.GetOrCreateAmplificationShader(String("../GraphicsEngine/Font/FontBillboardAS.hlsl"));
			billboardMeshShader_ = shaderCache.GetOrCreateMeshShader(String("../GraphicsEngine/Font/FontBillboardMS.hlsl"));
			billboardPixelShader_ = shaderCache.GetOrCreatePixelShader(String("../GraphicsEngine/Font/FontBillboardPS.hlsl"));

			PipelineStateKey psokey{};
			memset(&psokey, 0, sizeof(psokey));
			psokey.rootSignature_ = rootSignature_.Get(fontRootSignature_)->Get();
			psokey.amplificationShader_ = shaderCache.GetAmplificationShader(billboardAmplificationShader_)->Bytecode();
			psokey.meshShader_ = shaderCache.GetMeshShader(billboardMeshShader_)->Bytecode();
			psokey.pixelShader_ = shaderCache.GetPixelShader(billboardPixelShader_)->Bytecode();
			psokey.rasterizerDesc_ = RasterizerState::Get(RasterizerStateType::SolidNoneLHS);
			psokey.blendDesc_ = BlendState::Get(BlendStateType::Alpha);
			psokey.depthStencilDesc_ = DepthStencilState::Get(DepthStencilStateType::DepthOnWriteOnReverseZ);
			psokey.renderTargetViewFormat_[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
			psokey.renderTargetViewCount_ = 1;
			psokey.depthStencilViewFormat_ = DXGI_FORMAT_D32_FLOAT;
			psokey.primitiveTopologyType_ = D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED;
			pipelineStateObjectBillboard_ = pipelineStateObject_.GetOrCreate(device, psokey);
		}

		/// [EN] Selection outline mask PSOs: depth off for both (Billboard's normal
		///      PSO above tests depth; the mask ignores it, same reasoning as the
		///      model mask in ModelShader.cpp).
		/// [JP] 選択アウトラインマスク PSO: 両方とも深度オフ（上の通常 Billboard PSO
		///      は深度テストするが、マスクは無視する。理由はモデル側マスクと同じ）。
		{
			spriteSelectionAmplificationShader_ = shaderCache.GetOrCreateAmplificationShader(String("../GraphicsEngine/Font/FontSpriteSelectionAS.hlsl"));
			billboardSelectionAmplificationShader_ = shaderCache.GetOrCreateAmplificationShader(String("../GraphicsEngine/Font/FontBillboardSelectionAS.hlsl"));
			selectionMaskPixelShader_ = shaderCache.GetOrCreatePixelShader(String("../GraphicsEngine/Font/FontSelectionMaskPS.hlsl"));

			PipelineStateKey psokey{};
			memset(&psokey, 0, sizeof(psokey));
			psokey.rootSignature_ = rootSignature_.Get(fontRootSignature_)->Get();
			psokey.amplificationShader_ = shaderCache.GetAmplificationShader(spriteSelectionAmplificationShader_)->Bytecode();
			psokey.meshShader_ = shaderCache.GetMeshShader(spriteMeshShader_)->Bytecode();
			psokey.pixelShader_ = shaderCache.GetPixelShader(selectionMaskPixelShader_)->Bytecode();
			psokey.rasterizerDesc_ = RasterizerState::Get(RasterizerStateType::SolidNoneLHS);
			psokey.blendDesc_ = BlendState::Get(BlendStateType::Opaque);
			psokey.depthStencilDesc_ = DepthStencilState::Get(DepthStencilStateType::DepthOff);
			psokey.renderTargetViewFormat_[0] = DXGI_FORMAT_R8_UNORM;
			psokey.renderTargetViewCount_ = 1;
			psokey.depthStencilViewFormat_ = DXGI_FORMAT_UNKNOWN;
			psokey.primitiveTopologyType_ = D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED;
			pipelineStateObjectSelectionMaskSprite_ = pipelineStateObject_.GetOrCreate(device, psokey);

			psokey.amplificationShader_ = shaderCache.GetAmplificationShader(billboardSelectionAmplificationShader_)->Bytecode();
			psokey.meshShader_ = shaderCache.GetMeshShader(billboardMeshShader_)->Bytecode();
			pipelineStateObjectSelectionMaskBillboard_ = pipelineStateObject_.GetOrCreate(device, psokey);
		}
	}

	ID3D12PipelineState* FontShader::GetPipelineStateSprite()const
	{
		return pipelineStateObject_.Get(pipelineStateObjectSprite_);
	}

	ID3D12PipelineState* FontShader::GetPipelineStateBillboard()const
	{
		return pipelineStateObject_.Get(pipelineStateObjectBillboard_);
	}

	ID3D12PipelineState* FontShader::GetPipelineStateSelectionMaskSprite()const
	{
		return pipelineStateObject_.Get(pipelineStateObjectSelectionMaskSprite_);
	}

	ID3D12PipelineState* FontShader::GetPipelineStateSelectionMaskBillboard()const
	{
		return pipelineStateObject_.Get(pipelineStateObjectSelectionMaskBillboard_);
	}

	ID3D12RootSignature* FontShader::GetRootSignature()const
	{
		return rootSignature_.Get(fontRootSignature_)->Get();
	}
}
