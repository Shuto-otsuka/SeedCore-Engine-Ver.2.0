#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/ECS/Entity.h>

#include <GraphicsEngine/D3D12/Descriptor/DescriptorHeap.h>
#include <GraphicsEngine/D3D12/Buffer/FrameBuffer.h>
#include <GraphicsEngine/D3D12/PipelineState/RootSignature.h>
#include <GraphicsEngine/D3D12/PipelineState/PipelineStateObject.h>
#include <GraphicsEngine/D3D12/PipelineState/RaytracingStateObject.h>
#include <GraphicsEngine/System/IndicesSystem.h>
#include <GraphicsEngine/System/LightSystem.h>
#include <GraphicsEngine/System/CelestialSystem.h>
#include <GraphicsEngine/System/WeatherSystem.h>
#include <GraphicsEngine/System/ConstraintSystem.h>

#include <GraphicsEngine/D3D12/Buffer/GeometryBuffer.h>
#include <GraphicsEngine/D3D12/Buffer/HiZBuffer.h>
#include <GraphicsEngine/D3D12/Buffer/DepthResizeBuffer.h>
#include <GraphicsEngine/Model/MaterialResolveShader.h>
#include <GraphicsEngine/Model/MaterialSortBuffer.h>
#include <GraphicsEngine/Renderer/ImageRenderer.h>
#include <GraphicsEngine/Renderer/FontRenderer.h>
#include <GraphicsEngine/Renderer/MovieRenderer.h>
#include <GraphicsEngine/Renderer/ModelRenderer.h>
#include <GraphicsEngine/Renderer/OutlineRenderer.h>
#include <GraphicsEngine/Renderer/ColliderRenderer.h>
#include <GraphicsEngine/Renderer/RaytracingRenderer.h>
#include <GraphicsEngine/Renderer/SkyRenderer.h>
#include <GraphicsEngine/Renderer/PreviewRenderer.h>
#include <GraphicsEngine/Renderer/EffekseerRenderer.h>
#include <GraphicsEngine/Renderer/PostProcessRenderer.h>
#include <GraphicsEngine/Renderer/DlssRayReconstructionRenderer.h>
#include <GraphicsEngine/Renderer/ViewMode.h>
#include <GraphicsEngine/DLSS/DlssManager.h>
#include <GraphicsEngine/Effect/Effekseer/EffekseerManager.h>
#include <GraphicsEngine/Profiler/GpuProfiler.h>

namespace SeedCore
{
	struct LoaderSystem;
	class ResourceCache;
	class BindlessHeap;
	class D3D12CommandList;
	class World;
	class ShaderCache;
	class SceneSystem;

	class Renderer :public NonCopyable
	{
	public:
		Renderer();
		~Renderer() = default;

		void Create(ID3D12Device* device, ID3D12CommandQueue* commandQueue, Uint32 swapBufferCount, BindlessHeap* bindlessHeap, ShaderCache& shaderCache, Uint32 width, Uint32 height);

		/// [EN] Resizes every resolution-dependent sub-object in the same order
		///      Create() constructs them, given the caller has already drained
		///      the GPU (see Graphics::Resize). nativeWidth/nativeHeight is the
		///      G-Buffer/render resolution; outputWidth/outputHeight is the
		///      DLSS-RR/PostProcess upscale target (equal to native when DLSS-RR
		///      is off).
		/// [JP] Create() が構築するのと同じ順序で、解像度に依存する全サブ
		///      オブジェクトをリサイズする。呼び出し側が既にGPUをドレイン済み
		///      であること(Graphics::Resize 参照)。nativeWidth/nativeHeight は
		///      G-Buffer/レンダー解像度、outputWidth/outputHeight は
		///      DLSS-RR/PostProcess のアップスケール先(DLSS-RR オフ時はネイティブ
		///      と同じ)。
		void Resize(ID3D12Device* device, BindlessHeap* bindlessHeap, ShaderCache& shaderCache, Uint32 nativeWidth, Uint32 nativeHeight, Uint32 outputWidth, Uint32 outputHeight);

