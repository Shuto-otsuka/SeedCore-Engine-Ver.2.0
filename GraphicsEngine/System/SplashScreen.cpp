#include <GraphicsEngine/System/SplashScreen.h>
#include <GraphicsEngine/Resource/TextureLoader.h>
#include <GraphicsEngine/D3D12/Descriptor/BindlessHeap.h>
#include <GraphicsEngine/D3D12/Context/D3D12CommandQueue.h>
#include <GraphicsEngine/Shader/ShaderCompiler.h>
#include <FoundationEngine/Log/DxFail.h>
#include <FoundationEngine/Log/Notice.h>

namespace SeedCore
{
	void SplashScreen::Initialize(ID3D12Device* device, D3D12CommandQueue* cmdQueue, BindlessHeap* bindlessHeap)
	{
		bindlessHeap_ = bindlessHeap;
		cmdQueue_ = cmdQueue;

		TextureLoader loader;

		dayTextureIndex_ = bindlessHeap->AllocateIndex();
		loader.CreateTexture(device, cmdQueue->GetCommandQueue(), bindlessHeap->Heap(), String("../Runtime/Logo/day_logo.dds"), dayResource_, dayTextureIndex_);

		nightTextureIndex_ = bindlessHeap->AllocateIndex();
		loader.CreateTexture(device, cmdQueue->GetCommandQueue(), bindlessHeap->Heap(), String("../Runtime/Logo/night_logo.dds"), nightResource_, nightTextureIndex_);

		HRESULT hr{ S_OK };

		D3D12_DESCRIPTOR_RANGE shaderResourceViewRange{};
		shaderResourceViewRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		shaderResourceViewRange.NumDescriptors = 1;
		shaderResourceViewRange.BaseShaderRegister = 0;
		shaderResourceViewRange.RegisterSpace = 0;
		shaderResourceViewRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

		D3D12_ROOT_PARAMETER params[2]{};

		params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
		params[0].Constants.ShaderRegister = 0;
		params[0].Constants.RegisterSpace = 0;
		params[0].Constants.Num32BitValues = 5;
		params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

		params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		params[1].DescriptorTable.NumDescriptorRanges = 1;
		params[1].DescriptorTable.pDescriptorRanges = &shaderResourceViewRange;
		params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

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
		rootSignatureDesc.NumParameters = 2;
		rootSignatureDesc.pParameters = params;
		rootSignatureDesc.NumStaticSamplers = 1;
		rootSignatureDesc.pStaticSamplers = &sampler;
		rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

		Microsoft::WRL::ComPtr<ID3DBlob> serialized;
		Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
		hr = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &serialized, &errorBlob);
		SC_HR_CHECK(hr, "SplashScreen RootSignatureのシリアライズに失敗しました");

		hr = device->CreateRootSignature(0, serialized->GetBufferPointer(), serialized->GetBufferSize(), IID_PPV_ARGS(&rootSignature_));
		SC_HR_CHECK(hr, "SplashScreen RootSignatureの生成に失敗しました");

		auto vertexShaderResult = ShaderCompiler::CompileVertexShader(L"../GraphicsEngine/System/SplashScreenVS.hlsl", "main");
		auto pixelShaderResult = ShaderCompiler::CompilePixelShader(L"../GraphicsEngine/System/SplashScreenPS.hlsl", "main");

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
		psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		psoDesc.SampleDesc.Count = 1;
		psoDesc.SampleDesc.Quality = 0;

		hr = device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipelineState_));
		SC_HR_CHECK(hr, "SplashScreen PipelineStateの生成に失敗しました");

		initialized_ = true;
		finished_ = false;
		started_ = false;

		SC_LOG_NOTICE("スプラッシュスクリーンを初期化しました");
	}

	void SplashScreen::Draw(ID3D12GraphicsCommandList6* cmdList, ID3D12Resource* backBuffer, D3D12_CPU_DESCRIPTOR_HANDLE renderTargetViewHandle, Float screenWidth, Float screenHeight, Bool loadComplete)
	{
		if (!initialized_ || finished_)
		{
			return;
		}

		if (!started_)
		{
			startTime_ = std::chrono::steady_clock::now();
			started_ = true;
		}

		auto now = std::chrono::steady_clock::now();
		Float elapsed = std::chrono::duration<Float>(now - startTime_).count();

		Bool logoPhase = elapsed < minDuration_;
		Float alpha = 1.0f;

		if (logoPhase)
		{
			if (elapsed < fadeInTime_)
			{
				alpha = elapsed / fadeInTime_;
			}
			else if (elapsed > minDuration_ - fadeOutTime_)
			{
				alpha = (minDuration_ - elapsed) / fadeOutTime_;
			}
		}
		else if (loadComplete)
		{
			finished_ = true;

			cmdQueue_->Signal();
			cmdQueue_->Wait();

			dayResource_.Reset();
			nightResource_.Reset();
			bindlessHeap_->FreeIndex(dayTextureIndex_);
			bindlessHeap_->FreeIndex(nightTextureIndex_);

			rootSignature_.Reset();
			pipelineState_.Reset();

			return;
		}

		auto systemNow = std::chrono::system_clock::now();
		std::time_t time = std::chrono::system_clock::to_time_t(systemNow);
		std::tm local{};
		localtime_s(&local, &time);
		Int hour = local.tm_hour;
		Bool isDaytime = (hour >= 6 && hour < 18);

		Uint textureIndex = isDaytime ? dayTextureIndex_ : nightTextureIndex_;
		ID3D12Resource* texResource = isDaytime ? dayResource_.Get() : nightResource_.Get();

		Float textureAspect = 1.0f;
		if (texResource)
		{
			D3D12_RESOURCE_DESC desc = texResource->GetDesc();
			textureAspect = static_cast<Float>(desc.Width) / static_cast<Float>(desc.Height);
		}

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

		Float showLogo = logoPhase ? 1.0f : 0.0f;

		Float constants[5] = { alpha, screenWidth, screenHeight, textureAspect, showLogo };
		cmdList->SetGraphicsRoot32BitConstants(0, 5, constants, 0);

		ID3D12DescriptorHeap* heaps[] = { bindlessHeap_->Heap() };
		cmdList->SetDescriptorHeaps(1, heaps);
		cmdList->SetGraphicsRootDescriptorTable(1, bindlessHeap_->GPUHandle(textureIndex));

		cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		cmdList->DrawInstanced(3, 1, 0, 0);
	}

	Bool SplashScreen::IsFinished()const
	{
		return finished_;
	}
}
