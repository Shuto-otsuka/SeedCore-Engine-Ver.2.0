#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/ECS/Entity.h>
#include <FoundationEngine/ECS/WorldSnapshot.h>
#include <Editor/Editor/GizmoContext.h>
#include <GraphicsEngine/Renderer/ViewMode.h>
#include <GraphicsEngine/Raytracing/RaytracingContext.h>
#include <GraphicsEngine/D3D12/SwapChain/GraphicsResolution.h>

namespace SeedCore
{
	class Actor;
	class World;
	class ResourceCache;
	class DescriptorHeap;
	class GameTimer;
	class EditorCamera;
	class EditorCameraController;
	class CanvasCamera;
	class PreviewCamera;
	class CameraSystem;
	struct LoaderSystem;
	class BC7CompressShader;
	class BindlessHeap;
	class ImGuiRenderer;

	struct EditorContext
	{
		World* world_ = nullptr;
		ResourceCache* resource_ = nullptr;
		LoaderSystem* loader_ = nullptr;
		GameTimer* gameTimer_ = nullptr;
		EditorCamera* editorCamera_ = nullptr;
		EditorCameraController* editorCameraController_ = nullptr;
		CanvasCamera* canvasCamera_ = nullptr;
		PreviewCamera* previewCamera_ = nullptr;
		CameraSystem* cameraSystem_ = nullptr;

		ImGuiRenderer* imgui_ = nullptr;

		ID3D12Device* device_ = nullptr;
		ID3D12CommandQueue* cmdQueue_ = nullptr;
		BC7CompressShader* bc7Shader_ = nullptr;
		IDXGIAdapter4* adapter_ = nullptr;
		DescriptorHeap* descHeap_ = nullptr;
		BindlessHeap* bindlessHeap_ = nullptr;

		Uint64 uiFrame_ = 0;

		Entity selectedEntity_ = Entity::Null();
		Actor* selectedActor_ = nullptr;

		/// [EN] Full multi-selection from the Hierarchy panel. selectedActor_ is always
		///      the "primary" entry (last clicked), kept in sync for Inspector/Gizmo,
		///      which are not multi-select aware.
		/// [JP] Hierarchy パネルからの全複数選択。selectedActor_ は常に「主選択」
		///      （最後にクリックされたもの）であり、複数選択に非対応な
		///      Inspector/Gizmo のために同期を保つ。
		DynamicArray<Actor*> selectedActors_;

		GuizmoContext guizmo_;

		WorldSnapshot worldSnapshot_;

		std::filesystem::path currentScenePath_;

		Uint32 requestedSceneAssetID_ = 0;

		Bool exitRequested_ = false;

		/// [JP] エディタービューの表示モード。メニュー(グラフィックス→ビューモード)と
		///      エディタービューのツールバー、両方から共有して切り替える。
		ViewMode viewMode_ = ViewMode::Lit;

		/// [JP] レイトレーシング機能の設定。メニュー(グラフィックス→
		///      レイトレーシング)から編集する。
		RaytracingContext raytracing_;

		/// [EN] Live render/output resolution, edited from ConfigPanel's ゲーム設定
		///      tab. Engine::MainLoop checks resizeRequested_ once per frame
		///      (before Graphics::Begin) and, if set, derives the native (DLSS-
		///      scaled) resolution from this + raytracing_.dlssRayReconstructionEnabled_/
		///      dlssMode_, calls Graphics::Resize, then clears the flag.
		/// [JP] ライブのレンダー/出力解像度。ConfigPanel の ゲーム設定タブから
		///      編集する。Engine::MainLoop が毎フレーム(Graphics::Begin より前に)
		///      resizeRequested_ を確認し、立っていればこれと
		///      raytracing_.dlssRayReconstructionEnabled_/dlssMode_ から
		///      ネイティブ(DLSSスケール後)解像度を導出して Graphics::Resize を
		///      呼び、フラグを下ろす。
		ResolutionPreset outputResolution_ = ResolutionPreset::HD;
		Bool resizeRequested_ = false;

		/// [EN] Timeline panel's 3D preview selection, written by TimelinePanel
		///      each frame and read by Engine::MainLoop before calling
		///      Graphics::PreviewRender — one frame of lag is fine here.
		/// [JP] Timelineパネルの3Dプレビュー選択。毎フレームTimelinePanelが
		///      書き込み、Engine::MainLoopがGraphics::PreviewRenderを呼ぶ前に
		///      読む — 1フレームの遅延は許容する。
		Bool previewActive_ = false;
		Uint32 previewMeshAssetId_ = 0;
		Uint32 previewAnimationAssetId_ = 0;
		Float previewTime_ = 0.0f;
	};
}
