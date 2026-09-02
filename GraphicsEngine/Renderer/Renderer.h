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
#include <GraphicsEngine/System/AnimationSystem.h>
#include <GraphicsEngine/System/ConstraintSystem.h>

#include <GraphicsEngine/D3D12/Buffer/GeometryBuffer.h>
#include <GraphicsEngine/D3D12/Buffer/HiZBuffer.h>
#include <GraphicsEngine/D3D12/Buffer/DepthResizeBuffer.h>
#include <GraphicsEngine/Model/Material/MaterialResolveShader.h>
#include <GraphicsEngine/Model/Material/MaterialSortBuffer.h>
#include <GraphicsEngine/Renderer/ImageRenderer.h>
#include <GraphicsEngine/Renderer/FontRenderer.h>
#include <GraphicsEngine/Renderer/MovieRenderer.h>
#include <GraphicsEngine/Renderer/ModelRenderer.h>
#include <GraphicsEngine/Renderer/OutlineRenderer.h>
#include <GraphicsEngine/Renderer/HUDComposeRenderer.h>
#include <GraphicsEngine/Renderer/ColliderRenderer.h>
#include <GraphicsEngine/Renderer/RaytracingRenderer.h>
#include <GraphicsEngine/Renderer/SkyRenderer.h>
#include <GraphicsEngine/Renderer/TimelineRenderer.h>
#include <GraphicsEngine/Renderer/ModelTransformRenderer.h>
#include <GraphicsEngine/Renderer/MaterialRenderer.h>
#include <GraphicsEngine/Renderer/EffekseerRenderer.h>
#include <GraphicsEngine/Renderer/PostProcessRenderer.h>
#include <GraphicsEngine/D3D12/Buffer/HudlessBuffer.h>
#include <GraphicsEngine/Renderer/DlssRayReconstructionRenderer.h>
#include <GraphicsEngine/Renderer/TaauUpsamplingRenderer.h>
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

		void Resize(ID3D12Device* device, BindlessHeap* bindlessHeap, ShaderCache& shaderCache, Uint32 nativeWidth, Uint32 nativeHeight, Uint32 outputWidth, Uint32 outputHeight);

	public:
		void BeginEditorFrame(D3D12CommandList* cmdList);

		void EndEditorFrame(D3D12CommandList* cmdList, const SceneConstantBuffer& scene);

		void BeginGameFrame(D3D12CommandList* cmdList);

		void EndGameFrame(D3D12CommandList* cmdList, const SceneConstantBuffer& scene);

		void BeginCanvasFrame(D3D12CommandList* cmdList);

		void EndCanvasFrame(D3D12CommandList* cmdList);

		void BeginTimelineFrame(D3D12CommandList* cmdList);

		void EndTimelineFrame(D3D12CommandList* cmdList);

		void BeginModelTransformFrame(D3D12CommandList* cmdList);

		void EndModelTransformFrame(D3D12CommandList* cmdList);

		void BeginMaterialFrame(D3D12CommandList* cmdList);

		void EndMaterialFrame(D3D12CommandList* cmdList);

	public:
		void GatherScenePreview(LoaderSystem& loaderSystem, ResourceCache& resourceCache, World& world, const SceneConstantBuffer& scene, const DynamicArray<ColliderInstance>& colliderInstances, Entity selectedEntity = Entity::Null());

		void GatherTimelinePreview(LoaderSystem& loaderSystem, ResourceCache& resourceCache, Uint32 meshAssetId, Uint32 animationAssetId, Float time, const Matrix& worldMatrix);

		void GatherModelTransformPreview(LoaderSystem& loaderSystem, ResourceCache& resourceCache, Uint32 meshAssetId, Uint32 animationAssetId, Float time, const Matrix& worldMatrix);

		void GatherMaterialPreview(LoaderSystem& loaderSystem, ResourceCache& resourceCache, Uint32 meshAssetId, Uint32 surfaceAssetId, const Matrix& worldMatrix);

	public:
		void Raytracing(const RaytracingContext& settings);

		[[nodiscard]] Vector2 PostProcessOutputSize()const;

	public:
		void EditorFlush(D3D12CommandList* cmdList, SceneSystem* sceneSystem, Float deltaTime, ViewMode viewMode);

		void GameFlush(D3D12CommandList* cmdList, SceneSystem* sceneSystem, Float deltaTime, Bool hasActiveCamera);

		void CanvasFlush(D3D12CommandList* cmdList, SceneSystem* sceneSystem);

		void TimelineFlush(D3D12CommandList* cmdList, const SceneConstantBuffer& scene);

		void ModelTransformFlush(D3D12CommandList* cmdList, const SceneConstantBuffer& scene);

		void MaterialFlush(D3D12CommandList* cmdList, const SceneConstantBuffer& scene);

	public:
		[[nodiscard]] const GpuProfiler& GetGpuProfiler()const;

		[[nodiscard]] FrameBuffer* GetEditorFrameBuffer()const;

		[[nodiscard]] FrameBuffer* GetGameFrameBuffer()const;

		[[nodiscard]] FrameBuffer* GetCanvasFrameBuffer()const;

		[[nodiscard]] D3D12_GPU_DESCRIPTOR_HANDLE EditorFrameBufferGPUHandle()const;

		[[nodiscard]] D3D12_GPU_DESCRIPTOR_HANDLE GameFrameBufferGPUHandle()const;

		[[nodiscard]] D3D12_GPU_DESCRIPTOR_HANDLE CanvasFrameBufferGPUHandle()const;

	public:
		void RegisterImGuiShaderResourceViews(ID3D12Device* device, DescriptorHeap* imguiHeap);

		[[nodiscard]] D3D12_GPU_DESCRIPTOR_HANDLE EditorImGuiGPUHandle()const;

		[[nodiscard]] D3D12_GPU_DESCRIPTOR_HANDLE GameImGuiGPUHandle()const;

		[[nodiscard]] D3D12_GPU_DESCRIPTOR_HANDLE CanvasImGuiGPUHandle()const;

		[[nodiscard]] D3D12_GPU_DESCRIPTOR_HANDLE TimelineImGuiGPUHandle()const;

		[[nodiscard]] D3D12_GPU_DESCRIPTOR_HANDLE ModelTransformImGuiGPUHandle()const;

		[[nodiscard]] D3D12_GPU_DESCRIPTOR_HANDLE MaterialImGuiGPUHandle()const;

	private:
		void RefreshImGui(RaytracingView view);

	private:
		RootSignature rootSignature_;
		PipelineStateObject pipelineStateObject_;
		RaytracingStateObject raytracingStateObject_;

		ResourcePtr<EffekseerManager> effekseerManager_;

		ResourcePtr<IndicesSystem> indicesSystem_;

		ResourcePtr<LightSystem> lightSystem_;

		AnimationSystem animationSystem_;

		ConstraintSystem constraintSystem_;

	private:
		ResourcePtr<ImageRenderer> imageRenderer_;

		ResourcePtr<FontRenderer> fontRenderer_;

		ResourcePtr<MovieRenderer> movieRenderer_;

		ResourcePtr<ModelRenderer> modelRenderer_;

		ResourcePtr<OutlineRenderer> outlineRenderer_;

		ResourcePtr<HUDComposeRenderer> hudComposeRenderer_;

		ResourcePtr<ColliderRenderer> colliderRenderer_;

		ResourcePtr<RaytracingRenderer> raytracingRenderer_;

		ResourcePtr<SkyRenderer> skyRenderer_;

		ResourcePtr<TimelineRenderer> timelineRenderer_;

		ResourcePtr<ModelTransformRenderer> modelTransformRenderer_;
		ResourcePtr<MaterialRenderer> materialRenderer_;

		ResourcePtr<EffekseerRenderer> effekseerRenderer_;

		ResourcePtr<PostProcessRenderer> postProcessRenderer_;

		ResourcePtr<DlssRayReconstructionRenderer> dlssRayReconstructionRenderer_;

		ResourcePtr<TaauUpsamplingRenderer> taauUpsamplingRenderer_;

		/// [EN] Cached from the last Raytracing() call. Passed into
		///      DlssRayReconstructionRenderer::Dispatch/TaauUpsamplingRenderer's
		///      render-scale calculation in EndEditorFrame/EndGameFrame.
		/// [JP] 直近の Raytracing() 呼び出しからキャッシュ。
		///      EndEditorFrame/EndGameFrame で DlssRayReconstructionRenderer::
		///      Dispatch / TaauUpsamplingRenderer のレンダースケール計算に渡す。
		UpscaleMode upscaleMode_ = UpscaleMode::Balanced;

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

		/// [EN] Cached from the last Raytracing() call. daySystem_ is
		///      already advanced upstream (Editor::Engine, before RaytracingContext
		///      is pushed down) - Gather() only computes this frame's sun/moon
		///      state from it via CelestialSystem::Compute (pure, no deltaTime).
		/// [JP] 直近の Raytracing() 呼び出しからキャッシュ。daySystem_
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

		/// [EN] VisibilityBuffer material resolve compute pass (Model/Material/MaterialResolveCS.hlsl)
		///      - rewrites geometryBuffer_'s RT0/1/2/3 from RT4(visibility id)+depth.
		///      Also owns the material sort PSOs (Classify/PrefixSum/Scatter)
		///      that run before it - see materialSortBuffer_.
		/// [JP] VisibilityBuffer マテリアル解決コンピュートパス(Model/Material/MaterialResolveCS.hlsl)
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

		DescriptorHeap uiColorAlphaRenderTargetViewHeap_;

		ResourcePtr<FrameBuffer> editorFrameBuffer_;

		ResourcePtr<FrameBuffer> gameFrameBuffer_;

		ResourcePtr<FrameBuffer> canvasFrameBuffer_;

		ResourcePtr<FrameBuffer> uiColorAlphaFrameBuffer_;

		HudlessBuffer hudlessBuffer_;

		BindlessHeap* bindlessHeap_ = nullptr;

		ID3D12Device* device_ = nullptr;

		DescriptorHeap* imguiHeap_ = nullptr;

		Uint32 editorImGuiShaderResourceViewIndex_ = 0;

		Uint32 gameImGuiShaderResourceViewIndex_ = 0;

		Uint32 canvasImGuiShaderResourceViewIndex_ = 0;
	};
}