		void BeginEditorFrame(D3D12CommandList* cmdList);

		/// [EN] scene is that view's camera SceneConstantBuffer (view/
		///      projection/jitter/etc.) - needed here (not just Gather()'s
		///      LOD-reference camera) because DLSS Ray Reconstruction, when
		///      active, needs the actual per-view camera state for
		///      reprojection (see DlssManager::Constants).
		/// [JP] scene はそのビューのカメラの SceneConstantBuffer(view/
		///      projection/ジッタ等)。Gather() の LOD 参照用カメラとは別に
		///      ここで必要な理由: DLSS Ray Reconstruction が有効な間、
		///      リプロジェクションに実際のビューごとのカメラ状態が要る
		///      (DlssManager::Constants 参照)。
		void EndEditorFrame(D3D12CommandList* cmdList, const SceneConstantBuffer& scene);

		void BeginGameFrame(D3D12CommandList* cmdList);

		void EndGameFrame(D3D12CommandList* cmdList, const SceneConstantBuffer& scene);

		void BeginCanvasFrame(D3D12CommandList* cmdList);

		void EndCanvasFrame(D3D12CommandList* cmdList);

		void BeginPreviewFrame(D3D12CommandList* cmdList);

		void EndPreviewFrame(D3D12CommandList* cmdList);

		/// [EN] colliderInstances is gathered by the caller (Editor: it alone
		///      links both PhysicsEngine and GraphicsEngine, so it is the only
		///      layer allowed to read Collider component fields) and simply
		///      forwarded into ColliderRenderer — Renderer/GraphicsEngine never
		///      references PhysicsEngine types directly.
		/// [JP] colliderInstances は呼び出し側（Editor: PhysicsEngine と
		///      GraphicsEngine の両方にリンクする唯一の層であり、Collider
		///      コンポーネントのフィールドを読める唯一の層）が集めたものを
		///      そのまま ColliderRenderer へ渡すだけ — Renderer/GraphicsEngine
		///      は PhysicsEngine の型を一切参照しない。
		void Gather(LoaderSystem& loaderSystem, ResourceCache& resourceCache, World& world, const SceneConstantBuffer& scene, const DynamicArray<ColliderInstance>& colliderInstances, Entity selectedEntity = Entity::Null());

		/// [EN] Isolated single-model preview gather (Timeline panel), fully
		///      independent of ModelRenderer/World — see PreviewRenderer.
		/// [JP] 単体モデルプレビュー用の独立した gather(Timelineパネル)。
		///      ModelRenderer/World と完全に独立 — PreviewRenderer 参照。
		void GatherPreview(LoaderSystem& loaderSystem, ResourceCache& resourceCache, Uint32 meshAssetId, Uint32 animationAssetId, Float time, const Matrix& worldMatrix);

		/// [EN] Stored on Renderer (not threaded through Flush's parameters) so
		///      adding future tunables — for this or other raytraced effects —
		///      never grows EditorFlush/GameFlush's signatures: just add a field
		///      to the settings struct and a new SetXSettings() call site.
		/// [JP] Flush の引数で通さず Renderer に保持する。これにより今後
		///      チューニング項目が増えても（このシャドウ機能でも他のレイトレ
		///      機能でも）EditorFlush/GameFlush のシグネチャは変わらない —
		///      設定構造体にフィールドを足し、新しい SetXSettings() の呼び出し
		///      を1つ足すだけで済む。
		void SetRaytracingSettings(const RaytracingContext& settings);

		/// [EN] Must be called once, after Graphics::Initialize() constructs
		///      its DlssManager and before the first EndEditorFrame/
		///      EndGameFrame - Renderer does not own DlssManager's Streamline
		///      SDK lifecycle (Graphics does, alongside the swap chain), it
		///      only needs a pointer to drive Tag()/EvaluateRayReconstruction()
		///      when DLSS-RR is enabled.
		/// [JP] Graphics::Initialize() が DlssManager を構築した後、最初の
		///      EndEditorFrame/EndGameFrame より前に1度だけ呼ぶこと。
		///      Renderer は DlssManager の Streamline SDK ライフサイクルを
		///      所有しない(Graphics がスワップチェインと一緒に所有する) —
		///      DLSS-RR 有効時に Tag()/EvaluateRayReconstruction() を駆動する
		///      ためのポインタが要るだけ。
		void SetDlssManager(DlssManager* dlssManager);

