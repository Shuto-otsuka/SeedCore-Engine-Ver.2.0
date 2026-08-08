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

		warningTextureIndex_ = bindlessHeap->AllocateIndex();
		loader.CreateTexture(device, cmdQueue->GetCommandQueue(), bindlessHeap->Heap(), String("../Runtime/Logo/seedcore_warning_notice.dds"), warningResource_, warningTextureIndex_);

		fictionTextureIndex_ = bindlessHeap->AllocateIndex();
		loader.CreateTexture(device, cmdQueue->GetCommandQueue(), bindlessHeap->Heap(), String("../Runtime/Logo/seedcore_fiction_notice.dds"), fictionResource_, fictionTextureIndex_);

		criLogoTextureIndex_ = bindlessHeap->AllocateIndex();
		loader.CreateTexture(device, cmdQueue->GetCommandQueue(), bindlessHeap->Heap(), String("../Runtime/Logo/criware_logo.dds"), criLogoResource_, criLogoTextureIndex_);

		progressBackgroundTextureIndex_ = bindlessHeap->AllocateIndex();
		loader.CreateTexture(device, cmdQueue->GetCommandQueue(), bindlessHeap->Heap(), String("../Runtime/Logo/seedcore_progress_background.dds"), progressBackgroundResource_, progressBackgroundTextureIndex_);

		progressBarTextureIndex_ = bindlessHeap->AllocateIndex();
		loader.CreateTexture(device, cmdQueue->GetCommandQueue(), bindlessHeap->Heap(), String("../Runtime/Logo/seedcore_progress_bar.dds"), progressBarResource_, progressBarTextureIndex_);

		progressFrameTextureIndex_ = bindlessHeap->AllocateIndex();
		loader.CreateTexture(device, cmdQueue->GetCommandQueue(), bindlessHeap->Heap(), String("../Runtime/Logo/seedcore_progress_frame.dds"), progressFrameResource_, progressFrameTextureIndex_);

		HRESULT hr{ S_OK };

		D3D12_DESCRIPTOR_RANGE shaderResourceViewRange{};
		shaderResourceViewRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		shaderResourceViewRange.NumDescriptors = 1;
		shaderResourceViewRange.BaseShaderRegister = 0;
		shaderResourceViewRange.RegisterSpace = 0;
		shaderResourceViewRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

		D3D12_DESCRIPTOR_RANGE progressBarRange{};
		progressBarRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		progressBarRange.NumDescriptors = 1;
		progressBarRange.BaseShaderRegister = 1;
		progressBarRange.RegisterSpace = 0;
		progressBarRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

		D3D12_DESCRIPTOR_RANGE progressFrameRange{};
		progressFrameRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		progressFrameRange.NumDescriptors = 1;
		progressFrameRange.BaseShaderRegister = 2;
		progressFrameRange.RegisterSpace = 0;
		progressFrameRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

		D3D12_DESCRIPTOR_RANGE progressBackgroundRange{};
		progressBackgroundRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		progressBackgroundRange.NumDescriptors = 1;
		progressBackgroundRange.BaseShaderRegister = 3;
		progressBackgroundRange.RegisterSpace = 0;
		progressBackgroundRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

		D3D12_ROOT_PARAMETER params[5]{};

		params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
		params[0].Constants.ShaderRegister = 0;
		params[0].Constants.RegisterSpace = 0;
		params[0].Constants.Num32BitValues = 10;
		params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

		params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		params[1].DescriptorTable.NumDescriptorRanges = 1;
		params[1].DescriptorTable.pDescriptorRanges = &shaderResourceViewRange;
		params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

		params[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		params[2].DescriptorTable.NumDescriptorRanges = 1;
		params[2].DescriptorTable.pDescriptorRanges = &progressBarRange;
		params[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

		params[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		params[3].DescriptorTable.NumDescriptorRanges = 1;
		params[3].DescriptorTable.pDescriptorRanges = &progressFrameRange;
		params[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

		params[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		params[4].DescriptorTable.NumDescriptorRanges = 1;
		params[4].DescriptorTable.pDescriptorRanges = &progressBackgroundRange;
		params[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

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
		rootSignatureDesc.NumParameters = 5;
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

	void SplashScreen::Draw(ID3D12GraphicsCommandList6* cmdList, ID3D12Resource* backBuffer, D3D12_CPU_DESCRIPTOR_HANDLE renderTargetViewHandle, Float screenWidth, Float screenHeight, Bool loadComplete, Float progress, Bool showWarning, Bool showFiction)
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

		/// [EN] Warning -> Fiction -> CRI logo -> Engine logo -> Progress, in that
		///      order. Each of the first two phases is skipped entirely (zero
		///      duration) if its show flag is false; all four single-image
		///      phases fade in/out the same way.
		/// [JP] Warning -> Fiction -> CRIロゴ -> エンジンロゴ -> Progress の順。
		///      最初の2フェーズはそれぞれの表示フラグがfalseなら丸ごと
		///      スキップ（時間0）される。単一画像の4フェーズとも同じように
		///      フェードイン/アウトする。
		Float warningPhaseDuration = showWarning_ ? warningDuration_ : 0.0f;
		Float fictionPhaseDuration = showFiction_ ? fictionDuration_ : 0.0f;

		Float warningEnd = warningPhaseDuration;
		Float fictionEnd = warningEnd + fictionPhaseDuration;
		Float criLogoEnd = fictionEnd + criLogoDuration_;
		Float logoEnd = criLogoEnd + minDuration_;

		Bool warningPhase = elapsed < warningEnd;
		Bool fictionPhase = !warningPhase && elapsed < fictionEnd;
		Bool criLogoPhase = !warningPhase && !fictionPhase && elapsed < criLogoEnd;
		Bool logoPhase = !warningPhase && !fictionPhase && !criLogoPhase && elapsed < logoEnd;

		Float alpha = 1.0f;

		if (warningPhase || fictionPhase || criLogoPhase || logoPhase)
		{
			/// [EN] Elapsed time local to whichever phase is active, and that
			///      phase's total duration - used for the shared fade in/out.
			/// [JP] 現在アクティブなフェーズを基準にしたローカル経過時間と、
			///      そのフェーズの総時間 - 共通のフェードイン/アウトに使う。
			Float phaseElapsed = warningPhase ? elapsed : (fictionPhase ? elapsed - warningEnd : (criLogoPhase ? elapsed - fictionEnd : elapsed - criLogoEnd));
			Float phaseDuration = warningPhase ? warningPhaseDuration : (fictionPhase ? fictionPhaseDuration : (criLogoPhase ? criLogoDuration_ : minDuration_));

			if (phaseElapsed < fadeInTime_)
			{
				alpha = phaseElapsed / fadeInTime_;
			}
			else if (phaseElapsed > phaseDuration - fadeOutTime_)
			{
				alpha = (phaseDuration - phaseElapsed) / fadeOutTime_;
			}
		}
		else if (loadComplete)
		{
			finished_ = true;

			cmdQueue_->Signal();
			cmdQueue_->Wait();

			dayResource_.Reset();
			nightResource_.Reset();
			warningResource_.Reset();
			fictionResource_.Reset();
			criLogoResource_.Reset();
			progressBackgroundResource_.Reset();
			progressBarResource_.Reset();
			progressFrameResource_.Reset();
			bindlessHeap_->FreeIndex(dayTextureIndex_);
			bindlessHeap_->FreeIndex(nightTextureIndex_);
			bindlessHeap_->FreeIndex(warningTextureIndex_);
			bindlessHeap_->FreeIndex(fictionTextureIndex_);
			bindlessHeap_->FreeIndex(criLogoTextureIndex_);
			bindlessHeap_->FreeIndex(progressBackgroundTextureIndex_);
			bindlessHeap_->FreeIndex(progressBarTextureIndex_);
			bindlessHeap_->FreeIndex(progressFrameTextureIndex_);

			rootSignature_.Reset();
			pipelineState_.Reset();

			return;
		}

		/// [EN] Selects which single centered/letterboxed image (if any) is
		///      active this frame - warning/fiction/CRI-logo/day-or-night-logo -
		///      all drawn through the same t0 slot + texture_aspect_/show_logo_
		///      path in the shader.
		/// [JP] このフレームでどの単一の中央寄せ/レターボックス画像を使うか
		///      選ぶ（無ければ無し） - 警告/フィクション/CRIロゴ/昼夜ロゴの
		///      いずれも、シェーダー内で同じ t0 スロット +
		///      texture_aspect_/show_logo_ の経路で描画される。
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
			/// [EN] Also computed (harmlessly unused) outside logoPhase - keeps
			///      root param 1 bound to a real Texture2D at all times, since
			///      the progress phase's show_logo_=0 means the shader never
			///      samples it, but D3D still expects a validly-typed descriptor
			///      bound there.
			/// [JP] logoPhase 以外でも（実害なく未使用のまま）計算しておく -
			///      進捗フェーズでは show_logo_=0 なのでシェーダーは t0 を
			///      サンプルしないが、D3D 側は root param 1 に常に妥当な型の
			///      デスクリプタがバインドされていることを期待するため。
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

		/// [JP] progress_bar.dds/progress_frame.dds は同じレイアウトの重ね合わせ前提なので、バーの縦横比だけ取得すれば両方に使える。
		Float barAspect = 1.0f;
		if (progressBarResource_)
		{
			D3D12_RESOURCE_DESC desc = progressBarResource_->GetDesc();
			barAspect = static_cast<Float>(desc.Width) / static_cast<Float>(desc.Height);
		}

		Float backgroundAspect = 1.0f;
		if (progressBackgroundResource_)
		{
			D3D12_RESOURCE_DESC desc = progressBackgroundResource_->GetDesc();
			backgroundAspect = static_cast<Float>(desc.Width) / static_cast<Float>(desc.Height);
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

		Float showLogo = (warningPhase || fictionPhase || criLogoPhase || logoPhase) ? 1.0f : 0.0f;

		/// [EN] Past all four single-image phases (and not yet complete, since
		///      loadComplete already returned above) means the loading progress
		///      bar is on screen instead.
		/// [JP] 4つの単一画像フェーズを全て過ぎていて（かつ loadComplete なら
		///      ここまでに return しているので）まだ完了していない場合、
		///      代わりにロード進捗バーを表示する。
		Float showProgress = showLogo > 0.5f ? 0.0f : 1.0f;

		Float constants[10] = { alpha, screenWidth, screenHeight, textureAspect, showLogo, progress, showProgress, barAspect, backgroundAspect, elapsed };
		cmdList->SetGraphicsRoot32BitConstants(0, 10, constants, 0);

		ID3D12DescriptorHeap* heaps[] = { bindlessHeap_->Heap() };
		cmdList->SetDescriptorHeaps(1, heaps);
		cmdList->SetGraphicsRootDescriptorTable(1, bindlessHeap_->GPUHandle(textureIndex));
		cmdList->SetGraphicsRootDescriptorTable(2, bindlessHeap_->GPUHandle(progressBarTextureIndex_));
		cmdList->SetGraphicsRootDescriptorTable(3, bindlessHeap_->GPUHandle(progressFrameTextureIndex_));
		cmdList->SetGraphicsRootDescriptorTable(4, bindlessHeap_->GPUHandle(progressBackgroundTextureIndex_));

		cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		cmdList->DrawInstanced(3, 1, 0, 0);
	}

	Bool SplashScreen::IsFinished()const
	{
		return finished_;
	}
}
