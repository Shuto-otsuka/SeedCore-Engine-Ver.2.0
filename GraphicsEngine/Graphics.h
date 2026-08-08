#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/ECS/Entity.h>
#include <GraphicsEngine/D3D12/Context/D3D12Context.h>
#include <GraphicsEngine/D3D12/SwapChain/SwapChain.h>
#include <GraphicsEngine/D3D12/SwapChain/GraphicsResolution.h>
#include <GraphicsEngine/D3D12/Descriptor/BindlessHeap.h>
#include <GraphicsEngine/Shader/ShaderCache.h>
#include <GraphicsEngine/Model/BC7CompressShader.h>
#include <GraphicsEngine/DLSS/DlssManager.h>
#include <GraphicsEngine/System/SceneSystem.h>
#include <GraphicsEngine/System/CameraSystem.h>
#include <GraphicsEngine/System/MovieSystem.h>
#include <GraphicsEngine/Renderer/Renderer.h>
#include <GraphicsEngine/System/SplashScreen.h>
#include <GraphicsEngine/System/FadeScreen.h>

namespace SeedCore
{
	class WorldTimer;
	class EditorCamera;
	class CanvasCamera;
	class PreviewCamera;
	class World;
	struct LoaderSystem;
	class ResourceCache;

	class GameTimer;

	class SEEDCORE_API Graphics
	{
	public:
		Graphics() = default;
		~Graphics() = default;

		Bool Initialize(HWND hwnd, Float width, Float height);

		void Finalize();

		/// [EN] Drains the GPU (all three queues) then resizes every
		///      resolution-dependent render resource. nativeWidth/nativeHeight
		///      is the G-Buffer/render resolution; outputWidth/outputHeight is
		///      the DLSS-RR/PostProcess upscale target (pass the same value as
		///      native when DLSS-RR is off). imguiHeap, if non-null, gets its
		///      Editor/Game/Canvas SRVs re-registered afterward since every
		///      FrameBuffer's bindless index changes on recreation.
		/// [JP] GPUをドレイン(全3キュー)した後、解像度に依存する全レンダー
		///      リソースをリサイズする。nativeWidth/nativeHeight は
		///      G-Buffer/レンダー解像度、outputWidth/outputHeight は
		///      DLSS-RR/PostProcess のアップスケール先(DLSS-RRオフ時はネイティブ
		///      と同じ値を渡す)。imguiHeap が非nullなら、Editor/Game/Canvasの
		///      SRVを再登録する(再生成でbindlessインデックスが変わるため)。
		void Resize(Uint32 nativeWidth, Uint32 nativeHeight, Uint32 outputWidth, Uint32 outputHeight, DescriptorHeap* imguiHeap);

		void EditorRender(WorldTimer& timer, const EditorCamera& editorCamera, LoaderSystem& loaderSystem, ResourceCache& resourceCache, World& world, ViewMode viewMode, const DynamicArray<ColliderInstance>& colliderInstances, Entity selectedEntity = Entity::Null());

		/// [EN] Passthrough to Renderer::SetRaytracingSettings — stored on
		///      Renderer, not threaded through EditorRender's parameters, so
		///      adding tunables for this or future raytraced effects (AO, GI,
		///      reflection...) never grows EditorRender's signature: just add
		///      a field to RaytracingContext.
		/// [JP] Renderer::SetRaytracingSettings へのパススルー。EditorRender の
		///      引数で通さず Renderer に保持させるので、このシャドウ機能でも
		///      将来の他のレイトレ機能(AO/GI/反射...)でもチューニング項目が
		///      増えても EditorRender のシグネチャは変わらない —
		///      RaytracingContext にフィールドを足すだけで済む。
		void SetRaytracingSettings(const RaytracingContext& settings);

		void GameRender(GameTimer& timer, LoaderSystem& loaderSystem, ResourceCache& resourceCache, World& world);

		void CanvasRender(WorldTimer& timer, const CanvasCamera& canvasCamera, LoaderSystem& loaderSystem, ResourceCache& resourceCache, World& world);

		/// [EN] Isolated single-model preview (Timeline panel). Unlike
		///      CanvasRender, does not reuse EditorRender's gathered instance
		///      list — meshAssetId/animationAssetId/time select exactly one
		///      model+pose, independent of the live World/ECS.
		/// [JP] 単体モデルプレビュー(Timelineパネル)。CanvasRenderと異なり
		///      EditorRenderが収集したインスタンス一覧は再利用しない —
		///      meshAssetId/animationAssetId/timeでモデル+ポーズを1つだけ
		///      指定する。ライブのWorld/ECSに依存しない。
		void PreviewRender(WorldTimer& timer, const PreviewCamera& previewCamera, LoaderSystem& loaderSystem, ResourceCache& resourceCache, Uint32 meshAssetId, Uint32 animationAssetId, Float time, const Matrix& worldMatrix);

		void Begin();

		void End();

		void Clear();

		D3D12Context* GetContext()const;

		SwapChain* GetSwapChain()const;

		[[nodiscard]] D3D12_GPU_DESCRIPTOR_HANDLE GetEditorGPUHandle()const;