		/// [EN] Cached from the last SetRaytracingSettings() call. Lets a caller that runs before this frame's EditorFlush (e.g. Graphics::EditorRender building the scene constant buffer) know whether the post-tonemap debug overlay will end up drawing at PostProcessOutputSize() (true) or the native resolution (false) later this same frame - see EndEditorFrame.
		/// [JP] 直近の SetRaytracingSettings() 呼び出しからキャッシュ。この関数を、今フレームのEditorFlushより前に実行される呼び出し側(例えばシーン定数バッファを組み立てるGraphics::EditorRender)が、トーンマップ後デバッグオーバーレイが今フレーム後でPostProcessOutputSize()(true)とネイティブ解像度(false)のどちらで描画することになるかを知るために使う - EndEditorFrame参照。
		[[nodiscard]] Bool IsDlssRayReconstructionEnabled()const { return dlssRayReconstructionEnabled_; }

		/// [EN] PostProcessRenderer's DLSS-RR-upscaled output resolution - see IsDlssRayReconstructionEnabled().
		/// [JP] PostProcessRendererのDLSS-RRアップスケール後出力解像度 - IsDlssRayReconstructionEnabled()参照。
		[[nodiscard]] Vector2 PostProcessOutputSize()const;

		void EditorFlush(D3D12CommandList* cmdList, SceneSystem* sceneSystem, Float deltaTime, ViewMode viewMode);

		void GameFlush(D3D12CommandList* cmdList, SceneSystem* sceneSystem, Float deltaTime);

		void CanvasFlush(D3D12CommandList* cmdList, SceneSystem* sceneSystem);

		/// [EN] Draws the preview instances using PreviewRenderer's own
		///      ConstantIndices/StructuredIndices buffers — never touches
		///      IndicesSystem, so it cannot corrupt what Editor/Game/Canvas's
		///      already-recorded draw calls read from their shared, per-frame
		///      -ring constant buffers when the GPU actually executes them.
		/// [JP] PreviewRenderer 自身の ConstantIndices/StructuredIndices
		///      バッファで描画する — IndicesSystem には一切触れないため、
		///      Editor/Game/Canvas が既に記録済みの描画コマンドが GPU 実行時に
		///      読む共有・フレームリング定数バッファを壊すことがない。
		void PreviewFlush(D3D12CommandList* cmdList, const SceneConstantBuffer& scene);

		/// [EN] Per-pass GPU times for the profiler panel. Values lag the current
		///      frame by a couple of frames (see GpuProfiler).
		/// [JP] プロファイラパネル向けのパス別 GPU 時間。値は現在のフレームより
		///      数フレーム遅れる(GpuProfiler 参照)。
		[[nodiscard]] const GpuProfiler& GetGpuProfiler()const;

		[[nodiscard]] FrameBuffer* GetEditorFrameBuffer()const;

		[[nodiscard]] FrameBuffer* GetGameFrameBuffer()const;

		[[nodiscard]] FrameBuffer* GetCanvasFrameBuffer()const;

		[[nodiscard]] D3D12_GPU_DESCRIPTOR_HANDLE EditorFrameBufferGPUHandle()const;

		[[nodiscard]] D3D12_GPU_DESCRIPTOR_HANDLE GameFrameBufferGPUHandle()const;

		[[nodiscard]] D3D12_GPU_DESCRIPTOR_HANDLE CanvasFrameBufferGPUHandle()const;

		void RegisterImGuiShaderResourceViews(ID3D12Device* device, DescriptorHeap* imguiHeap);

		[[nodiscard]] D3D12_GPU_DESCRIPTOR_HANDLE EditorImGuiGPUHandle()const;

