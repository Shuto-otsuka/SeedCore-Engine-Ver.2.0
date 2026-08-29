#include <GraphicsEngine/DLSS/DlssManager.h>
#include <FoundationEngine/Log/SlFail.h>
#include <FoundationEngine/File/FileDirectory.h>
#include <FoundationEngine/Log/Warning.h>

#ifndef _DEBUG
#include <External/DLSS/Include/sl_security.h>
#endif

namespace SeedCore
{
#if !SC_RENDER_DOC_USAGE
	namespace
	{
		sl::float4x4 ToSlMatrix(const Matrix& m)
		{
			sl::float4x4 result{};
			result.setRow(0, sl::float4(m._11, m._12, m._13, m._14));
			result.setRow(1, sl::float4(m._21, m._22, m._23, m._24));
			result.setRow(2, sl::float4(m._31, m._32, m._33, m._34));
			result.setRow(3, sl::float4(m._41, m._42, m._43, m._44));
			return result;
		}

		sl::DLSSMode ToStreamlineDlssMode(UpscaleMode mode)
		{
			switch (mode)
			{
			case UpscaleMode::MaxPerformance:
				return sl::DLSSMode::eMaxPerformance;
			case UpscaleMode::Balanced:
				return sl::DLSSMode::eBalanced;
			case UpscaleMode::MaxQuality:
				return sl::DLSSMode::eMaxQuality;
			case UpscaleMode::UltraPerformance:
				return sl::DLSSMode::eUltraPerformance;
			case UpscaleMode::Dlaa:
				return sl::DLSSMode::eDLAA;
			}
			return sl::DLSSMode::eBalanced;
		}
	}
#endif

	Bool DlssManager::Initialize()
	{
#if !SC_RENDER_DOC_USAGE
		sl::Result sr{ sl::Result::eOk };

		/// [EN] sl.interposer.dll intercepts every D3D12/Vulkan call the engine makes, so verify it is the genuine NVIDIA-signed binary before slInit loads it - a replaced module at this location could hijack the entire graphics pipeline. Skipped in Debug, since development/self-built SL DLLs are unsigned per the Streamline security guide.
		/// [JP] sl.interposer.dllはエンジンが行う全てのD3D12/Vulkan呼び出しを横取りするため、slInitで読み込む前に本物のNVIDIA署名付きバイナリか検証する - ここが差し替えられるとグラフィックスパイプライン全体を乗っ取られかねない。開発版/自前ビルドのSL DLLは署名が無いため(Streamlineセキュリティガイド)、Debugではスキップする。
#ifndef _DEBUG
		std::wstring interposerPath = FileDirectory::ExecutableDirectory() + L"\\sl.interposer.dll";
		if (!sl::security::verifyEmbeddedSignature(interposerPath.c_str()))
		{
			SC_LOG_WARNING("sl.interposer.dllの署名検証に失敗しました - 改ざんされた/非公式のモジュールの可能性があります");
			return false;
		}
#endif

		sl::Feature features[] =
		{
			sl::kFeatureDLSS,
			sl::kFeatureReflex,
			sl::kFeatureDeepDVC,
			sl::kFeatureDLSS_G,
			sl::kFeatureDLSS_RR,
		};

		sl::Preferences preference{};
		preference.showConsole = false;
		//preference.showConsole = true;
#ifdef _DEBUG
		//preference.logLevel = sl::LogLevel::eDefault;
		preference.logLevel = sl::LogLevel::eVerbose;
#else
		preference.logLevel = sl::LogLevel::eOff;
#endif
		preference.pathsToPlugins = nullptr;
		preference.numPathsToPlugins = 0;
		preference.pathToLogsAndData = nullptr;
		preference.logMessageCallback = ScDlssErrorCallback;
		preference.featuresToLoad = features;
		preference.numFeaturesToLoad = static_cast<Uint32>(std::size(features));
		preference.engine = sl::EngineType::eCustom;
		preference.engineVersion = "2.0";
		preference.projectId = SC_ENCRYPTION_KEY_SEED;
		preference.renderAPI = sl::RenderAPI::eD3D12;
		preference.flags = sl::PreferenceFlags::eDisableCLStateTracking | sl::PreferenceFlags::eAllowOTA | sl::PreferenceFlags::eLoadDownloadedPlugins | sl::PreferenceFlags::eUseFrameBasedResourceTagging | sl::PreferenceFlags::eDisableDebugText;

		sr = slInit(preference);
		SC_SL_CHECK(sr, "Streamlineエンジンの初期化に失敗しました。アプリケーションIDやプラグインパスを確認してください。");

		sr = slIsFeatureLoaded(sl::kFeatureDLSS, loadResult_.isDlss_);
		SC_SL_CHECK(sr, "DLSS機能のロード状態を確認できませんでした。");

		sr = slIsFeatureLoaded(sl::kFeatureReflex, loadResult_.isReflex_);
		SC_SL_CHECK(sr, "Reflex機能のロード状態を確認できませんでした。");

		sr = slIsFeatureLoaded(sl::kFeatureDeepDVC, loadResult_.isDeepDVC_);
		SC_SL_CHECK(sr, "Deep DVC機能のロード状態を確認できませんでした。");

		sr = slIsFeatureLoaded(sl::kFeatureDLSS_G, loadResult_.isDlssFG_);
		SC_SL_CHECK(sr, "DLSS FG機能のロード状態を確認できませんでした。");

		sr = slIsFeatureLoaded(sl::kFeatureDLSS_RR, loadResult_.isDlssRR_);
		SC_SL_CHECK(sr, "DLSS RR機能のロード状態を確認できませんでした。");
#endif
		return true;
	}

