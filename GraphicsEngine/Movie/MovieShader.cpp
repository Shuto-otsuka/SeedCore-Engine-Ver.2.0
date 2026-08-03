#include <GraphicsEngine/Movie/MovieShader.h>
#include <GraphicsEngine/Shader/ShaderCache.h>
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
			spriteAmplificationShader_ = shaderCache.GetOrCreateAmplificationShader(String("../GraphicsEngine/Movie/MovieSpriteAS.hlsl"));
			spriteMeshShader_ = shaderCache.GetOrCreateMeshShader(String("../GraphicsEngine/Movie/MovieSpriteMS.hlsl"));
			spritePixelShader_ = shaderCache.GetOrCreatePixelShader(String("../GraphicsEngine/Movie/MovieSpritePS.hlsl"));

			PipelineStateKey psokey{};
			memset(&psokey, 0, sizeof(psokey));
			psokey.rootSignature_ = rootSignature_.Get(movieRootSignature_)->Get();
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
			billboardAmplificationShader_ = shaderCache.GetOrCreateAmplificationShader(String("../GraphicsEngine/Movie/MovieBillboardAS.hlsl"));
			billboardMeshShader_ = shaderCache.GetOrCreateMeshShader(String("../GraphicsEngine/Movie/MovieBillboardMS.hlsl"));
			billboardPixelShader_ = shaderCache.GetOrCreatePixelShader(String("../GraphicsEngine/Movie/MovieBillboardPS.hlsl"));

			PipelineStateKey psokey{};
			memset(&psokey, 0, sizeof(psokey));
			psokey.rootSignature_ = rootSignature_.Get(movieRootSignature_)->Get();
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

		{
			fullscreenAmplificationShader_ = shaderCache.GetOrCreateAmplificationShader(String("../GraphicsEngine/Movie/MovieFullscreenAS.hlsl"));
			fullscreenMeshShader_ = shaderCache.GetOrCreateMeshShader(String("../GraphicsEngine/Movie/MovieFullscreenMS.hlsl"));
			fullscreenPixelShader_ = shaderCache.GetOrCreatePixelShader(String("../GraphicsEngine/Movie/MovieFullscreenPS.hlsl"));

			PipelineStateKey psokey{};
			memset(&psokey, 0, sizeof(psokey));
			psokey.rootSignature_ = rootSignature_.Get(movieRootSignature_)->Get();
			psokey.amplificationShader_ = shaderCache.GetAmplificationShader(fullscreenAmplificationShader_)->Bytecode();
			psokey.meshShader_ = shaderCache.GetMeshShader(fullscreenMeshShader_)->Bytecode();
			psokey.pixelShader_ = shaderCache.GetPixelShader(fullscreenPixelShader_)->Bytecode();
			psokey.rasterizerDesc_ = RasterizerState::Get(RasterizerStateType::SolidNoneLHS);
			psokey.blendDesc_ = BlendState::Get(BlendStateType::Alpha);
			psokey.depthStencilDesc_ = DepthStencilState::Get(DepthStencilStateType::DepthOff);
			psokey.renderTargetViewFormat_[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
			psokey.renderTargetViewCount_ = 1;
			psokey.depthStencilViewFormat_ = DXGI_FORMAT_D32_FLOAT;
			psokey.primitiveTopologyType_ = D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED;
			pipelineStateObjectFullscreen_ = pipelineStateObject_.GetOrCreate(device, psokey);
		}

		{
			spriteSelectionAmplificationShader_ = shaderCache.GetOrCreateAmplificationShader(String("../GraphicsEngine/Movie/MovieSpriteSelectionAS.hlsl"));
			billboardSelectionAmplificationShader_ = shaderCache.GetOrCreateAmplificationShader(String("../GraphicsEngine/Movie/MovieBillboardSelectionAS.hlsl"));
			selectionMaskPixelShader_ = shaderCache.GetOrCreatePixelShader(String("../GraphicsEngine/Movie/MovieSelectionMaskPS.hlsl"));

			PipelineStateKey psokey{};
			memset(&psokey, 0, sizeof(psokey));
			psokey.rootSignature_ = rootSignature_.Get(movieRootSignature_)->Get();
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

	ID3D12PipelineState* MovieShader::GetPipelineStateSelectionMaskSprite()const
	{
		return pipelineStateObject_.Get(pipelineStateObjectSelectionMaskSprite_);
	}

	ID3D12PipelineState* MovieShader::GetPipelineStateSelectionMaskBillboard()const
	{
		return pipelineStateObject_.Get(pipelineStateObjectSelectionMaskBillboard_);
	}

	ID3D12RootSignature* MovieShader::GetRootSignature()const
	{
		return rootSignature_.Get(movieRootSignature_)->Get();
	}
}