		[[nodiscard]] D3D12_GPU_DESCRIPTOR_HANDLE GameImGuiGPUHandle()const;

		[[nodiscard]] D3D12_GPU_DESCRIPTOR_HANDLE CanvasImGuiGPUHandle()const;

		[[nodiscard]] D3D12_GPU_DESCRIPTOR_HANDLE PreviewImGuiGPUHandle()const;

		[[nodiscard]] EffekseerManager* GetEffekseerManager()const;

	private:
		/// [EN] Re-points the Editor/Game ImGui SRV at whichever
		///      PostProcessRenderer output resource is active THIS frame (SD
		///      or DLSS-RR's UHD chain — see PostProcessRenderer::OutputResource).
		///      RegisterImGuiShaderResourceViews only creates this descriptor
		///      ONCE at Editor startup; without a per-frame refresh, toggling
		///      DLSS-RR on switches which resource PostProcessRenderer writes
		///      into but the already-created ImGui descriptor keeps pointing
		///      at the old (now permanently stale) one, so the viewport freezes
		///      on whatever was last drawn there. Called at the end of
		///      EndEditorFrame/EndGameFrame, after PostProcessRenderer::
		///      Dispatch has decided this frame's chain. No-op before ImGui is
		///      initialized (imguiHeap_ still null).
		/// [JP] Editor/Game の ImGui SRV を、そのフレームで実際にアクティブな
		///      PostProcessRenderer 出力リソース(SD、またはDLSS-RRのUHD
		///      チェーン — PostProcessRenderer::OutputResource 参照)へ
		///      再設定する。RegisterImGuiShaderResourceViews はこの
		///      ディスクリプタを Editor 起動時に一度だけ作る — 毎フレーム
		///      更新しないと、DLSS-RR をオンにした瞬間 PostProcessRenderer の
		///      書き込み先は切り替わるのに、既に作成済みの ImGui
		///      ディスクリプタは古い(以後二度と更新されない)リソースを
		///      指したままになり、ビューポートが最後に描画された絵で固まって
		///      見える。EndEditorFrame/EndGameFrame の末尾、
		///      PostProcessRenderer::Dispatch がそのフレームのチェーンを
		///      決めた後に呼ぶ。ImGui 初期化前(imguiHeap_ がまだ null)は
		///      何もしない。
		void RefreshImGuiOutputView(RaytracingView view);

		RootSignature rootSignature_;
		PipelineStateObject pipelineStateObject_;
		RaytracingStateObject raytracingStateObject_;

		ResourcePtr<IndicesSystem> indicesSystem_;

		ResourcePtr<LightSystem> lightSystem_;

		ConstraintSystem constraintSystem_;

		ResourcePtr<ImageRenderer> imageRenderer_;

		ResourcePtr<FontRenderer> fontRenderer_;

		ResourcePtr<MovieRenderer> movieRenderer_;

		ResourcePtr<ModelRenderer> modelRenderer_;

		ResourcePtr<OutlineRenderer> outlineRenderer_;

		ResourcePtr<ColliderRenderer> colliderRenderer_;

		ResourcePtr<RaytracingRenderer> raytracingRenderer_;

		ResourcePtr<SkyRenderer> skyRenderer_;

		ResourcePtr<PreviewRenderer> previewRenderer_;

		ResourcePtr<EffekseerRenderer> effekseerRenderer_;

		ResourcePtr<EffekseerManager> effekseerManager_;

		ResourcePtr<PostProcessRenderer> postProcessRenderer_;

		ResourcePtr<DlssRayReconstructionRenderer> dlssRayReconstructionRenderer_;

		DlssManager* dlssManager_ = nullptr;

