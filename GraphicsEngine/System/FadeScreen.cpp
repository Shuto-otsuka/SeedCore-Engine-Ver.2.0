#include <GraphicsEngine/System/FadeScreen.h>
#include <GraphicsEngine/Shader/ShaderCompiler.h>
#include <FoundationEngine/Log/DxFail.h>

namespace SeedCore
{
	void FadeScreen::Initialize(ID3D12Device* device)
	{
		HRESULT hr{ S_OK };

		D3D12_ROOT_PARAMETER params[1]{};

		params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
		params[0].Constants.ShaderRegister = 0;
		params[0].Constants.RegisterSpace = 0;
		params[0].Constants.Num32BitValues = 1;
		params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

		D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc{};
		rootSignatureDesc.NumParameters = 1;
		rootSignatureDesc.pParameters = params;
		rootSignatureDesc.NumStaticSamplers = 0;
		rootSignatureDesc.pStaticSamplers = nullptr;
		rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

		Microsoft::WRL::ComPtr<ID3DBlob> serialized;
		Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
		hr = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &serialized, &errorBlob);
		SC_HR_CHECK(hr, "FadeScreen RootSignatureのシリアライズに失敗しました");

		hr = device->CreateRootSignature(0, serialized->GetBufferPointer(), serialized->GetBufferSize(), IID_PPV_ARGS(&rootSignature_));
		SC_HR_CHECK(hr, "FadeScreen RootSignatureの生成に失敗しました");

		auto vertexShaderResult = ShaderCompiler::CompileVertexShader(L"../GraphicsEngine/System/SplashScreenVS.hlsl", "main");
		auto pixelShaderResult = ShaderCompiler::CompilePixelShader(L"../GraphicsEngine/System/FadeScreenPS.hlsl", "main");

		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
		psoDesc.pRootSignature = rootSignature_.Get();
		psoDesc.VS = { vertexShaderResult.objectBlob->GetBufferPointer(), vertexShaderResult.objectBlob->GetBufferSize() };
		psoDesc.PS = { pixelShaderResult.objectBlob->GetBufferPointer(), pixelShaderResult.objectBlob->GetBufferSize() };

		psoDesc.BlendState.AlphaToCoverageEnable = FALSE;
		psoDesc.BlendState.IndependentBlendEnable = FALSE;
		psoDesc.BlendState.RenderTarget[0].BlendEnable = TRUE;
		psoDesc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
		psoDesc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
		psoDesc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
		psoDesc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
		psoDesc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
		psoDesc.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
		psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

		psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
		psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
		psoDesc.RasterizerState.FrontCounterClockwise = FALSE;
		psoDesc.RasterizerState.DepthBias = 0;
		psoDesc.RasterizerState.DepthBiasClamp = 0.0f;
		psoDesc.RasterizerState.SlopeScaledDepthBias = 0.0f;
		psoDesc.RasterizerState.DepthClipEnable = TRUE;
		psoDesc.RasterizerState.MultisampleEnable = FALSE;
		psoDesc.RasterizerState.AntialiasedLineEnable = FALSE;
		psoDesc.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

		psoDesc.DepthStencilState.DepthEnable = FALSE;
		psoDesc.DepthStencilState.StencilEnable = FALSE;

		psoDesc.SampleMask = UINT_MAX;
		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		psoDesc.NumRenderTargets = 1;
		psoDesc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
		psoDesc.SampleDesc.Count = 1;
		psoDesc.SampleDesc.Quality = 0;

		hr = device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipelineState_));
		SC_HR_CHECK(hr, "FadeScreen PipelineStateの生成に失敗しました");

		initialized_ = true;
	}

	void FadeScreen::Finalize()
	{
		rootSignature_.Reset();
		pipelineState_.Reset();

		initialized_ = false;
	}

	void FadeScreen::Draw(ID3D12GraphicsCommandList6* cmdList, Float alpha, Float screenWidth, Float screenHeight)
	{
		if (!initialized_ || alpha <= 0.0f)
		{
			return;
		}

		D3D12_VIEWPORT viewport{};
		viewport.TopLeftX = 0.0f;
		viewport.TopLeftY = 0.0f;
		viewport.Width = screenWidth;
		viewport.Height = screenHeight;
		viewport.MinDepth = 0.0f;
		viewport.MaxDepth = 1.0f;
		cmdList->RSSetViewports(1, &viewport);

		D3D12_RECT scissor{};
		scissor.left = 0;
		scissor.top = 0;
		scissor.right = static_cast<LONG>(screenWidth);
		scissor.bottom = static_cast<LONG>(screenHeight);
		cmdList->RSSetScissorRects(1, &scissor);

		cmdList->SetPipelineState(pipelineState_.Get());
		cmdList->SetGraphicsRootSignature(rootSignature_.Get());

		Float constants[1] = { alpha };
		cmdList->SetGraphicsRoot32BitConstants(0, 1, constants, 0);

		cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		cmdList->DrawInstanced(3, 1, 0, 0);
	}
}
