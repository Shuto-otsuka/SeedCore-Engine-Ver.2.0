#pragma once
#include <FoundationEngine/Prelude.h>
#include <GraphicsEngine/D3D12/SwapChain/GraphicsResolution.h>
#include <GraphicsEngine/DLSS/DlssMode.h>
#include <GraphicsEngine/System/SceneSystem.h>

namespace SeedCore
{
	struct DlssLoadResult
	{
		Bool isDlss_ = false;
		Bool isReflex_ = false;
		Bool isDlssFG_ = false;
		Bool isDlssRR_ = false;
	};

	struct DlssBufferTag
	{
		ID3D12Resource* colorBuffer_ = nullptr;
		ID3D12Resource* depthBuffer_ = nullptr;
		ID3D12Resource* albedoBuffer_ = nullptr;
		ID3D12Resource* velocityBuffer_ = nullptr;
		ID3D12Resource* normalRoughnessBuffer_ = nullptr;

		/// [EN] DLSS-RR requires diffuse and specular albedo tagged
		///      separately (kBufferTypeAlbedo / kBufferTypeSpecularAlbedo).
		///      Both are DlssNormalRoughnessCS.hlsl outputs, not raw G-Buffer
		///      RTs directly: diffuse = base_color*(1-metallic), specular =
		///      EnvBRDFApprox2(lerp(RT2.gba dielectric F0, base_color,
		///      metallic), roughness^2, NdotV) — matching
		///      Model/DeferredLightingPS.hlsl's own f0/diffuse_color split.
		/// [JP] DLSS-RR は拡散/鏡面のアルベドを別々にタグ付けする必要がある
		///      (kBufferTypeAlbedo / kBufferTypeSpecularAlbedo)。どちらも
		///      G-Buffer RT の生値ではなく DlssNormalRoughnessCS.hlsl の出力:
		///      拡散 = base_color*(1-metallic)、鏡面 =
		///      EnvBRDFApprox2(lerp(RT2.gbaの誘電体F0, base_color, metallic),
		///      roughness^2, NdotV) — Model/DeferredLightingPS.hlsl 自身の
		///      f0/diffuse_color の分岐と一致させている。
		ID3D12Resource* specularAlbedoBuffer_ = nullptr;

		/// [EN] Streamline requires an explicit current D3D12 resource state
		///      per tagged resource ("Resource state must be provided" —
		///      sl::Resource's state defaults to UINT_MAX/unspecified, which
		///      setTag rejects outright despite
		///      PreferenceFlags::eUseFrameBasedResourceTagging). Must match
		///      the actual state of each resource at the moment Tag() runs.
		/// [JP] Streamlineはタグ付けする各リソースについて現在のD3D12状態を
		///      明示的に要求する("Resource state must be provided" —
		///      sl::Resourceのstateはデフォルトで UINT_MAX(未指定)だが、
		///      PreferenceFlags::eUseFrameBasedResourceTagging を立てていても
		///      setTagはそれを拒否する)。Tag() 実行時点での各リソースの
		///      実際の状態と一致させること。
		D3D12_RESOURCE_STATES colorBufferState_ = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		D3D12_RESOURCE_STATES depthBufferState_ = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		D3D12_RESOURCE_STATES albedoBufferState_ = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
		D3D12_RESOURCE_STATES velocityBufferState_ = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		D3D12_RESOURCE_STATES normalRoughnessBufferState_ = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
		D3D12_RESOURCE_STATES specularAlbedoBufferState_ = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;

		Float width_ = ScResolution::SC_HD.Width;
		Float height_ = ScResolution::SC_HD.Height;
	};

	class DlssManager
	{
	public:
		DlssManager() = default;
		~DlssManager() = default;

		Bool Initialize();

		Bool Prepare(ID3D12Device* device);

		/// [EN] Mints this Present-frame's Streamline frame token exactly once.
		///      Must be called once per frame BEFORE the first Tag()/Evaluate*()
		///      call of that frame (e.g. at the very top of
		///      Graphics::EditorRender, before both the Editor and Game
		///      viewports are processed) — NOT inside Tag() itself. A frame
		///      token identifies the frame (motion/jitter/history advance), not
		///      a viewport; minting a new one per viewport-call would silently
		///      hand the second viewport's token to the first viewport's
		///      Evaluate call.
		/// [JP] このPresentフレームのStreamlineフレームトークンを1回だけ取得する。
		///      毎フレーム、そのフレームの最初のTag()/Evaluate*()呼び出しより
		///      【前】に1回だけ呼ぶこと(例: Graphics::EditorRenderの先頭、
		///      Editor/Game両ビューポートを処理する前) — Tag()の内部では呼ばない。
		///      フレームトークンはフレームを識別するもの(モーション/ジッタ/
		///      履歴の進行)であり、ビューポートを識別するものではない。
		///      ビューポート呼び出しごとに新規取得すると、2つ目のビューポートの
		///      トークンが黙って1つ目のEvaluate呼び出しに渡ってしまう。
		void BeginFrame();