	void DlssManager::Finalize()
	{
#if !SC_RENDER_DOC_USAGE
		sl::Result sr{ sl::Result::eOk };

		/// [EN] sl.deepdvc (Streamline SDK 2.12.0) does not export a freeResources callback, so this call is a no-op until NVIDIA implements it.
		/// [JP] sl.deepdvc(Streamline SDK 2.12.0)はfreeResourcesコールバックをエクスポートしていないため、NVIDIA側が実装するまではこの呼び出しは何もしない。
		//slFreeResources(sl::kFeatureDeepDVC, sl::ViewportHandle(0));
		//slFreeResources(sl::kFeatureDeepDVC, sl::ViewportHandle(1));

		if (loadResult_.isDeepDVC_)
		{
			sr = slSetFeatureLoaded(sl::kFeatureDeepDVC, false);
			SC_SL_CHECK(sr, "DeepDVC機能のアンロードに失敗しました。");
		}
		
		if (loadResult_.isDlssFG_)
		{
			sr = slSetFeatureLoaded(sl::kFeatureDLSS_G, false);
			SC_SL_CHECK(sr, "DLSS FG機能のアンロードに失敗しました。");
		}

		sr = slShutdown();
		SC_SL_CHECK(sr, "Streamlineのシャットダウン中にエラーが発生しました。");
#endif
	}

	Bool DlssManager::Prepare(ID3D12Device* device)
	{
#if !SC_RENDER_DOC_USAGE
		sl::Result sr{ sl::Result::eOk };

		sr = slSetD3DDevice(device);
		SC_SL_CHECK(sr, "Direct3DデバイスをStreamlineに登録できませんでした。");

		sl::AdapterInfo dlssData{};
		memset(&dlssData, 0, sizeof(sl::AdapterInfo));

		LUID luid = device->GetAdapterLuid();
		dlssData.deviceLUID = reinterpret_cast<Uint8*>(&luid);
		dlssData.deviceLUIDSizeInBytes = sizeof(LUID);

		sr = slIsFeatureSupported(sl::kFeatureDLSS, dlssData);
		SC_SL_CHECK(sr, "DLSS機能が現在のGPUでサポートされているか確認できませんでした。");

		sr = slIsFeatureSupported(sl::kFeatureReflex, dlssData);
		SC_SL_CHECK(sr, "Reflex機能が現在のGPUでサポートされているか確認できませんでした。");

		sr = slIsFeatureSupported(sl::kFeatureDeepDVC, dlssData);
		SC_SL_CHECK(sr, "Deep DVC機能が現在のGPUでサポートされているか確認できませんでした。");

		sr = slIsFeatureSupported(sl::kFeatureDLSS_G, dlssData);
		SC_SL_CHECK(sr, "DLSS FG機能が現在のGPUでサポートされているか確認できませんでした。");

		sr = slIsFeatureSupported(sl::kFeatureDLSS_RR, dlssData);
		SC_SL_CHECK(sr, "DLSS RR機能が現在のGPUでサポートされているか確認できませんでした。");
#endif
		return true;
	}

