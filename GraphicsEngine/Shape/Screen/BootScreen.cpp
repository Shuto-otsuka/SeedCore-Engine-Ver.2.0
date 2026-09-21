#include <GraphicsEngine/Shape/Screen/BootScreen.h>
#include <GraphicsEngine/Texture/TextureLoader.h>
#include <GraphicsEngine/D3D12/Descriptor/BindlessHeap.h>
#include <GraphicsEngine/D3D12/Context/D3D12CommandQueue.h>
#include <GraphicsEngine/Shader/ShaderCompiler.h>
#include <FoundationEngine/Log/DxFail.h>

namespace SeedCore
{
	void BootScreen::Initialize(ID3D12Device* device, D3D12CommandQueue* cmdQueue, BindlessHeap* bindlessHeap, DXGI_FORMAT renderTargetFormat)
	{
		device_ = device;
		cmdQueue_ = cmdQueue;
		bindlessHeap_ = bindlessHeap;

		HRESULT hr{ S_OK };

		D3D12_DESCRIPTOR_RANGE backgroundRange{};
		backgroundRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		backgroundRange.NumDescriptors = 1;
		backgroundRange.BaseShaderRegister = 0;
		backgroundRange.RegisterSpace = 0;
		backgroundRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

		D3D12_DESCRIPTOR_RANGE barRange{};
		barRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		barRange.NumDescriptors = 1;
		barRange.BaseShaderRegister = 1;
		barRange.RegisterSpace = 0;
		barRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

		D3D12_DESCRIPTOR_RANGE frameRange{};
		frameRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		frameRange.NumDescriptors = 1;
		frameRange.BaseShaderRegister = 2;
		frameRange.RegisterSpace = 0;
		frameRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

		D3D12_ROOT_PARAMETER params[4]{};

		params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
		params[0].Constants.ShaderRegister = 0;
		params[0].Constants.RegisterSpace = 0;
		params[0].Constants.Num32BitValues = sizeof(BootScreenConstantBuffer) / sizeof(Uint32);
		params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

		params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		params[1].DescriptorTable.NumDescriptorRanges = 1;
		params[1].DescriptorTable.pDescriptorRanges = &backgroundRange;
		params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

		params[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		params[2].DescriptorTable.NumDescriptorRanges = 1;
		params[2].DescriptorTable.pDescriptorRanges = &barRange;
		params[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

		params[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		params[3].DescriptorTable.NumDescriptorRanges = 1;
		params[3].DescriptorTable.pDescriptorRanges = &frameRange;
		params[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

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
		rootSignatureDesc.NumParameters = 4;
		rootSignatureDesc.pParameters = params;
		rootSignatureDesc.NumStaticSamplers = 1;
		rootSignatureDesc.pStaticSamplers = &sampler;
		rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

		Microsoft::WRL::ComPtr<ID3DBlob> serialized;
		Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
		hr = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &serialized, &errorBlob);
		SC_HR_CHECK(hr, "BootScreen RootSignatureのシリアライズに失敗しました");

		hr = device->CreateRootSignature(0, serialized->GetBufferPointer(), serialized->GetBufferSize(), IID_PPV_ARGS(&rootSignature_));
		SC_HR_CHECK(hr, "BootScreen RootSignatureの生成に失敗しました");

		auto vertexShaderResult = ShaderCompiler::CompileVertexShader(L"../GraphicsEngine/Shape/Screen/SplashScreenVS.hlsl", "main");
		auto pixelShaderResult = ShaderCompiler::CompilePixelShader(L"../GraphicsEngine/Shape/Screen/BootScreenPS.hlsl", "main");

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
		psoDesc.RTVFormats[0] = renderTargetFormat;
		psoDesc.SampleDesc.Count = 1;
		psoDesc.SampleDesc.Quality = 0;

		hr = device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipelineState_));
		SC_HR_CHECK(hr, "BootScreen PipelineStateの生成に失敗しました");

		initialized_ = true;
	}

	void BootScreen::Finalize()
	{
		if (bindlessHeap_)
		{
			ReleaseImage(backgroundTextureIndex_, backgroundResource_);
			ReleaseImage(barTextureIndex_, barResource_);
			ReleaseImage(frameTextureIndex_, frameResource_);
		}

		rootSignature_.Reset();
		pipelineState_.Reset();

		device_ = nullptr;
		cmdQueue_ = nullptr;
		bindlessHeap_ = nullptr;
		initialized_ = false;
	}

	void BootScreen::LoadImages(const BootConfig& config)
	{
		if (!initialized_)
		{
			return;
		}

		ReplaceImage(config.backgroundImage_, "ProgressBackground", backgroundTextureIndex_, backgroundResource_);
		ReplaceImage(config.barImage_, "ProgressBar", barTextureIndex_, barResource_);
		ReplaceImage(config.frameImage_, "ProgressFrame", frameTextureIndex_, frameResource_);
	}

	void BootScreen::Draw(ID3D12GraphicsCommandList6* cmdList, D3D12_CPU_DESCRIPTOR_HANDLE renderTargetViewHandle, const BootConfig& config, Float screenWidth, Float screenHeight, Float progress, Float time, Float alpha)
	{
		if (!initialized_ || backgroundTextureIndex_ == invalidIndex_)
		{
			return;
		}

		BootScreenConstantBuffer constants{};
		constants.screenSize_ = Vector2(screenWidth, screenHeight);
		constants.alpha_ = alpha;
		constants.progress_ = progress;
		constants.time_ = time;
		constants.backgroundAspect_ = ImageAspect(backgroundResource_);
		constants.fillMethod_ = static_cast<Uint>(config.fillMethod_);
		constants.fillOrigin_ = static_cast<Uint>(Max(config.fillOrigin_, 0));
		constants.clockwise_ = config.clockwise_ ? 1 : 0;
		constants.wave_ = config.wave_ ? 1 : 0;
		constants.useBackgroundImage_ = config.useBackgroundImage_ ? 1 : 0;
		constants.useFrame_ = config.useFrame_ ? 1 : 0;
		constants.barRect_ = BarRect(config, screenWidth, screenHeight, BarAspect());
		constants.backgroundColor_ = Vector4(config.backgroundColor_.x, config.backgroundColor_.y, config.backgroundColor_.z, config.backgroundColor_.w);
		constants.backgroundTint_ = Vector4(config.backgroundTint_.x, config.backgroundTint_.y, config.backgroundTint_.z, config.backgroundTint_.w);
		constants.barTint_ = Vector4(config.barTint_.x, config.barTint_.y, config.barTint_.z, config.barTint_.w);
		constants.frameTint_ = Vector4(config.frameTint_.x, config.frameTint_.y, config.frameTint_.z, config.frameTint_.w);

		cmdList->OMSetRenderTargets(1, &renderTargetViewHandle, FALSE, nullptr);

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
		cmdList->SetGraphicsRoot32BitConstants(0, sizeof(BootScreenConstantBuffer) / sizeof(Uint32), &constants, 0);

		ID3D12DescriptorHeap* heaps[] = { bindlessHeap_->Heap() };
		cmdList->SetDescriptorHeaps(1, heaps);
		cmdList->SetGraphicsRootDescriptorTable(1, bindlessHeap_->GPUHandle(backgroundTextureIndex_));
		cmdList->SetGraphicsRootDescriptorTable(2, bindlessHeap_->GPUHandle(barTextureIndex_));
		cmdList->SetGraphicsRootDescriptorTable(3, bindlessHeap_->GPUHandle(frameTextureIndex_));

		cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		cmdList->DrawInstanced(3, 1, 0, 0);
	}

	Bool BootScreen::Initialized()const
	{
		return initialized_;
	}

	Float BootScreen::BarAspect()const
	{
		return ImageAspect(barResource_);
	}

	Vector4 BootScreen::BarRect(const BootConfig& config, Float screenWidth, Float screenHeight, Float barAspect)
	{
		Int32 anchorIndex = static_cast<Int32>(config.anchor_);
		Vector2 anchor = Vector2(static_cast<Float>(anchorIndex % 3) * 0.5f, static_cast<Float>(anchorIndex / 3) * 0.5f);

		Vector2 size = Vector2(config.width_, config.width_ * screenWidth / (Max(barAspect, 0.0001f) * Max(screenHeight, 1.0f)));
		Vector2 barMin = anchor + config.offset_ - anchor * size;

		return Vector4(barMin.x, barMin.y, barMin.x + size.x, barMin.y + size.y);
	}

	void BootScreen::ReplaceImage(const DynamicArray<Byte>& image, const Char* defaultName, Uint& textureIndex, Microsoft::WRL::ComPtr<ID3D12Resource>& resource)
	{
		ReleaseImage(textureIndex, resource);

		textureIndex = bindlessHeap_->AllocateIndex();

		if (!image.empty())
		{
			TextureLoader::CreateTextureMemory(device_, cmdQueue_, bindlessHeap_->Heap(), image, resource, textureIndex);
		}

		if (!resource)
		{
			String filePath = String(std::string("../Runtime/Logo/") + defaultName + ".sub.logo");
			TextureLoader::CreateTexturePath(device_, cmdQueue_, bindlessHeap_->Heap(), filePath, resource, textureIndex);
		}

		if (!resource)
		{
			D3D12_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDesc{};
			shaderResourceViewDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			shaderResourceViewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			shaderResourceViewDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			shaderResourceViewDesc.Texture2D.MipLevels = 1;
			device_->CreateShaderResourceView(nullptr, &shaderResourceViewDesc, bindlessHeap_->CPUHandle(textureIndex));
		}
	}

	void BootScreen::ReleaseImage(Uint& textureIndex, Microsoft::WRL::ComPtr<ID3D12Resource>& resource)
	{
		if (resource)
		{
			bindlessHeap_->DeferRelease(resource);
			resource.Reset();
		}

		if (textureIndex != invalidIndex_)
		{
			bindlessHeap_->FreeIndex(textureIndex);
			textureIndex = invalidIndex_;
		}
	}

	Float BootScreen::ImageAspect(const Microsoft::WRL::ComPtr<ID3D12Resource>& resource)
	{
		if (!resource)
		{
			return 1.0f;
		}

		D3D12_RESOURCE_DESC desc = resource->GetDesc();
		return static_cast<Float>(desc.Width) / static_cast<Float>(Max<Uint32>(desc.Height, 1));
	}
}
