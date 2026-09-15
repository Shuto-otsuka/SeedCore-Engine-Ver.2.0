#include <GraphicsEngine/Font/FontShader.h>
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
	FontShader::FontShader(RootSignature& rootSignature, PipelineStateObject& pipelineStateObject) : rootSignature_(rootSignature), pipelineStateObject_(pipelineStateObject)
	{
		/// No Code
	}

	void FontShader::Create(ShaderCache& shaderCache, ID3D12Device* device)
	{
		fontRootSignature_ = rootSignature_.GetOrCreate(device);

		{
			spritePixelShader_ = shaderCache.GetOrCreatePixelShader(String("../GraphicsEngine/Font/Sprite/FontSpritePS.hlsl"));

			PipelineStateKey psokey{};
			memset(&psokey, 0, sizeof(psokey));
			psokey.rootSignature_ = rootSignature_.Get(fontRootSignature_)->Get();
			if (D3D12Check::GetLevel() == D3D12Level::D12_2)
			{
				spriteAmplificationShader_ = shaderCache.GetOrCreateAmplificationShader(String("../GraphicsEngine/Font/Sprite/FontSpriteAS.hlsl"));
				spriteMeshShader_ = shaderCache.GetOrCreateMeshShader(String("../GraphicsEngine/Font/Sprite/FontSpriteMS.hlsl"));
				psokey.amplificationShader_ = shaderCache.GetAmplificationShader(spriteAmplificationShader_)->Bytecode();
				psokey.meshShader_ = shaderCache.GetMeshShader(spriteMeshShader_)->Bytecode();
				psokey.primitiveTopologyType_ = D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED;
			}
			else
			{
				spriteVertexShader_ = shaderCache.GetOrCreateVertexShader(String("../GraphicsEngine/Font/Sprite/FontSpriteVS.hlsl"));
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
			billboardPixelShader_ = shaderCache.GetOrCreatePixelShader(String("../GraphicsEngine/Font/Billboard/FontBillboardPS.hlsl"));

			PipelineStateKey psokey{};
			memset(&psokey, 0, sizeof(psokey));
			psokey.rootSignature_ = rootSignature_.Get(fontRootSignature_)->Get();
			if (D3D12Check::GetLevel() == D3D12Level::D12_2)
			{
				billboardAmplificationShader_ = shaderCache.GetOrCreateAmplificationShader(String("../GraphicsEngine/Font/Billboard/FontBillboardAS.hlsl"));
				billboardMeshShader_ = shaderCache.GetOrCreateMeshShader(String("../GraphicsEngine/Font/Billboard/FontBillboardMS.hlsl"));
				psokey.amplificationShader_ = shaderCache.GetAmplificationShader(billboardAmplificationShader_)->Bytecode();
				psokey.meshShader_ = shaderCache.GetMeshShader(billboardMeshShader_)->Bytecode();
				psokey.primitiveTopologyType_ = D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED;
			}
			else
			{
				billboardVertexShader_ = shaderCache.GetOrCreateVertexShader(String("../GraphicsEngine/Font/Billboard/FontBillboardVS.hlsl"));
				psokey.vertexShader_ = shaderCache.GetVertexShader(billboardVertexShader_)->Bytecode();
				psokey.primitiveTopologyType_ = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
			}
			psokey.pixelShader_ = shaderCache.GetPixelShader(billboardPixelShader_)->Bytecode();
			psokey.rasterizerDesc_ = RasterizerState::Get(RasterizerStateType::SolidNoneLHS);
			psokey.blendDesc_ = BlendState::Get(BlendStateType::Alpha);
			psokey.depthStencilDesc_ = DepthStencilState::Get(DepthStencilStateType::DepthOnWriteOffReverseZ);
			psokey.renderTargetViewFormat_[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
			psokey.renderTargetViewCount_ = 1;
			psokey.depthStencilViewFormat_ = DXGI_FORMAT_D32_FLOAT;
			pipelineStateObjectBillboard_ = pipelineStateObject_.GetOrCreate(device, psokey);
		}

		/// [EN] Silhouette PSOs: depth off for both (Billboard's normal
		///      PSO above tests depth; the mask ignores it, same reasoning as the
		///      model mask in ModelShader.cpp).
		/// [JP] シルエット PSO: 両方とも深度オフ（上の通常 Billboard PSO
		///      は深度テストするが、マスクは無視する。理由はモデル側マスクと同じ）。
		{
			silhouettePixelShader_ = shaderCache.GetOrCreatePixelShader(String("../GraphicsEngine/Font/FontSilhouettePS.hlsl"));

			PipelineStateKey psokey{};
			memset(&psokey, 0, sizeof(psokey));
			psokey.rootSignature_ = rootSignature_.Get(fontRootSignature_)->Get();
			if (D3D12Check::GetLevel() == D3D12Level::D12_2)
			{
				spriteSilhouetteAmplificationShader_ = shaderCache.GetOrCreateAmplificationShader(String("../GraphicsEngine/Font/Sprite/FontSpriteSilhouetteAS.hlsl"));
				psokey.amplificationShader_ = shaderCache.GetAmplificationShader(spriteSilhouetteAmplificationShader_)->Bytecode();
				psokey.meshShader_ = shaderCache.GetMeshShader(spriteMeshShader_)->Bytecode();
				psokey.primitiveTopologyType_ = D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED;
			}
			else
			{
				spriteSilhouetteVertexShader_ = shaderCache.GetOrCreateVertexShader(String("../GraphicsEngine/Font/Sprite/FontSpriteSilhouetteVS.hlsl"));
				psokey.vertexShader_ = shaderCache.GetVertexShader(spriteSilhouetteVertexShader_)->Bytecode();
				psokey.primitiveTopologyType_ = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
			}
			psokey.pixelShader_ = shaderCache.GetPixelShader(silhouettePixelShader_)->Bytecode();
			psokey.rasterizerDesc_ = RasterizerState::Get(RasterizerStateType::SolidNoneLHS);
			psokey.blendDesc_ = BlendState::Get(BlendStateType::Opaque);
			psokey.depthStencilDesc_ = DepthStencilState::Get(DepthStencilStateType::DepthOff);
			psokey.renderTargetViewFormat_[0] = DXGI_FORMAT_R8_UNORM;
			psokey.renderTargetViewCount_ = 1;
			psokey.depthStencilViewFormat_ = DXGI_FORMAT_UNKNOWN;
			pipelineStateObjectSilhouetteSprite_ = pipelineStateObject_.GetOrCreate(device, psokey);

			if (D3D12Check::GetLevel() == D3D12Level::D12_2)
			{
				billboardSilhouetteAmplificationShader_ = shaderCache.GetOrCreateAmplificationShader(String("../GraphicsEngine/Font/Billboard/FontBillboardSilhouetteAS.hlsl"));
				psokey.amplificationShader_ = shaderCache.GetAmplificationShader(billboardSilhouetteAmplificationShader_)->Bytecode();
				psokey.meshShader_ = shaderCache.GetMeshShader(billboardMeshShader_)->Bytecode();
			}
			else
			{
				billboardSilhouetteVertexShader_ = shaderCache.GetOrCreateVertexShader(String("../GraphicsEngine/Font/Billboard/FontBillboardSilhouetteVS.hlsl"));
				psokey.vertexShader_ = shaderCache.GetVertexShader(billboardSilhouetteVertexShader_)->Bytecode();
			}
			pipelineStateObjectSilhouetteBillboard_ = pipelineStateObject_.GetOrCreate(device, psokey);
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

	ID3D12PipelineState* FontShader::GetPipelineStateSilhouetteSprite()const
	{
		return pipelineStateObject_.Get(pipelineStateObjectSilhouetteSprite_);
	}

	ID3D12PipelineState* FontShader::GetPipelineStateSilhouetteBillboard()const
	{
		return pipelineStateObject_.Get(pipelineStateObjectSilhouetteBillboard_);
	}

	ID3D12RootSignature* FontShader::GetRootSignature()const
	{
		return rootSignature_.Get(fontRootSignature_)->Get();
	}
}