	void DlssManager::BeginFrame()
	{
#if !SC_RENDER_DOC_USAGE
		sl::Result sr = slGetNewFrameToken(currentToken_);
		SC_SL_CHECK(sr, "フレームトークンの取得に失敗しました");
#endif
	}

	void DlssManager::RayReconstructionTag(DlssBufferTag tags, ID3D12CommandList* cmdList, ID3D12Resource* outPutBuffer, Uint32 viewportIndex, Uint32 outputWidth, Uint32 outputHeight)
	{
#if !SC_RENDER_DOC_USAGE
		if (!currentToken_)
		{
			return;
		}

		sl::Result sr{ sl::Result::eOk };
		sl::ViewportHandle viewport(viewportIndex);

		sl::Resource colorResource           = { sl::ResourceType::eTex2d, tags.colorBuffer_,           static_cast<Uint32>(tags.colorBufferState_) };
		sl::Resource depthResource           = { sl::ResourceType::eTex2d, tags.depthBuffer_,           static_cast<Uint32>(tags.depthBufferState_) };
		sl::Resource velocityResource        = { sl::ResourceType::eTex2d, tags.velocityBuffer_,        static_cast<Uint32>(tags.velocityBufferState_) };
		sl::Resource albedoResource          = { sl::ResourceType::eTex2d, tags.albedoBuffer_,          static_cast<Uint32>(tags.albedoBufferState_) };
		sl::Resource normalRoughnessResource = { sl::ResourceType::eTex2d, tags.normalRoughnessBuffer_, static_cast<Uint32>(tags.normalRoughnessBufferState_) };
		sl::Resource specularAlbedoResource  = { sl::ResourceType::eTex2d, tags.specularAlbedoBuffer_,  static_cast<Uint32>(tags.specularAlbedoBufferState_) };
		sl::Resource outputResource          = { sl::ResourceType::eTex2d, outPutBuffer,                static_cast<Uint32>(D3D12_RESOURCE_STATE_UNORDERED_ACCESS) };

		sl::Extent inputSize{};
		inputSize.top = 0;
		inputSize.left = 0;
		inputSize.width = static_cast<Uint32>(tags.width_);
		inputSize.height = static_cast<Uint32>(tags.height_);

		sl::Extent outputSize{};
		outputSize.top = 0;
		outputSize.left = 0;
		outputSize.width = outputWidth;
		outputSize.height = outputHeight;

		/// [EN] DLSS-G reads the same Depth/MotionVectors buffers at Present() time, after slEvaluateFeature has already returned, so while FG is enabled these two need the wider eValidUntilPresent lifecycle instead of eValidUntilEvaluate.
		/// [JP] DLSS-G(FG)はslEvaluateFeatureが戻った後のPresent()時にも同じDepth/MotionVectorsバッファを読むため、FG有効時はこの2つだけeValidUntilEvaluateではなく、より広いeValidUntilPresentが必要。
		sl::ResourceLifecycle depthMotionVectorsLifecycle = frameGenerationEnabled_ ? sl::ResourceLifecycle::eValidUntilPresent : sl::ResourceLifecycle::eValidUntilEvaluate;

		tags_[0] = { &colorResource,           sl::kBufferTypeScalingInputColor,  sl::ResourceLifecycle::eValidUntilEvaluate, &inputSize };
		tags_[1] = { &depthResource,           sl::kBufferTypeDepth,              depthMotionVectorsLifecycle,                &inputSize };
		tags_[2] = { &velocityResource,        sl::kBufferTypeMotionVectors,      depthMotionVectorsLifecycle,                &inputSize };
		tags_[3] = { &albedoResource,          sl::kBufferTypeAlbedo,             sl::ResourceLifecycle::eValidUntilEvaluate, &inputSize };
		tags_[4] = { &normalRoughnessResource, sl::kBufferTypeNormalRoughness,    sl::ResourceLifecycle::eValidUntilEvaluate, &inputSize };
		tags_[5] = { &specularAlbedoResource,  sl::kBufferTypeSpecularAlbedo,     sl::ResourceLifecycle::eValidUntilEvaluate, &inputSize };
		tags_[6] = { &outputResource,          sl::kBufferTypeScalingOutputColor, sl::ResourceLifecycle::eValidUntilEvaluate, &outputSize };

		sr = slSetTagForFrame(*currentToken_, viewport, tags_, static_cast<Uint32>(std::size(tags_)), cmdList);
		SC_SL_CHECK(sr, "フレームへのタグ設定に失敗しました");
#endif
	}

