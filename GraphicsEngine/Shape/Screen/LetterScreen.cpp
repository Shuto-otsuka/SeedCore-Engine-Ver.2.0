#include <GraphicsEngine/Shape/Screen/LetterScreen.h>
#include <GraphicsEngine/D3D12/Descriptor/BindlessHeap.h>
#include <GraphicsEngine/Shader/ShaderCompiler.h>
#include <FoundationEngine/Log/DxFail.h>

namespace SeedCore
{
	void LetterScreen::Initialize(ID3D12Device* device, BindlessHeap* bindlessHeap)
	{
		device_ = device;
		bindlessHeap_ = bindlessHeap;
		sourceTextureIndex_ = bindlessHeap->AllocateIndex();

		HRESULT hr{ S_OK };

		D3D12_DESCRIPTOR_RANGE shaderResourceViewRange{};
		shaderResourceViewRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		shaderResourceViewRange.NumDescriptors = 1;
		shaderResourceViewRange.BaseShaderRegister = 0;
		shaderResourceViewRange.RegisterSpace = 0;
		shaderResourceViewRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

		D3D12_ROOT_PARAMETER params[1]{};

		params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		params[0].DescriptorTable.NumDescriptorRanges = 1;
		params[0].DescriptorTable.pDescriptorRanges = &shaderResourceViewRange;
		params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

		D3D12_STATIC_SAMPLER_DESC sampler{};
		sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
		sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		sampler.MipLODBias = 0.0f;
		sampler.MaxAnisotropy = 1;
		sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
		sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK;
		sampler.MinLOD = 0.0f;
		sampler.MaxLOD = D3D12_FLOAT32_MAX;
		sampler.ShaderRegister = 0;
		sampler.RegisterSpace = 0;
		sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

		D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc{};
		rootSignatureDesc.NumParameters = 1;
		rootSignatureDesc.pParameters = params;
		rootSignatureDesc.NumStaticSamplers = 1;
		rootSignatureDesc.pStaticSamplers = &sampler;
		rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

		Microsoft::WRL::ComPtr<ID3DBlob> serialized;
		Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
		hr = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &serialized, &errorBlob);
		SC_HR_CHECK(hr, "LetterScreen RootSignatureのシリアライズに失敗しました");

		hr = device->CreateRootSignature(0, serialized->GetBufferPointer(), serialized->GetBufferSize(), IID_PPV_ARGS(&rootSignature_));
		SC_HR_CHECK(hr, "LetterScreen RootSignatureの生成に失敗しました");

		auto vertexShaderResult = ShaderCompiler::CompileVertexShader(L"../GraphicsEngine/Shape/Screen/SplashScreenVS.hlsl", "main");
		auto pixelShaderResult = ShaderCompiler::CompilePixelShader(L"../GraphicsEngine/Shape/Screen/LetterScreenPS.hlsl", "main");

		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
		psoDesc.pRootSignature = rootSignature_.Get();
		psoDesc.VS = { vertexShaderResult.objectBlob->GetBufferPointer(), vertexShaderResult.objectBlob->GetBufferSize() };
		psoDesc.PS = { pixelShaderResult.objectBlob->GetBufferPointer(), pixelShaderResult.objectBlob->GetBufferSize() };

		psoDesc.BlendState.AlphaToCoverageEnable = FALSE;
		psoDesc.BlendState.IndependentBlendEnable = FALSE;
		psoDesc.BlendState.RenderTarget[0].BlendEnable = FALSE;
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
		psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		psoDesc.SampleDesc.Count = 1;
		psoDesc.SampleDesc.Quality = 0;

		hr = device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipelineState_));
		SC_HR_CHECK(hr, "LetterScreen PipelineStateの生成に失敗しました");

		initialized_ = true;
	}

	void LetterScreen::Finalize()
	{
		if (bindlessHeap_)
		{
			bindlessHeap_->FreeIndex(sourceTextureIndex_);
			bindlessHeap_ = nullptr;
		}

		rootSignature_.Reset();
		pipelineState_.Reset();

		device_ = nullptr;
		initialized_ = false;
	}

	void LetterScreen::Draw(ID3D12GraphicsCommandList6* cmdList, ID3D12Resource* sourceResource, D3D12_CPU_DESCRIPTOR_HANDLE renderTargetViewHandle, Float screenWidth, Float screenHeight)
	{
		if (!initialized_ || !sourceResource)
		{
			return;
		}

		D3D12_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDesc{};
		shaderResourceViewDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		shaderResourceViewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		shaderResourceViewDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		shaderResourceViewDesc.Texture2D.MipLevels = 1;
		device_->CreateShaderResourceView(sourceResource, &shaderResourceViewDesc, bindlessHeap_->CPUHandle(sourceTextureIndex_));

		D3D12_RESOURCE_DESC sourceDesc = sourceResource->GetDesc();
		Float sourceWidth = static_cast<Float>(sourceDesc.Width);
		Float sourceHeight = static_cast<Float>(sourceDesc.Height);

		Float scale = Min(screenWidth / sourceWidth, screenHeight / sourceHeight);
		Float letterWidth = sourceWidth * scale;
		Float letterHeight = sourceHeight * scale;

		cmdList->OMSetRenderTargets(1, &renderTargetViewHandle, FALSE, nullptr);

		const Float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
		cmdList->ClearRenderTargetView(renderTargetViewHandle, clearColor, 0, nullptr);

		D3D12_VIEWPORT viewport{};
		viewport.TopLeftX = (screenWidth - letterWidth) * 0.5f;
		viewport.TopLeftY = (screenHeight - letterHeight) * 0.5f;
		viewport.Width = letterWidth;
		viewport.Height = letterHeight;
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

		ID3D12DescriptorHeap* heaps[] = { bindlessHeap_->Heap() };
		cmdList->SetDescriptorHeaps(1, heaps);
		cmdList->SetGraphicsRootDescriptorTable(0, bindlessHeap_->GPUHandle(sourceTextureIndex_));

		cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		cmdList->DrawInstanced(3, 1, 0, 0);
	}
}
