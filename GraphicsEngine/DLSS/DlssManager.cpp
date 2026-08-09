#include <GraphicsEngine/DLSS/DlssManager.h>
#include <FoundationEngine/Log/SlFail.h>

namespace SeedCore
{
#if !SC_RENDER_DOC_USAGE
	namespace
	{
		/// [JP] DirectX::SimpleMath::Matrix(row-major)を sl::float4x4(同じく
		///      row-major、単純な4行の配列)へそのままコピーする。
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

		sl::Feature features[] =
		{
			sl::kFeatureDLSS,
			sl::kFeaturePCL,
			sl::kFeatureReflex,
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
		preference.projectId = "8e5d9cac-4c56-4513-813c-09a0f1168a83";
		preference.renderAPI = sl::RenderAPI::eD3D12;
		preference.flags = sl::PreferenceFlags::eDisableCLStateTracking | sl::PreferenceFlags::eAllowOTA | sl::PreferenceFlags::eLoadDownloadedPlugins | sl::PreferenceFlags::eUseFrameBasedResourceTagging | sl::PreferenceFlags::eDisableDebugText;

		sr = slInit(preference);
		SC_SL_CHECK(sr, "Streamlineエンジンの初期化に失敗しました。アプリケーションIDやプラグインパスを確認してください。");
	
		sr = slIsFeatureLoaded(sl::kFeatureDLSS, loadResult_.isDlss_);
		SC_SL_CHECK(sr, "DLSS機能のロード状態を確認できませんでした。");

		sr = slIsFeatureLoaded(sl::kFeatureReflex, loadResult_.isReflex_);
		SC_SL_CHECK(sr, "Reflex機能のロード状態を確認できませんでした。");

		sr = slIsFeatureLoaded(sl::kFeatureDLSS_G, loadResult_.isDlssFG_);
		SC_SL_CHECK(sr, "DLSS FG機能のロード状態を確認できませんでした。");

		sr = slIsFeatureLoaded(sl::kFeatureDLSS_RR, loadResult_.isDlssRR_);
		SC_SL_CHECK(sr, "DLSS RR機能のロード状態を確認できませんでした。");
#endif
		return true;
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

	void DlssManager::Tag(DlssBufferTag tags, ID3D12CommandList* cmdList, ID3D12Resource* outPutBuffer, Uint32 viewportIndex, Uint32 outputWidth, Uint32 outputHeight)
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

		/// [JP] 出力側(アップスケール後)は入力側と解像度が違う — scaling-output-color
		///      タグに入力側の size を使い回すと、DLSS-RR が出力バッファを入力
		///      解像度としか認識できず正しくアップスケールされない。
		sl::Extent outputSize{};
		outputSize.top = 0;
		outputSize.left = 0;
		outputSize.width = outputWidth;
		outputSize.height = outputHeight;

		tags_[0] = { &colorResource,           sl::kBufferTypeScalingInputColor,  sl::ResourceLifecycle::eValidUntilEvaluate, &inputSize };
		tags_[1] = { &depthResource,           sl::kBufferTypeDepth,              sl::ResourceLifecycle::eValidUntilEvaluate, &inputSize };
		tags_[2] = { &velocityResource,        sl::kBufferTypeMotionVectors,      sl::ResourceLifecycle::eValidUntilEvaluate, &inputSize };
		tags_[3] = { &albedoResource,          sl::kBufferTypeAlbedo,             sl::ResourceLifecycle::eValidUntilEvaluate, &inputSize };
		tags_[4] = { &normalRoughnessResource, sl::kBufferTypeNormalRoughness,    sl::ResourceLifecycle::eValidUntilEvaluate, &inputSize };
		tags_[5] = { &specularAlbedoResource,  sl::kBufferTypeSpecularAlbedo,     sl::ResourceLifecycle::eValidUntilEvaluate, &inputSize };
		tags_[6] = { &outputResource,          sl::kBufferTypeScalingOutputColor, sl::ResourceLifecycle::eValidUntilEvaluate, &outputSize };

		sr = slSetTagForFrame(*currentToken_, viewport, tags_, static_cast<Uint32>(std::size(tags_)), cmdList);
		SC_SL_CHECK(sr, "フレームへのタグ設定に失敗しました");
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

		/// [JP] EditorCamera::Update() は projection_ = nonJitterProjection_ に
		///      ._31/._32 のNDCオフセットを足してジッタを掛けている
		///      (projection_._31 += jitter.x*2/width、
		///       projection_._32 -= jitter.y*2/height)。Streamline は行列に
		///      ジッタを含めず jitterOffset で別に受け取りたいので、その逆算で
		///      ピクセル単位のジッタ量を復元する。
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

		/// [JP] このエンジンは reverse-Z(GI/AOのraygenコメント参照)。G-Buffer
		///      速度は (current_ndc - previous_ndc) * 0.5 でカメラ移動込みの
		///      スクリーン空間デルタとして書かれる(AmbientOcclusionDenoiseCS.hlsl
		///      等参照)ので mvecScale=1、cameraMotionIncluded=true。
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

	void DlssManager::Finalize()
	{
#if !SC_RENDER_DOC_USAGE
		sl::Result sr{ sl::Result::eOk };

		sr = slShutdown();
		SC_SL_CHECK(sr, "Streamlineのシャットダウン中にエラーが発生しました。");
#endif
	}
}