		/// [EN] Cached from the last SetRaytracingSettings() call. Read in
		///      EndEditorFrame/EndGameFrame to decide whether to run
		///      DlssRayReconstructionRenderer before PostProcessRenderer, and
		///      in EditorFlush/GameFlush to decide which output chain
		///      PostProcessRenderer::PrepareView should target.
		/// [JP] 直近の SetRaytracingSettings() 呼び出しからキャッシュ。
		///      EndEditorFrame/EndGameFrame で DlssRayReconstructionRenderer を
		///      PostProcessRenderer より前に走らせるか判断するのに使い、
		///      EditorFlush/GameFlush では PostProcessRenderer::PrepareView が
		///      どちらの出力チェーンを対象にするか判断するのに使う。
		Bool dlssRayReconstructionEnabled_ = false;

		/// [EN] Cached from the last SetRaytracingSettings() call. Passed into
		///      DlssRayReconstructionRenderer::Dispatch in EndEditorFrame/
		///      EndGameFrame.
		/// [JP] 直近の SetRaytracingSettings() 呼び出しからキャッシュ。
		///      EndEditorFrame/EndGameFrame で DlssRayReconstructionRenderer::
		///      Dispatch に渡す。
		DlssMode dlssMode_ = DlssMode::Balanced;

		/// [EN] Native (G-Buffer/render) resolution — set by Create()/Resize(),
		///      passed as DlssRayReconstructionRenderer::Dispatch's
		///      sourceWidth/sourceHeight in EndEditorFrame/EndGameFrame.
		/// [JP] ネイティブ(G-Buffer/レンダー)解像度 — Create()/Resize() が設定し、
		///      EndEditorFrame/EndGameFrame で
		///      DlssRayReconstructionRenderer::Dispatch の
		///      sourceWidth/sourceHeight として渡す。
		Uint32 nativeWidth_ = 0;
		Uint32 nativeHeight_ = 0;

		/// [JP] プロシージャル空の雲の風スクロール用に蓄積する経過時間。
		Float skyTotalTime_ = 0.0f;

		/// [EN] Cached from the last SetRaytracingSettings() call. daySystem_ is
		///      already advanced upstream (Editor::Engine, before RaytracingContext
		///      is pushed down) - Gather() only computes this frame's sun/moon
		///      state from it via CelestialSystem::Compute (pure, no deltaTime).
		/// [JP] 直近の SetRaytracingSettings() 呼び出しからキャッシュ。daySystem_
		///      は上流(Editor::Engine、RaytracingContext を渡す前)で既に
		///      進めてある - Gather() はそこから CelestialSystem::Compute
		///      (純関数、deltaTime不要)でこのフレームの太陽/月状態を計算するだけ。
		Bool daySystemEnabled_ = false;
		DaySystemConstantBuffer daySystem_;

		Bool sunLightEnabled_ = false;
		SunLightSettings sunLight_;

		Bool moonLightEnabled_ = false;
		MoonLightSettings moonLight_;

		/// [EN] Computed once per Gather() from daySystem_/sunLight_/moonLight_.
		///      Fed into LightSystem::Gather (sun/moon override) and into
		///      RaytracingRenderer::Build (nightFactor_, for VolumetricStar's
		///      shooting star spawn chance).
		/// [JP] Gather() 毎に daySystem_/sunLight_/moonLight_ から計算する。
		///      LightSystem::Gather(太陽/月の上書き)と RaytracingRenderer::Build
		///      (nightFactor_、VolumetricStar の流れ星スポーン確率用)へ渡す。
		CelestialResult celestialResult_;

		/// [EN] Computed once per Gather() via WeatherSystem::ReadGpuState.
		///      Fed into LightSystem::Gather (wetness_/snowCoverage_/
		///      thunderFlash_) and RaytracingRenderer::Build (snowIntensity_,
		///      for the snow particle system's density).
		/// [JP] Gather() 毎に WeatherSystem::ReadGpuState で計算する。
		///      LightSystem::Gather(wetness_/snowCoverage_/thunderFlash_)と
		///      RaytracingRenderer::Build(snowIntensity_、雪パーティクル系の
		///      密度用)へ渡す。
		WeatherGpuState weatherState_;

