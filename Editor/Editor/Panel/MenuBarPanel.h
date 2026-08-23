#pragma once
#include <FoundationEngine/Prelude.h>
#include <GraphicsEngine/Renderer/ViewMode.h>
#include <Editor/Editor/Panel/GraphicsMenuPanel.h>
#include <Editor/Editor/Build/RuntimeBuilder.h>

namespace SeedCore
{
	struct EditorContext;

	class MenuBarPanel
	{
	public:
		MenuBarPanel(EditorContext& context);
		~MenuBarPanel() = default;

		void Draw();

		Bool ConsumeShortCutKeyRequest();

		Bool ConsumeSpecMemoRequest();

		Bool ConsumeConsoleRequest();

		Bool ConsumeProfilerRequest();

		Bool ConsumeTodoListRequest();

		Bool ConsumeVersionRequest();

		Bool ConsumeConfigRequest();

		Bool ConsumeLayerSettingsRequest();

		Bool ConsumeAnimatorControllerRequest();

		Bool ConsumeTimelineRequest();

		Bool ConsumeBoneControllerRequest();

		Bool ConsumeMaterialRequest();

		Bool ConsumeModelTransformRequest();

		[[nodiscard]] ViewMode GetViewMode()const;

	private:
		enum class PendingSceneOp
		{
			None,
			New,
			OpenPath,
			OpenAsset,
			Exit,
		};

		void BuildRuntime();

		void RequestSceneSwitch(PendingSceneOp op, const std::filesystem::path& path, Uint32 assetID);

		void ExecutePendingSceneOp();

		void NewScene();

		void OpenScene();

		void SaveScene();

		void OverwriteSaveScene();

	private:
		EditorContext& context_;

		Bool shortCutKeyRequested_ = false;
		Bool specMemoRequested_ = false;
		Bool consoleRequested_ = false;
		Bool profilerRequested_ = false;
		Bool todoListRequested_ = false;
		Bool versionRequested_ = false;
		Bool configRequested_ = false;
		Bool layerSettingsRequested_ = false;
		Bool animatorControllerRequested_ = false;
		Bool timelineRequested_ = false;
		Bool boneControllerRequested_ = false;
		Bool materialRequested_ = false;
		Bool modelTransformRequested_ = false;

		GraphicsMenuPanel graphicsMenuPanel_;
		RuntimeBuilder runtimeBuilder_;

		PendingSceneOp pendingSceneOp_ = PendingSceneOp::None;
		std::filesystem::path pendingScenePath_;
		Uint32 pendingSceneAssetID_ = 0;
	};
}