		/// [EN] viewportIndex disambiguates independent per-viewport DLSS
		///      state/history (0 = Editor, 1 = Game, ...). outputWidth/Height is
		///      the FINAL (post-upscale) resolution — used for the
		///      scaling-output-color resource tag's extent, which must NOT
		///      reuse tags.width_/height_ (the pre-upscale input extent).
		/// [JP] viewportIndex はビューごとに独立したDLSS状態/履歴を区別する
		///      (0=Editor, 1=Game, ...)。outputWidth/Height は最終(アップスケール後)
		///      解像度 — scaling-output-color リソースタグのExtentに使う。
		///      tags.width_/height_(アップスケール前の入力Extent)を使い回しては
		///      いけない。
		void Tag(DlssBufferTag tags, ID3D12CommandList* cmdList, ID3D12Resource* outPutBuffer, Uint32 viewportIndex, Uint32 outputWidth, Uint32 outputHeight);

		/// [EN] Feeds Streamline the per-viewport camera state DLSS/DLSS-RR
		///      needs for reprojection (sl::Constants — required, not
		///      optional: without this call DLSS-RR has no camera matrices and
		///      cannot reconstruct/denoise correctly). Must be called once per
		///      view per frame, BEFORE Tag()/EvaluateRayReconstruction() for
		///      that view. Matrices must be the NON-jittered ones (scene's
		///      nonJitterProjection_/nonJitterViewProjection_) — Streamline
		///      wants jitter reported separately via jitterOffset, derived
		///      here from the difference between scene.projection_ (jittered)
		///      and scene.nonJitterProjection_.
		/// [JP] DLSS/DLSS-RR がリプロジェクションに必要とするビューポートごとの
		///      カメラ状態(sl::Constants)を Streamline へ渡す — 必須呼び出しで
		///      オプションではない(これを呼ばないと DLSS-RR はカメラ行列を
		///      持たず正しく再構成/デノイズできない)。そのビューの
		///      Tag()/EvaluateRayReconstruction() より前に、フレームに1回
		///      呼ぶこと。行列はジッタ無し版(scene の
		///      nonJitterProjection_/nonJitterViewProjection_)を使う —
		///      Streamline はジッタを jitterOffset で別途受け取りたいため、
		///      ここでは scene.projection_(ジッタ有り)と
		///      scene.nonJitterProjection_ の差分から逆算する。
		void Constants(const SceneConstantBuffer& scene, Uint32 viewportIndex, Bool reset);

		/// [EN] scene is required here (not just Constants()) because
		///      DLSSDOptions::worldToCameraView/cameraViewToWorld are
		///      mandatory fields with no default (eErrorMissingInputParameter
		///      if left zeroed) — Streamline's own camera-state struct
		///      (sl::Constants) is separate from DLSSD's per-evaluate options.
		/// [JP] scene がここでも必要な理由(Constants()だけでは足りない):
		///      DLSSDOptions::worldToCameraView/cameraViewToWorld は既定値の
		///      無い必須フィールド(ゼロのままだと
		///      eErrorMissingInputParameter)。Streamline自身のカメラ状態
		///      構造体(sl::Constants)とDLSSDのEvaluateごとのオプションは別物。
		void EvaluateRayReconstruction(ID3D12CommandList* cmdList, const SceneConstantBuffer& scene, Uint32 viewportIndex, Uint32 outputWidth, Uint32 outputHeight, UpscaleMode mode);

		void EvaluateDlss(ID3D12CommandList* cmdList, Uint32 viewportIndex, Uint32 outputWidth, Uint32 outputHeight, UpscaleMode mode);

		void Finalize();

	private:
		DlssLoadResult loadResult_{};

#if !SC_RENDER_DOC_USAGE
		sl::ResourceTag tags_[7]{};
		sl::Constants constants_{};
		sl::FrameToken* currentToken_{ nullptr };
#endif
	};

#if !SC_RENDER_DOC_USAGE
	inline void ScDlssErrorCallback(sl::LogType type, const Char* message)
	{
		Char buffer[512];
		const Char* prefix = (type == sl::LogType::eError) ? "DLSS Error" : "DLSS Info";
		wsprintfA(buffer, "[%s]: %s\n", prefix, message);
		OutputDebugStringA(buffer);
	}
#endif
}