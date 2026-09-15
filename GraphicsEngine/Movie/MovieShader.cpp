#include <GraphicsEngine/Movie/MovieShader.h>
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
	MovieShader::MovieShader(RootSignature& rootSignature, PipelineStateObject& pipelineStateObject) : rootSignature_(rootSignature), pipelineStateObject_(pipelineStateObject)
	{
		/// No Code
	}

	void MovieShader::Create(ShaderCache& shaderCache, ID3D12Device* device)
	{
		movieRootSignature_ = rootSignature_.GetOrCreate(device);

		{
			spritePixelShader_ = shaderCache.GetOrCreatePixelShader(String("../GraphicsEngine/Movie/MovieSpritePS.hlsl"));

			PipelineStateKey psokey{};
			memset(&psokey, 0, sizeof(psokey));
			psokey.rootSignature_ = rootSignature_.Get(movieRootSignature_)->Get();
			if (D3D12Check::GetLevel() == D3D12Level::D12_2)
			{
				spriteAmplificationShader_ = shaderCache.GetOrCreateAmplificationShader(String("../GraphicsEngine/Movie/MovieSpriteAS.hlsl"));
				spriteMeshShader_ = shaderCache.GetOrCreateMeshShader(String("../GraphicsEngine/Movie/MovieSpriteMS.hlsl"));
				psokey.amplificationShader_ = shaderCache.GetAmplificationShader(spriteAmplificationShader_)->Bytecode();
				psokey.meshShader_ = shaderCache.GetMeshShader(spriteMeshShader_)->Bytecode();
				psokey.primitiveTopologyType_ = D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED;
			}
			else
			{
				spriteVertexShader_ = shaderCache.GetOrCreateVertexShader(String("../GraphicsEngine/Movie/MovieSpriteVS.hlsl"));
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
			billboardPixelShader_ = shaderCache.GetOrCreatePixelShader(String("../GraphicsEngine/Movie/MovieBillboardPS.hlsl"));

			PipelineStateKey psokey{};
			memset(&psokey, 0, sizeof(psokey));
			psokey.rootSignature_ = rootSignature_.Get(movieRootSignature_)->Get();
			if (D3D12Check::GetLevel() == D3D12Level::D12_2)
			{
				billboardAmplificationShader_ = shaderCache.GetOrCreateAmplificationShader(String("../GraphicsEngine/Movie/MovieBillboardAS.hlsl"));
				billboardMeshShader_ = shaderCache.GetOrCreateMeshShader(String("../GraphicsEngine/Movie/MovieBillboardMS.hlsl"));
				psokey.amplificationShader_ = shaderCache.GetAmplificationShader(billboardAmplificationShader_)->Bytecode();
				psokey.meshShader_ = shaderCache.GetMeshShader(billboardMeshShader_)->Bytecode();
				psokey.primitiveTopologyType_ = D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED;
			}
			else
			{
				billboardVertexShader_ = shaderCache.GetOrCreateVertexShader(String("../GraphicsEngine/Movie/MovieBillboardVS.hlsl"));
				psokey.vertexShader_ = shaderCache.GetVertexShader(billboardVertexShader_)->Bytecode();
				psokey.primitiveTopologyType_ = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
			}
			psokey.pixelShader_ = shaderCache.GetPixelShader(billboardPixelShader_)->Bytecode();
			psokey.rasterizerDesc_ = RasterizerState::Get(RasterizerStateType::SolidNoneLHS);
			psokey.blendDesc_ = BlendState::Get(BlendStateType::Alpha);
			psokey.depthStencilDesc_ = DepthStencilState::Get(DepthStencilStateType::DepthOnWriteOnReverseZ);
			psokey.renderTargetViewFormat_[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
			psokey.renderTargetViewCount_ = 1;
			psokey.depthStencilViewFormat_ = DXGI_FORMAT_D32_FLOAT;
			pipelineStateObjectBillboard_ = pipelineStateObject_.GetOrCreate(device, psokey);
		}

		{
			fullscreenPixelShader_ = shaderCache.GetOrCreatePixelShader(String("../GraphicsEngine/Movie/MovieFullscreenPS.hlsl"));

			PipelineStateKey psokey{};
			memset(&psokey, 0, sizeof(psokey));
			psokey.rootSignature_ = rootSignature_.Get(movieRootSignature_)->Get();
			if (D3D12Check::GetLevel() == D3D12Level::D12_2)
			{
				fullscreenAmplificationShader_ = shaderCache.GetOrCreateAmplificationShader(String("../GraphicsEngine/Movie/MovieFullscreenAS.hlsl"));
				fullscreenMeshShader_ = shaderCache.GetOrCreateMeshShader(String("../GraphicsEngine/Movie/MovieFullscreenMS.hlsl"));
				psokey.amplificationShader_ = shaderCache.GetAmplificationShader(fullscreenAmplificationShader_)->Bytecode();
				psokey.meshShader_ = shaderCache.GetMeshShader(fullscreenMeshShader_)->Bytecode();
				psokey.primitiveTopologyType_ = D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED;
			}
			else
			{
				fullscreenVertexShader_ = shaderCache.GetOrCreateVertexShader(String("../GraphicsEngine/Movie/MovieFullscreenVS.hlsl"));
				psokey.vertexShader_ = shaderCache.GetVertexShader(fullscreenVertexShader_)->Bytecode();
				psokey.primitiveTopologyType_ = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
			}
			psokey.pixelShader_ = shaderCache.GetPixelShader(fullscreenPixelShader_)->Bytecode();
			psokey.rasterizerDesc_ = RasterizerState::Get(RasterizerStateType::SolidNoneLHS);
			psokey.blendDesc_ = BlendState::Get(BlendStateType::Alpha);
			psokey.depthStencilDesc_ = DepthStencilState::Get(DepthStencilStateType::DepthOff);
			psokey.renderTargetViewFormat_[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
			psokey.renderTargetViewCount_ = 1;
			psokey.depthStencilViewFormat_ = DXGI_FORMAT_D32_FLOAT;
			pipelineStateObjectFullscreen_ = pipelineStateObject_.GetOrCreate(device, psokey);
		}

		{
			silhouettePixelShader_ = shaderCache.GetOrCreatePixelShader(String("../GraphicsEngine/Movie/MovieSilhouettePS.hlsl"));

			PipelineStateKey psokey{};
			memset(&psokey, 0, sizeof(psokey));
			psokey.rootSignature_ = rootSignature_.Get(movieRootSignature_)->Get();
			if (D3D12Check::GetLevel() == D3D12Level::D12_2)
			{
				spriteSilhouetteAmplificationShader_ = shaderCache.GetOrCreateAmplificationShader(String("../GraphicsEngine/Movie/MovieSpriteSilhouetteAS.hlsl"));
				psokey.amplificationShader_ = shaderCache.GetAmplificationShader(spriteSilhouetteAmplificationShader_)->Bytecode();
				psokey.meshShader_ = shaderCache.GetMeshShader(spriteMeshShader_)->Bytecode();
				psokey.primitiveTopologyType_ = D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED;
			}
			else
			{
				spriteSilhouetteVertexShader_ = shaderCache.GetOrCreateVertexShader(String("../GraphicsEngine/Movie/MovieSpriteSilhouetteVS.hlsl"));
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
				billboardSilhouetteAmplificationShader_ = shaderCache.GetOrCreateAmplificationShader(String("../GraphicsEngine/Movie/MovieBillboardSilhouetteAS.hlsl"));
				psokey.amplificationShader_ = shaderCache.GetAmplificationShader(billboardSilhouetteAmplificationShader_)->Bytecode();
				psokey.meshShader_ = shaderCache.GetMeshShader(billboardMeshShader_)->Bytecode();
			}
			else
			{
				billboardSilhouetteVertexShader_ = shaderCache.GetOrCreateVertexShader(String("../GraphicsEngine/Movie/MovieBillboardSilhouetteVS.hlsl"));
				psokey.vertexShader_ = shaderCache.GetVertexShader(billboardSilhouetteVertexShader_)->Bytecode();
			}
			pipelineStateObjectSilhouetteBillboard_ = pipelineStateObject_.GetOrCreate(device, psokey);
		}
	}

	ID3D12PipelineState* MovieShader::GetPipelineStateSprite()const
	{
		return pipelineStateObject_.Get(pipelineStateObjectSprite_);
	}

	ID3D12PipelineState* MovieShader::GetPipelineStateBillboard()const
	{
		return pipelineStateObject_.Get(pipelineStateObjectBillboard_);
	}

	ID3D12PipelineState* MovieShader::GetPipelineStateFullscreen()const
	{
		return pipelineStateObject_.Get(pipelineStateObjectFullscreen_);
	}

	ID3D12PipelineState* MovieShader::GetPipelineStateSilhouetteSprite()const
	{
		return pipelineStateObject_.Get(pipelineStateObjectSilhouetteSprite_);
	}

	ID3D12PipelineState* MovieShader::GetPipelineStateSilhouetteBillboard()const
	{
		return pipelineStateObject_.Get(pipelineStateObjectSilhouetteBillboard_);
	}

	ID3D12RootSignature* MovieShader::GetRootSignature()const
	{
		return rootSignature_.Get(movieRootSignature_)->Get();
	}
}
