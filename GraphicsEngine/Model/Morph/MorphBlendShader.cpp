#include <GraphicsEngine/Model/Morph/MorphBlendShader.h>
#include <GraphicsEngine/Shader/ShaderCache.h>
#include <GraphicsEngine/D3D12/PipelineState/ComputeShader.h>
#include <FoundationEngine/Log/DxFail.h>

namespace SeedCore
{
	void MorphBlendShader::Create(ShaderCache& shaderCache, ID3D12Device* device)
	{
		D3D12_ROOT_PARAMETER params[5]{};
		params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
		params[0].Constants.ShaderRegister = 0;
		params[0].Constants.RegisterSpace = 0;
		params[0].Constants.Num32BitValues = 4;
		params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

		params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
		params[1].Descriptor.ShaderRegister = 0;
		params[1].Descriptor.RegisterSpace = 0;
		params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

		params[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
		params[2].Descriptor.ShaderRegister = 1;
		params[2].Descriptor.RegisterSpace = 0;
		params[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

		params[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
		params[3].Descriptor.ShaderRegister = 2;
		params[3].Descriptor.RegisterSpace = 0;
		params[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

		params[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
		params[4].Descriptor.ShaderRegister = 0;
		params[4].Descriptor.RegisterSpace = 0;
		params[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

		D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc{};
		rootSignatureDesc.NumParameters = 5;
		rootSignatureDesc.pParameters = params;

		Microsoft::WRL::ComPtr<ID3DBlob> serializedRootSignature;
		Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
		HRESULT hr = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &serializedRootSignature, &errorBlob);
		SC_HR_CHECK(hr, "RootSignatureのシリアライズに失敗しました");

		hr = device->CreateRootSignature(0, serializedRootSignature->GetBufferPointer(), serializedRootSignature->GetBufferSize(), IID_PPV_ARGS(rootSignature_.GetAddressOf()));
		SC_HR_CHECK(hr, "RootSignatureの生成に失敗しました");

		Handle<ComputeShader> shader = shaderCache.GetOrCreateComputeShader(String("../GraphicsEngine/Model/Morph/MorphBlendCS.hlsl"));

		D3D12_COMPUTE_PIPELINE_STATE_DESC pipelineStateDesc{};
		pipelineStateDesc.pRootSignature = rootSignature_.Get();
		pipelineStateDesc.CS = shaderCache.GetComputeShader(shader)->Bytecode();

		hr = device->CreateComputePipelineState(&pipelineStateDesc, IID_PPV_ARGS(pipelineState_.GetAddressOf()));
		SC_HR_CHECK(hr, "PipelineStateの生成に失敗しました");
	}

	ID3D12RootSignature* MorphBlendShader::GetRootSignature()const
	{
		return rootSignature_.Get();
	}

	ID3D12PipelineState* MorphBlendShader::GetPipelineState()const
	{
		return pipelineState_.Get();
	}
}
