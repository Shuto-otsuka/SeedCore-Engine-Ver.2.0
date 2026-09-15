#include <GraphicsEngine/Environment/WeatherParticleShader.h>
#include <GraphicsEngine/Shader/ShaderCache.h>
#include <GraphicsEngine/D3D12/Context/D3D12Check.h>
#include <GraphicsEngine/D3D12/PipelineState/VertexShader.h>
#include <GraphicsEngine/D3D12/PipelineState/ComputeShader.h>
#include <GraphicsEngine/D3D12/PipelineState/AmplificationShader.h>
#include <GraphicsEngine/D3D12/PipelineState/MeshShader.h>
#include <GraphicsEngine/D3D12/PipelineState/PixelShader.h>
#include <GraphicsEngine/D3D12/PipelineState/RasterizerState.h>
#include <GraphicsEngine/D3D12/PipelineState/BlendState.h>
#include <GraphicsEngine/D3D12/PipelineState/DepthStencilState.h>

namespace SeedCore
{
	WeatherParticleShader::WeatherParticleShader(RootSignature& rootSignature, PipelineStateObject& pipelineStateObject) : rootSignature_(rootSignature), pipelineStateObject_(pipelineStateObject)
	{
		/// No Code
	}

	void WeatherParticleShader::Create(ShaderCache& shaderCache, ID3D12Device* device)
	{
		particleRootSignature_ = rootSignature_.GetOrCreate(device);
		ID3D12RootSignature* signature = rootSignature_.Get(particleRootSignature_)->Get();

		simulateShader_ = shaderCache.GetOrCreateComputeShader(String("../GraphicsEngine/Environment/WeatherParticleSimulateCS.hlsl"));

		PipelineStateKey simulateKey{};
		memset(&simulateKey, 0, sizeof(simulateKey));
		simulateKey.rootSignature_ = signature;
		simulateKey.computeShader_ = shaderCache.GetComputeShader(simulateShader_)->Bytecode();
		simulatePipelineStateHandle_ = pipelineStateObject_.GetOrCreate(device, simulateKey);

		Handle<PixelShader> pixelShader = shaderCache.GetOrCreatePixelShader(String("../GraphicsEngine/Environment/WeatherParticlePS.hlsl"));

		PipelineStateKey drawKey{};
		memset(&drawKey, 0, sizeof(drawKey));
		drawKey.rootSignature_ = signature;
		if (D3D12Check::GetLevel() == D3D12Level::D12_2)
		{
			Handle<AmplificationShader> amplificationShader = shaderCache.GetOrCreateAmplificationShader(String("../GraphicsEngine/Environment/WeatherParticleAS.hlsl"));
			Handle<MeshShader> meshShader = shaderCache.GetOrCreateMeshShader(String("../GraphicsEngine/Environment/WeatherParticleMS.hlsl"));
			drawKey.amplificationShader_ = shaderCache.GetAmplificationShader(amplificationShader)->Bytecode();
			drawKey.meshShader_ = shaderCache.GetMeshShader(meshShader)->Bytecode();
			drawKey.primitiveTopologyType_ = D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED;
		}
		else
		{
			Handle<VertexShader> vertexShader = shaderCache.GetOrCreateVertexShader(String("../GraphicsEngine/Environment/WeatherParticleVS.hlsl"));
			drawKey.vertexShader_ = shaderCache.GetVertexShader(vertexShader)->Bytecode();
			drawKey.primitiveTopologyType_ = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		}
		drawKey.pixelShader_ = shaderCache.GetPixelShader(pixelShader)->Bytecode();
		drawKey.rasterizerDesc_ = RasterizerState::Get(RasterizerStateType::SolidNoneLHS);
		drawKey.blendDesc_ = BlendState::Get(BlendStateType::Additive);
		/// [JP] 深度テストのみ・書込み無し: 既存のG-Buffer深度に対して
		///      ハードウェアが画素単位で遮蔽判定する(パーティクルとモデルの
		///      「衝突」)。Reverse-Z のこのエンジンに合わせる。
		drawKey.depthStencilDesc_ = DepthStencilState::Get(DepthStencilStateType::DepthOnWriteOffReverseZ);
		drawKey.renderTargetViewFormat_[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
		drawKey.renderTargetViewCount_ = 1;
		drawKey.depthStencilViewFormat_ = DXGI_FORMAT_D32_FLOAT;
		drawPipelineStateHandle_ = pipelineStateObject_.GetOrCreate(device, drawKey);
	}

	ID3D12PipelineState* WeatherParticleShader::GetSimulatePipelineState()const
	{
		return pipelineStateObject_.Get(simulatePipelineStateHandle_);
	}

	ID3D12PipelineState* WeatherParticleShader::GetDrawPipelineState()const
	{
		return pipelineStateObject_.Get(drawPipelineStateHandle_);
	}

	ID3D12RootSignature* WeatherParticleShader::GetRootSignature()const
	{
		return rootSignature_.Get(particleRootSignature_)->Get();
	}
}