		/// [EN] Cached from the last Gather() call's scene parameter, for
		///      RaytracingRenderer::Build's weather particle recycling volume
		///      (which follows the camera).
		/// [JP] 直近の Gather() 呼び出しの scene 引数からキャッシュ。
		///      RaytracingRenderer::Build の天候パーティクル再スポーン
		///      ボリューム(カメラに追従)用。
		Vector3 lastCameraPosition_ = { 0.0f, 0.0f, 0.0f };

		GeometryBuffer geometryBuffer_;
		HiZBuffer hiZBuffer_;

		/// [EN] VisibilityBuffer material resolve compute pass (Model/MaterialResolveCS.hlsl)
		///      - rewrites geometryBuffer_'s RT0/1/2/3 from RT4(visibility id)+depth.
		///      Also owns the material sort PSOs (Classify/PrefixSum/Scatter)
		///      that run before it - see materialSortBuffer_.
		/// [JP] VisibilityBuffer マテリアル解決コンピュートパス(Model/MaterialResolveCS.hlsl)
		///      - geometryBuffer_ の RT0/1/2/3 を RT4(visibility id)+depth から書き直す。
		///      その前段のマテリアルソートPSO(Classify/PrefixSum/Scatter)も持つ -
		///      materialSortBuffer_ 参照。
		ResourcePtr<MaterialResolveShader> materialResolveShader_;

		/// [EN] GPU resources for the material sort - see MaterialResolveShader.
		/// [JP] マテリアルソート用のGPUリソース - MaterialResolveShader 参照。
		MaterialSortBuffer materialSortBuffer_;

		/// [EN] geometryBuffer_'s depth resized to PostProcessRenderer's DLSS-RR-upscaled output resolution - see Renderer::EndEditorFrame's debug overlay.
		/// [JP] geometryBuffer_の深度をPostProcessRendererのDLSS-RRアップスケール後出力解像度へリサイズしたもの - Renderer::EndEditorFrameのデバッグオーバーレイ参照。
		DepthResizeBuffer debugDepthResizeBuffer_;

		/// [EN] Per-pass GPU timing. Owned here because Renderer is where every
		///      timed pass is issued, so no plumbing has to reach further down.
		///      Advance() runs once per frame from BeginEditorFrame.
		/// [JP] パス別 GPU 計測。計測対象のパスは全て Renderer から発行されるので
		///      ここが所有者で、下位へバケツリレーする必要がない。Advance() は
		///      BeginEditorFrame から毎フレーム1回呼ぶ。
		GpuProfiler gpuProfiler_;

		/// [EN] Selection outline mask: a single shared single-channel (R8_UNORM)
		///      target that Model/Sprite/Billboard/Font renderers each draw their
		///      selected instances into before one shared edge-detect composite.
		/// [JP] 選択アウトラインマスク: Model/Sprite/Billboard/Font の各 Renderer が
		///      選択中インスタンスを描き込む、共有の単チャンネル(R8_UNORM)ターゲット
		///      1 枚。最後に 1 回だけ共有のエッジ検出合成を行う。
		DescriptorHeap selectionMaskRenderTargetViewHeap_;
		ResourcePtr<FrameBuffer> selectionMaskFrameBuffer_;

		DescriptorHeap editorRenderTargetViewHeap_;

		DescriptorHeap editorDepthStencilViewHeap_;

		DescriptorHeap gameRenderTargetViewHeap_;

		DescriptorHeap gameDepthStencilViewHeap_;

		DescriptorHeap canvasRenderTargetViewHeap_;

		DescriptorHeap canvasDepthStencilViewHeap_;

		ResourcePtr<FrameBuffer> editorFrameBuffer_;

		ResourcePtr<FrameBuffer> gameFrameBuffer_;

		ResourcePtr<FrameBuffer> canvasFrameBuffer_;

		BindlessHeap* bindlessHeap_ = nullptr;

		ID3D12Device* device_ = nullptr;

		DescriptorHeap* imguiHeap_ = nullptr;

		Uint32 editorImGuiShaderResourceViewIndex_ = 0;

		Uint32 gameImGuiShaderResourceViewIndex_ = 0;

		Uint32 canvasImGuiShaderResourceViewIndex_ = 0;
	};
}
