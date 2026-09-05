#include <GraphicsEngine/Shape/Collider/ColliderLineShader.h>
#include <GraphicsEngine/Shader/ShaderCache.h>
#include <GraphicsEngine/D3D12/PipelineState/MeshShader.h>
#include <GraphicsEngine/D3D12/PipelineState/PixelShader.h>
#include <GraphicsEngine/D3D12/PipelineState/RasterizerState.h>
#include <GraphicsEngine/D3D12/PipelineState/BlendState.h>
#include <GraphicsEngine/D3D12/PipelineState/DepthStencilState.h>
#include <FoundationEngine/Log/DxFail.h>

namespace SeedCore
{
	void ColliderLineShader::Create(ShaderCache& shaderCache, ID3D12Device* device, PipelineStateObject& pipelineStateObject, DepthStencilStateType depthStencilStateType)
	{
		pipelineStateObject_ = &pipelineStateObject;

		D3D12_ROOT_PARAMETER params[2]{};

		/// [JP] 既存の ConstantIndices（Constants.hlsli, register b0 space1）を
		///      そのまま束縛する — 呼び出し側が Renderer::EditorFlush で既に
		///      持っている EditorConstantAddress をそのまま渡せる。
		params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		params[0].Descriptor.ShaderRegister = 0;
		params[0].Descriptor.RegisterSpace = 1;
		params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

		/// [JP] このフレームのラインバッファ index と本数だけを持つ、
		///      このシェーダ専用の小さな定数バッファ（register b0 space0）。
		params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		params[1].Descriptor.ShaderRegister = 0;
		params[1].Descriptor.RegisterSpace = 0;
		params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

		D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc{};
		rootSignatureDesc.NumParameters = 2;
		rootSignatureDesc.pParameters = params;
		rootSignatureDesc.NumStaticSamplers = 0;
		rootSignatureDesc.pStaticSamplers = nullptr;
		rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED;

		Microsoft::WRL::ComPtr<ID3DBlob> serializedRootSignature;
		Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
		HRESULT hr = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &serializedRootSignature, &errorBlob);
		if (FAILED(hr))
		{
			if (errorBlob)
			{
				OutputDebugStringA(static_cast<const Char*>(errorBlob->GetBufferPointer()));
			}
			return;
		}

		hr = device->CreateRootSignature(0, serializedRootSignature->GetBufferPointer(), serializedRootSignature->GetBufferSize(), IID_PPV_ARGS(&rootSignature_));
		SC_HR_CHECK(hr, "ColliderLineShader RootSignatureの生成に失敗しました");

		lineMeshShader_ = shaderCache.GetOrCreateMeshShader(String("../GraphicsEngine/Shape/Collider/ColliderLineMS.hlsl"));
		linePixelShader_ = shaderCache.GetOrCreatePixelShader(String("../GraphicsEngine/Shape/Collider/ColliderLinePS.hlsl"));

		PipelineStateKey psoKey{};
		memset(&psoKey, 0, sizeof(psoKey));
		psoKey.rootSignature_ = rootSignature_.Get();
		psoKey.meshShader_ = shaderCache.GetMeshShader(lineMeshShader_)->Bytecode();
		psoKey.pixelShader_ = shaderCache.GetPixelShader(linePixelShader_)->Bytecode();
		psoKey.rasterizerDesc_ = RasterizerState::Get(RasterizerStateType::SolidNoneLHS);
		psoKey.blendDesc_ = BlendState::Get(BlendStateType::Opaque);
		psoKey.depthStencilDesc_ = DepthStencilState::Get(depthStencilStateType);
		psoKey.renderTargetViewFormat_[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
		psoKey.renderTargetViewCount_ = 1;
		psoKey.depthStencilViewFormat_ = DXGI_FORMAT_D32_FLOAT;
		psoKey.primitiveTopologyType_ = D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED;
		pipelineState_ = pipelineStateObject_->GetOrCreate(device, psoKey);
	}

	ID3D12PipelineState* ColliderLineShader::GetPipelineState()const
	{
		return pipelineStateObject_->Get(pipelineState_);
	}

	ID3D12RootSignature* ColliderLineShader::GetRootSignature()const
	{
		return rootSignature_.Get();
	}
}