	void DlssManager::FrameGenerationTag(DlssBufferTag tags, ID3D12CommandList* cmdList, ID3D12Resource* hudlessResource, ID3D12Resource* uiColorAlphaResource, Uint32 viewportIndex, Uint32 outputWidth, Uint32 outputHeight)
	{
#if !SC_RENDER_DOC_USAGE
		if (!currentToken_)
		{
			return;
		}

		sl::ViewportHandle viewport(viewportIndex);

		sl::Resource depthResource           = { sl::ResourceType::eTex2d, tags.depthBuffer_,     static_cast<Uint32>(tags.depthBufferState_) };
		sl::Resource velocityResource        = { sl::ResourceType::eTex2d, tags.velocityBuffer_,  static_cast<Uint32>(tags.velocityBufferState_) };
		sl::Resource hudlessResourceTag      = { sl::ResourceType::eTex2d, hudlessResource,       static_cast<Uint32>(D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE) };
		sl::Resource uiColorAlphaResourceTag = { sl::ResourceType::eTex2d, uiColorAlphaResource,  static_cast<Uint32>(D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE) };

		sl::Extent inputSize{};
		inputSize.top = 0;
		inputSize.left = 0;
		inputSize.width = static_cast<Uint32>(tags.width_);
		inputSize.height = static_cast<Uint32>(tags.height_);

		sl::Extent outputSize{};
		outputSize.top = 0;
		outputSize.left = 0;
		outputSize.width = outputWidth;
		outputSize.height = outputHeight;

		sl::ResourceTag frameGenerationTags[4] =
		{
			{ &depthResource,           sl::kBufferTypeDepth,           sl::ResourceLifecycle::eValidUntilPresent, &inputSize },
			{ &velocityResource,        sl::kBufferTypeMotionVectors,   sl::ResourceLifecycle::eValidUntilPresent, &inputSize },
			{ &hudlessResourceTag,      sl::kBufferTypeHUDLessColor,    sl::ResourceLifecycle::eValidUntilPresent, &outputSize },
			{ &uiColorAlphaResourceTag, sl::kBufferTypeUIColorAndAlpha, sl::ResourceLifecycle::eValidUntilPresent, &outputSize },
		};

		sl::Result sr = slSetTagForFrame(*currentToken_, viewport, frameGenerationTags, static_cast<Uint32>(std::size(frameGenerationTags)), cmdList);
		SC_SL_CHECK(sr, "DLSS-G用リソースタグの設定に失敗しました");
#endif
	}

