#include <GraphicsEngine/Model/BC7CompressShader.h>
#include <GraphicsEngine/Shader/ShaderCache.h>
#include <GraphicsEngine/D3D12/PipelineState/ComputeShader.h>
#include <FoundationEngine/Log/DxFail.h>

namespace SeedCore
{
	void BC7CompressShader::Create(ShaderCache& shaderCache, ID3D12Device* device)
	{
		/// [EN] Root constants (TextureWidth/TextureHeight/BlockCountX/BlockCountY),
		///      a root SRV for the packed-pixel input buffer, a root UAV for the
		///      compressed-block output buffer. No descriptor table/heap involved.
		/// [JP] ルート定数(TextureWidth/TextureHeight/BlockCountX/BlockCountY)、
		///      パックしたピクセル入力バッファ用の root SRV、圧縮ブロック出力
		///      バッファ用の root UAV。ディスクリプタテーブル/ヒープは使わない。
		D3D12_ROOT_PARAMETER params[3]{};
		params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
		params[0].Constants.ShaderRegister = 0;
		params[0].Constants.RegisterSpace = 0;
		params[0].Constants.Num32BitValues = 4;
		params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

		params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
		params[1].Descriptor.ShaderRegister = 0;
		params[1].Descriptor.RegisterSpace = 0;
		params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

		params[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
		params[2].Descriptor.ShaderRegister = 0;
		params[2].Descriptor.RegisterSpace = 0;
		params[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

		D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc{};
		rootSignatureDesc.NumParameters = 3;
		rootSignatureDesc.pParameters = params;

		Microsoft::WRL::ComPtr<ID3DBlob> serializedRootSignature;
		Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
		HRESULT hr = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &serializedRootSignature, &errorBlob);
		SC_HR_CHECK(hr, "RootSignatureのシリアライズに失敗しました");

		hr = device->CreateRootSignature(0, serializedRootSignature->GetBufferPointer(), serializedRootSignature->GetBufferSize(), IID_PPV_ARGS(rootSignature_.GetAddressOf()));
		SC_HR_CHECK(hr, "RootSignatureの生成に失敗しました");

		Handle<ComputeShader> shader = shaderCache.GetOrCreateComputeShader(String("../GraphicsEngine/Model/BC7CompressCS.hlsl"));

		D3D12_COMPUTE_PIPELINE_STATE_DESC pipelineStateDesc{};
		pipelineStateDesc.pRootSignature = rootSignature_.Get();
		pipelineStateDesc.CS = shaderCache.GetComputeShader(shader)->Bytecode();

		hr = device->CreateComputePipelineState(&pipelineStateDesc, IID_PPV_ARGS(pipelineState_.GetAddressOf()));
		SC_HR_CHECK(hr, "PipelineStateの生成に失敗しました");
	}

	ID3D12RootSignature* BC7CompressShader::GetRootSignature()const
	{
		return rootSignature_.Get();
	}

	ID3D12PipelineState* BC7CompressShader::GetPipelineState()const
	{
		return pipelineState_.Get();
	}
}