		[[nodiscard]] D3D12_GPU_DESCRIPTOR_HANDLE GetGameGPUHandle()const;

		BindlessHeap* GetBindlessHeap()const;

		ShaderCache& GetShaderCache()const;

		BC7CompressShader& GetBC7CompressShader()const;

		void RegisterImGuiShaderResourceViews(ID3D12Device* device, DescriptorHeap* imguiHeap);

		/// [EN] Per-pass GPU times, for the editor's profiler panel. Passed by
		///      reference to whoever draws the panel rather than parked in
		///      EditorContext, so the dependency stays visible in signatures.
		/// [JP] エディタのプロファイラパネル向けのパス別 GPU 時間。EditorContext
		///      に置かずパネルを描く側へ引数で渡すことで、依存関係がシグネチャに
		///      見える形にしておく。
		[[nodiscard]] const GpuProfiler& GetGpuProfiler()const;

		[[nodiscard]] D3D12_GPU_DESCRIPTOR_HANDLE EditorImGuiGPUHandle()const;

		[[nodiscard]] D3D12_GPU_DESCRIPTOR_HANDLE GameImGuiGPUHandle()const;

		[[nodiscard]] D3D12_GPU_DESCRIPTOR_HANDLE CanvasImGuiGPUHandle()const;

		[[nodiscard]] D3D12_GPU_DESCRIPTOR_HANDLE PreviewImGuiGPUHandle()const;

		CameraSystem& GetCameraSystem();

		[[nodiscard]] EffekseerManager* GetEffekseerManager()const;

		void DrawSplashScreen(Bool loadComplete, Float progress, Bool showWarning, Bool showFiction);

		[[nodiscard]] Bool IsSplashFinished()const;

		/**
		* [EN]
		* GraphicsEngine is statically linked into SeedCore.dll while ImGui.lib
		* is also statically linked into Editor.exe separately — each binary
		* ends up with its own private copy of ImGui's global context pointer.
		* Editor.exe must call this once, right after ImGui::CreateContext(),
		* so GraphicsEngine's copy of ImGui points at the same ImGuiContext;
		* otherwise ImGui calls made from inside SeedCore.dll (e.g. component
		* OnInspectorGUI) crash on a null context.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* GraphicsEngine は SeedCore.dll に静的リンクされる一方、ImGui.lib は
		* Editor.exe 側にも別途静的リンクされるため、各バイナリが ImGui の
		* グローバルコンテキストポインタを個別に持ってしまう。Editor.exe は
		* ImGui::CreateContext() 直後にこれを一度呼び、GraphicsEngine 側の
		* ImGui を同じ ImGuiContext に向けさせる必要がある — そうしないと
		* SeedCore.dll 内から呼ばれる ImGui（コンポーネントの OnInspectorGUI 等）
		* が null コンテキストでクラッシュする。
		*/
		static void SetImGuiContext(ImGuiContext* context);

	private:
		void WaitForGpuIdle();

	private:
		Float width_ = ScResolution::SC_HD.Width;

		Float height_ = ScResolution::SC_HD.Height;

		/// [EN] Native (G-Buffer/render) and output (DLSS-RR/PostProcess
		///      upscale target) resolution — distinct from width_/height_
		///      above, which is the OS window size. Set by Initialize()/
		///      Resize(), fed to Renderer::Create()/Resize() and used for
		///      screenSize_ in the per-view SceneConstantBuffer.
		/// [JP] ネイティブ(G-Buffer/レンダー)解像度と出力(DLSS-RR/PostProcess
		///      アップスケール先)解像度 — 上の width_/height_(OSウィンドウ
		///      サイズ)とは別物。Initialize()/Resize() が設定し、
		///      Renderer::Create()/Resize() へ渡し、各ビューの
		///      SceneConstantBuffer の screenSize_ に使う。
		Uint32 nativeWidth_ = static_cast<Uint32>(ScResolution::SC_HD.Width);
		Uint32 nativeHeight_ = static_cast<Uint32>(ScResolution::SC_HD.Height);
		Uint32 outputWidth_ = static_cast<Uint32>(ScResolution::SC_HD.Width);
		Uint32 outputHeight_ = static_cast<Uint32>(ScResolution::SC_HD.Height);

		ResourcePtr<D3D12Context> context_;

		ResourcePtr<SwapChain> swapChain_;

		ResourcePtr<ShaderCache> shaderCache_;

		ResourcePtr<BC7CompressShader> bc7CompressShader_;

		ResourcePtr<BindlessHeap> bindlessHeap_;

		ResourcePtr<DlssManager> dlssManager_;

		ResourcePtr<SceneSystem> editorSceneSystem_;
		ResourcePtr<SceneSystem> gameSceneSystem_;
		ResourcePtr<SceneSystem> canvasSceneSystem_;

		CameraSystem cameraSystem_;

		MovieSystem movieSystem_;

		ResourcePtr<Renderer> renderer_;

		SplashScreen splashScreen_;

		FadeScreen fadeScreen_;
	};
}