	void DlssManager::Constants(const SceneConstantBuffer& scene, Uint32 viewportIndex, Bool reset)
	{
#if !SC_RENDER_DOC_USAGE
		if (!currentToken_)
		{
			return;
		}

		sl::ViewportHandle viewport(viewportIndex);

		Float width = scene.screenSize_.x;
		Float height = scene.screenSize_.y;
		sl::float2 jitterOffset((scene.projection_._31 - scene.nonJitterProjection_._31) * width * 0.5f, -(scene.projection_._32 - scene.nonJitterProjection_._32) * height * 0.5f);

		Matrix clipToPrevClip = scene.nonJitterViewProjection_.Invert() * scene.previousNonJitterViewProjection_;
		Matrix prevClipToClip = scene.previousNonJitterViewProjection_.Invert() * scene.nonJitterViewProjection_;

		Vector3 cameraRight = scene.inverseView_.Right();
		Vector3 cameraUp = scene.inverseView_.Up();
		Vector3 cameraForward = scene.inverseView_.Forward();

		sl::Constants constants{};
		constants.cameraViewToClip = ToSlMatrix(scene.nonJitterProjection_);
		constants.clipToCameraView = ToSlMatrix(scene.nonJitterProjection_.Invert());
		constants.clipToLensClip = ToSlMatrix(Matrix::Identity);
		constants.clipToPrevClip = ToSlMatrix(clipToPrevClip);
		constants.prevClipToClip = ToSlMatrix(prevClipToClip);
		constants.jitterOffset = jitterOffset;
		constants.mvecScale = sl::float2(-1.0f, 1.0f);
		constants.cameraPinholeOffset = sl::float2(0.0f, 0.0f);
		constants.cameraPos = sl::float3(scene.cameraPosition_.x, scene.cameraPosition_.y, scene.cameraPosition_.z);
		constants.cameraUp = sl::float3(cameraUp.x, cameraUp.y, cameraUp.z);
		constants.cameraRight = sl::float3(cameraRight.x, cameraRight.y, cameraRight.z);
		constants.cameraFwd = sl::float3(cameraForward.x, cameraForward.y, cameraForward.z);
		constants.cameraNear = scene.nearPlane_;
		constants.cameraFar = scene.farPlane_;
		constants.cameraFOV = scene.fieldOfView_;
		constants.cameraAspectRatio = width / height;

		constants.depthInverted = sl::Boolean::eTrue;
		constants.cameraMotionIncluded = sl::Boolean::eTrue;
		constants.motionVectors3D = sl::Boolean::eFalse;
		constants.reset = reset ? sl::Boolean::eTrue : sl::Boolean::eFalse;
		constants.orthographicProjection = sl::Boolean::eFalse;
		constants.motionVectorsDilated = sl::Boolean::eFalse;
		constants.motionVectorsJittered = sl::Boolean::eTrue;

		sl::Result sr = slSetConstants(constants, *currentToken_, viewport);
		SC_SL_CHECK(sr, "DLSS共通カメラ定数(sl::Constants)の設定に失敗しました");
#endif
	}

	void DlssManager::ReflexEnable(Bool enable)
	{
		reflexEnabled_ = enable;
	}

	Bool DlssManager::ReflexEnable()const
	{
		return reflexEnabled_;
	}

	void DlssManager::DeepDVCEnable(Bool enable)
	{
		deepDVCEnabled_ = enable;
	}

	Bool DlssManager::DeepDVCEnable()const
	{
		return deepDVCEnabled_;
	}

	void DlssManager::FrameGenerationEnable(Bool enable)
	{
		frameGenerationEnabled_ = enable;

#if !SC_RENDER_DOC_USAGE
		sl::Result sr = slSetFeatureLoaded(sl::kFeatureDLSS_G, enable);
		SC_SL_CHECK(sr, "DLSS-Gのロード状態切り替えに失敗しました");
#endif
	}

	Bool DlssManager::FrameGenerationEnable()const
	{
		return frameGenerationEnabled_;
	}

	void DlssManager::FrameGenerationSuppress(Bool suppress)
	{
		frameGenerationSuppressed_ = suppress;
	}

	Bool DlssManager::FrameGenerationVSyncSupported()const
	{
		return frameGenerationVSyncSupported_;
	}

	void DlssManager::RayReconstructionEnable(Bool enable)
	{
		rayReconstructionEnabled_ = enable;
	}

