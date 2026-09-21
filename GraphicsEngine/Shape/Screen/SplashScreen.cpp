#include <GraphicsEngine/Shape/Screen/SplashScreen.h>
#include <GraphicsEngine/Texture/TextureLoader.h>
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

		auto load = [&](const Char* name, const Char* tag, Uint& textureIndex, Microsoft::WRL::ComPtr<ID3D12Resource>& resource)
		{
			textureIndex = bindlessHeap->AllocateIndex();
			String filePath = String(std::string("../Runtime/Logo/") + name + "." + tag);
			TextureLoader::CreateTexturePath(device, cmdQueue, bindlessHeap->Heap(), filePath, resource, textureIndex);
		};

		load("Day",   "logo", dayTextureIndex_, dayResource_);
		load("Night", "logo", nightTextureIndex_, nightResource_);
		load("Warning", "sub.logo", warningTextureIndex_, warningResource_);
		load("Fiction", "sub.logo", fictionTextureIndex_, fictionResource_);
		load("CriWare", "logo", criLogoTextureIndex_, criLogoResource_);

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
		params[0].Constants.Num32BitValues = 4;
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

		auto vertexShaderResult = ShaderCompiler::CompileVertexShader(L"../GraphicsEngine/Shape/Screen/SplashScreenVS.hlsl", "main");
		auto pixelShaderResult = ShaderCompiler::CompilePixelShader(L"../GraphicsEngine/Shape/Screen/SplashScreenPS.hlsl", "main");

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

	void SplashScreen::Draw(ID3D12GraphicsCommandList6* cmdList, D3D12_CPU_DESCRIPTOR_HANDLE renderTargetViewHandle, Float screenWidth, Float screenHeight, Bool showWarning, Bool showFiction)
	{
		if (!initialized_ || finished_)
		{
			return;
		}

		if (!started_)
		{
			startTime_ = std::chrono::steady_clock::now();
			started_ = true;
			showWarning_ = showWarning;
			showFiction_ = showFiction;
		}

		auto now = std::chrono::steady_clock::now();
		Float elapsed = std::chrono::duration<Float>(now - startTime_).count();

		/// [EN] Warning -> Fiction -> CRI logo -> Engine logo, in that order.
		///      Each of the first two phases is skipped entirely (zero
		///      duration) if its show flag is false; all four single-image
		///      phases fade in/out the same way.
		/// [JP] Warning -> Fiction -> CRIロゴ -> エンジンロゴ の順。
		///      最初の2フェーズはそれぞれの表示フラグがfalseなら丸ごと
		///      スキップ（時間0）される。単一画像の4フェーズとも同じように
		///      フェードイン/アウトする。
		Float warningPhaseDuration = showWarning_ ? warningDuration_ : 0.0f;
		Float fictionPhaseDuration = showFiction_ ? fictionDuration_ : 0.0f;

		Float warningEnd = warningPhaseDuration;
		Float fictionEnd = warningEnd + fictionPhaseDuration;
		Float criLogoEnd = fictionEnd + criLogoDuration_;
		Float logoEnd = criLogoEnd + minDuration_;

		if (elapsed >= logoEnd)
		{
			finished_ = true;

			cmdQueue_->Signal();
			cmdQueue_->Wait();

			dayResource_.Reset();
			nightResource_.Reset();
			warningResource_.Reset();
			fictionResource_.Reset();
			criLogoResource_.Reset();
			bindlessHeap_->FreeIndex(dayTextureIndex_);
			bindlessHeap_->FreeIndex(nightTextureIndex_);
			bindlessHeap_->FreeIndex(warningTextureIndex_);
			bindlessHeap_->FreeIndex(fictionTextureIndex_);
			bindlessHeap_->FreeIndex(criLogoTextureIndex_);

			rootSignature_.Reset();
			pipelineState_.Reset();

			return;
		}

		Bool warningPhase = elapsed < warningEnd;
		Bool fictionPhase = !warningPhase && elapsed < fictionEnd;
		Bool criLogoPhase = !warningPhase && !fictionPhase && elapsed < criLogoEnd;

		/// [EN] Elapsed time local to whichever phase is active, and that
		///      phase's total duration - used for the shared fade in/out.
		/// [JP] 現在アクティブなフェーズを基準にしたローカル経過時間と、
		///      そのフェーズの総時間 - 共通のフェードイン/アウトに使う。
		Float phaseElapsed = warningPhase ? elapsed : (fictionPhase ? elapsed - warningEnd : (criLogoPhase ? elapsed - fictionEnd : elapsed - criLogoEnd));
		Float phaseDuration = warningPhase ? warningPhaseDuration : (fictionPhase ? fictionPhaseDuration : (criLogoPhase ? criLogoDuration_ : minDuration_));

		Float alpha = 1.0f;
		if (phaseElapsed < fadeInTime_)
		{
			alpha = phaseElapsed / fadeInTime_;
		}
		else if (phaseElapsed > phaseDuration - fadeOutTime_)
		{
			alpha = (phaseDuration - phaseElapsed) / fadeOutTime_;
		}

		/// [EN] Selects which single centered/letterboxed image is active this
		///      frame - warning/fiction/CRI-logo/day-or-night-logo - all drawn
		///      through the same t0 slot + texture_aspect_ path in the shader.
		/// [JP] このフレームでどの単一の中央寄せ/レターボックス画像を使うか
		///      選ぶ - 警告/フィクション/CRIロゴ/昼夜ロゴのいずれも、
		///      シェーダー内で同じ t0 スロット + texture_aspect_ の経路で
		///      描画される。
		Uint textureIndex = 0;
		ID3D12Resource* texResource = nullptr;

		if (warningPhase)
		{
			textureIndex = warningTextureIndex_;
			texResource = warningResource_.Get();
		}
		else if (fictionPhase)
		{
			textureIndex = fictionTextureIndex_;
			texResource = fictionResource_.Get();
		}
		else if (criLogoPhase)
		{
			textureIndex = criLogoTextureIndex_;
			texResource = criLogoResource_.Get();
		}
		else
		{
			auto systemNow = std::chrono::system_clock::now();
			std::time_t time = std::chrono::system_clock::to_time_t(systemNow);
			std::tm local{};
			localtime_s(&local, &time);
			Int hour = local.tm_hour;
			Bool isDaytime = (hour >= 6 && hour < 18);

			textureIndex = isDaytime ? dayTextureIndex_ : nightTextureIndex_;
			texResource = isDaytime ? dayResource_.Get() : nightResource_.Get();
		}

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

		Float constants[4] = { alpha, screenWidth, screenHeight, textureAspect };
		cmdList->SetGraphicsRoot32BitConstants(0, 4, constants, 0);

		ID3D12DescriptorHeap* heaps[] = { bindlessHeap_->Heap() };
		cmdList->SetDescriptorHeaps(1, heaps);
		cmdList->SetGraphicsRootDescriptorTable(1, bindlessHeap_->GPUHandle(textureIndex));

		cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		cmdList->DrawInstanced(3, 1, 0, 0);
	}

	Bool SplashScreen::Finished()const
	{
		return finished_;
	}
}