	Bool DlssManager::RayReconstructionEnable()const
	{
		return rayReconstructionEnabled_;
	}

	void DlssManager::Reflex(Bool useBoost)
	{
		reflexUseBoost_ = useBoost;
	}

	void DlssManager::DeepDVC(Float intensity, Float saturationBoost)
	{
		deepDVCIntensity_ = intensity;
		deepDVCSaturationBoost_ = saturationBoost;
	}

	void DlssManager::EvaluateReflex()
	{
#if !SC_RENDER_DOC_USAGE
		if (!currentToken_)
		{
			return;
		}

		sl::ReflexOptions reflexOptions{};
		reflexOptions.mode = reflexEnabled_ ? (reflexUseBoost_ ? sl::ReflexMode::eLowLatencyWithBoost : sl::ReflexMode::eLowLatency) : sl::ReflexMode::eOff;

		sl::Result sr = slReflexSetOptions(reflexOptions);
		SC_SL_CHECK(sr, "Reflexオプションの設定に失敗しました");

		sr = slReflexSleep(*currentToken_);
		SC_SL_CHECK(sr, "Reflexのスリープに失敗しました");
#endif
	}

	void DlssManager::EvaluateDeepDVC(ID3D12CommandList* cmdList, ID3D12Resource* colorBuffer, Uint32 viewportIndex, Uint32 width, Uint32 height)
	{
#if !SC_RENDER_DOC_USAGE
		if (!currentToken_ || !deepDVCEnabled_ || !loadResult_.isDeepDVC_)
		{
			return;
		}

		sl::Result sr{ sl::Result::eOk };
		sl::ViewportHandle viewport(viewportIndex);

		sl::Resource colorResource = { sl::ResourceType::eTex2d, colorBuffer, static_cast<Uint32>(D3D12_RESOURCE_STATE_UNORDERED_ACCESS) };

		sl::Extent extent{};
		extent.top = 0;
		extent.left = 0;
		extent.width = width;
		extent.height = height;

		sl::ResourceTag deepDVCTags[1] =
		{
			{ &colorResource, sl::kBufferTypeScalingOutputColor, sl::ResourceLifecycle::eValidUntilEvaluate, &extent },
		};

		sr = slSetTagForFrame(*currentToken_, viewport, deepDVCTags, static_cast<Uint32>(std::size(deepDVCTags)), cmdList);
		SC_SL_CHECK(sr, "DeepDVC用リソースタグの設定に失敗しました");

		sl::DeepDVCOptions deepDVCOptions{};
		deepDVCOptions.mode = sl::DeepDVCMode::eOn;
		deepDVCOptions.intensity = deepDVCIntensity_;
		deepDVCOptions.saturationBoost = deepDVCSaturationBoost_;

		sr = slDeepDVCSetOptions(viewport, deepDVCOptions);
		SC_SL_CHECK(sr, "DeepDVCオプションの設定に失敗しました");

		const sl::BaseStructure* inputs[] =
		{
			&viewport,
		};

		sr = slEvaluateFeature(sl::kFeatureDeepDVC, *currentToken_, inputs, static_cast<Uint32>(std::size(inputs)), cmdList);
		SC_SL_CHECK(sr, "DeepDVCの実行に失敗しました");
#endif
	}

	void DlssManager::EvaluateFrameGeneration(Uint32 viewportIndex)
	{
#if !SC_RENDER_DOC_USAGE
		if (!currentToken_ || !loadResult_.isDlssFG_)
		{
			return;
		}

		sl::ViewportHandle viewport(viewportIndex);

		sl::DLSSGOptions options{};
		options.mode = (frameGenerationEnabled_ && !frameGenerationSuppressed_) ? sl::DLSSGMode::eOn : sl::DLSSGMode::eOff;

		sl::Result sr = slDLSSGSetOptions(viewport, options);
		SC_SL_CHECK(sr, "DLSS-Gオプションの設定に失敗しました");

		/// [EN] bIsVsyncSupportAvailable is only meaningful once DLSS-G has actually evaluated at least once, so a failure here (e.g. before that point) is expected and left silent - frameGenerationVSyncSupported_ simply keeps its previous value.
		/// [JP] bIsVsyncSupportAvailableはDLSS-Gが一度でも実行された後でないと意味を持たないため、それ以前の失敗は想定内として無視する - frameGenerationVSyncSupported_は直前の値を保持したままになる。
		sl::DLSSGState state{};
		if (slDLSSGGetState(viewport, state, &options) == sl::Result::eOk)
		{
			frameGenerationVSyncSupported_ = (state.bIsVsyncSupportAvailable == sl::Boolean::eTrue);
		}
#endif
	}

	void DlssManager::EvaluateRayReconstruction(ID3D12CommandList* cmdList, const SceneConstantBuffer& scene, Uint32 viewportIndex, Uint32 outputWidth, Uint32 outputHeight, UpscaleMode mode)
	{
#if !SC_RENDER_DOC_USAGE
		if (!currentToken_)
		{
			return;
		}

		sl::Result sr{ sl::Result::eOk };
		sl::ViewportHandle viewport(viewportIndex);

		sl::DLSSDOptions dlssdOptions{};
		dlssdOptions.mode = ToStreamlineDlssMode(mode);
		dlssdOptions.outputWidth = outputWidth;
		dlssdOptions.outputHeight = outputHeight;
		dlssdOptions.sharpness = 0.0f;
		dlssdOptions.preExposure = 1.0f;
		dlssdOptions.colorBuffersHDR = sl::Boolean::eTrue;
		dlssdOptions.indicatorInvertAxisX = sl::Boolean::eFalse;
		dlssdOptions.indicatorInvertAxisY = sl::Boolean::eFalse;
		dlssdOptions.normalRoughnessMode = sl::DLSSDNormalRoughnessMode::ePacked;
		dlssdOptions.alphaUpscalingEnabled = sl::Boolean::eFalse;
		dlssdOptions.worldToCameraView = ToSlMatrix(scene.view_);
		dlssdOptions.cameraViewToWorld = ToSlMatrix(scene.inverseView_);
		sr = slDLSSDSetOptions(viewport, dlssdOptions);
		SC_SL_CHECK(sr, "DLSS-RRオプションの設定に失敗しました");

		const sl::BaseStructure* inputs[] =
		{
			&viewport,
		};

		sr = slEvaluateFeature(sl::kFeatureDLSS_RR, *currentToken_, inputs, static_cast<Uint32>(std::size(inputs)), cmdList);
		SC_SL_CHECK(sr, "DLSS-RRの実行に失敗しました");
#endif
	}

	void DlssManager::EvaluateDlss(ID3D12CommandList* cmdList, Uint32 viewportIndex, Uint32 outputWidth, Uint32 outputHeight, UpscaleMode mode)
	{
#if !SC_RENDER_DOC_USAGE
		if (!currentToken_)
		{
			return;
		}

		sl::Result sr{ sl::Result::eOk };
		sl::ViewportHandle viewport(viewportIndex);

		sl::DLSSOptions dlssOptions{};
		dlssOptions.mode = ToStreamlineDlssMode(mode);
		dlssOptions.outputWidth = outputWidth;
		dlssOptions.outputHeight = outputHeight;
		dlssOptions.sharpness = 0.0f;
		dlssOptions.preExposure = 1.0f;
		dlssOptions.colorBuffersHDR = sl::Boolean::eTrue;
		dlssOptions.indicatorInvertAxisX = sl::Boolean::eFalse;
		dlssOptions.indicatorInvertAxisY = sl::Boolean::eFalse;

		const sl::BaseStructure* inputs[] =
		{
			&viewport,
		};

		sr = slEvaluateFeature(sl::kFeatureDLSS, *currentToken_, inputs, static_cast<Uint32>(std::size(inputs)), cmdList);
		SC_SL_CHECK(sr, "DLSSの実行に失敗しました");
#endif
	}
}
