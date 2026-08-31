#include <Editor/Editor/Panel/MenuBarPanel.h>
#include <Editor/Editor/EditorContext.h>
#include <FoundationEngine/File/FileDialog.h>
#include <FoundationEngine/Log/Notice.h>
#include <FoundationEngine/Log/Warning.h>
#include <FoundationEngine/Resource/Scene.h>
#include <FoundationEngine/Resource/ResourceCache.h>
#include <FoundationEngine/ECS/World.h>
#include <FoundationEngine/Time/GameTimer.h>

namespace SeedCore
{
	MenuBarPanel::MenuBarPanel(EditorContext& context) : context_(context), graphicsMenuPanel_(context)
	{
		/// No Code
	}

	void MenuBarPanel::Draw()
	{
		const Bool isPlaying = context_.worldContext_.gameTimer_->IsPlaying();

		if (context_.sceneContext_.requestedSceneAssetID_ != 0)
		{
			Uint32 assetID = context_.sceneContext_.requestedSceneAssetID_;
			context_.sceneContext_.requestedSceneAssetID_ = 0;
			if (!isPlaying)
			{
				RequestSceneSwitch(PendingSceneOp::OpenAsset, {}, assetID);
			}
		}

		if (context_.exitRequested_)
		{
			context_.exitRequested_ = false;
			RequestSceneSwitch(PendingSceneOp::Exit, {}, 0);
		}

		ImGuiIO& io = ImGui::GetIO();
		if (!isPlaying && io.KeyCtrl && !io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_N, false))
		{
			RequestSceneSwitch(PendingSceneOp::New, {}, 0);
		}
		if (!isPlaying && io.KeyCtrl && !io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_O, false))
		{
			OpenScene();
		}
		if (!isPlaying && io.KeyCtrl && io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_S, false))
		{
			SaveScene();
		}
		if (!isPlaying && io.KeyCtrl && !io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_S, false))
		{
			OverwriteSaveScene();
		}
		if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_F1, false))
		{
			shortCutKeyRequested_ = true;
		}
		if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_F2, false))
		{
			specMemoRequested_ = true;
		}
		if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_F3, false))
		{
			consoleRequested_ = true;
		}
		if (io.KeyAlt && ImGui::IsKeyPressed(ImGuiKey_F4, false))
		{
			RequestSceneSwitch(PendingSceneOp::Exit, {}, 0);
		}
		if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_F4, false))
		{
			profilerRequested_ = true;
		}
		if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_F8, false))
		{
			todoListRequested_ = true;
		}
		if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_F9, false))
		{
			versionRequested_ = true;
		}

		if (ImGui::BeginMainMenuBar())
		{
			if (ImGui::BeginMenu("ファイル"))
			{
				ImGui::BeginDisabled(isPlaying);
				if (ImGui::MenuItem("Runtimeビルドでexeを出力"))
				{
					BuildRuntime();
				}
				ImGui::Separator();
				if (ImGui::MenuItem("新規", "Ctrl+N"))
				{
					RequestSceneSwitch(PendingSceneOp::New, {}, 0);
				}
				if (ImGui::MenuItem("開く", "Ctrl+O"))
				{
					OpenScene();
				}
				if (ImGui::MenuItem("保存", "Ctrl+Shift+S"))
				{
					SaveScene();
				}
				if (ImGui::MenuItem("上書き保存", "Ctrl+S"))
				{
					OverwriteSaveScene();
				}
				ImGui::EndDisabled();
				ImGui::Separator();
				if (ImGui::MenuItem("終了", "Alt+F4"))
				{
					RequestSceneSwitch(PendingSceneOp::Exit, {}, 0);
				}
				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("編集"))
			{
				if (ImGui::MenuItem("元に戻す", "Ctrl+Z", false, !isPlaying))
				{
					context_.sceneContext_.history_.Undo();
				}
				if (ImGui::MenuItem("やり直す", "Ctrl+Y", false, !isPlaying))
				{
					context_.sceneContext_.history_.Redo();
				}
				ImGui::Separator();
				if (ImGui::MenuItem("レイヤー編集"))
				{
					layerSettingsRequested_ = true;
				}
				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("ウィンドウ"))
			{
				if (ImGui::MenuItem("アニメーターコントローラー"))
				{
					animatorControllerRequested_ = true;
				}
				if (ImGui::MenuItem("タイムライン"))
				{
					timelineRequested_ = true;
				}
				if (ImGui::MenuItem("ボーンコントローラー"))
				{
					boneControllerRequested_ = true;
				}
				if (ImGui::MenuItem("マテリアルビューア"))
				{
					materialViewerRequested_ = true;
				}
				if (ImGui::MenuItem("モデル変換"))
				{
					modelTransformRequested_ = true;
				}
				ImGui::EndMenu();
			}

			graphicsMenuPanel_.Draw();

			if (ImGui::BeginMenu("ヘルプ"))
			{
				if (ImGui::MenuItem("ショートカットキー一覧", "Ctrl+F1"))
				{
					shortCutKeyRequested_ = true;
				}
				if (ImGui::MenuItem("仕様メモ", "Ctrl+F2"))
				{
					specMemoRequested_ = true;
				}
				ImGui::Separator();
				if (ImGui::MenuItem("コンソール", "Ctrl+F3"))
				{
					consoleRequested_ = true;
				}
				if (ImGui::MenuItem("パフォーマンスプロファイラー", "Ctrl+F4"))
				{
					profilerRequested_ = true;
				}
				ImGui::Separator();
				if (ImGui::MenuItem("ToDoリスト", "Ctrl+F8"))
				{
					todoListRequested_ = true;
				}
				if (ImGui::MenuItem("バージョン情報", "Ctrl+F9"))
				{
					versionRequested_ = true;
				}
				ImGui::Separator();
				if (ImGui::MenuItem("エンジン/ゲーム構成設定"))
				{
					configRequested_ = true;
				}
				ImGui::EndMenu();
			}

			ImGui::EndMainMenuBar();
		}

		if (pendingSceneOp_ != PendingSceneOp::None && !ImGui::IsPopupOpen("未保存の変更"))
		{
			ImGui::OpenPopup("未保存の変更");
		}

		if (ImGui::BeginPopupModal("未保存の変更", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		{
			ImGui::Text("現在のシーンを保存しますか？");
			ImGui::Separator();

			if (ImGui::Button("保存"))
			{
				OverwriteSaveScene();
				ExecutePendingSceneOp();
				ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if (ImGui::Button("保存しない"))
			{
				ExecutePendingSceneOp();
				ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if (ImGui::Button("キャンセル"))
			{
				pendingSceneOp_ = PendingSceneOp::None;
				ImGui::CloseCurrentPopup();
			}

			ImGui::EndPopup();
		}

		if (runtimeBuilder_.IsBuilding() && !ImGui::IsPopupOpen("Runtimeビルド中"))
		{
			ImGui::OpenPopup("Runtimeビルド中");
		}

		if (ImGui::BeginPopupModal("Runtimeビルド中", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove))
		{
			ImGui::Text("Runtimeをビルドしています...");
			ImGui::ProgressBar(runtimeBuilder_.GetProgress(), ImVec2(300, 0));

			if (!runtimeBuilder_.IsBuilding())
			{
				ImGui::CloseCurrentPopup();
			}

			ImGui::EndPopup();
		}

		Bool buildSuccess = false;
		std::string buildLog;
		if (runtimeBuilder_.ConsumeResult(buildSuccess, buildLog))
		{
			if (buildSuccess)
			{
				SC_LOG_NOTICE("Runtimeビルドが完了しました");
			}
			else
			{
				SC_LOG_WARNING("Runtimeビルドに失敗しました: {}", buildLog);
			}
		}
	}

	Bool MenuBarPanel::ConsumeShortCutKeyRequest()
	{
		if (shortCutKeyRequested_)
		{
			shortCutKeyRequested_ = false;
			return true;
		}
		return false;
	}

	Bool MenuBarPanel::ConsumeSpecMemoRequest()
	{
		if (specMemoRequested_)
		{
			specMemoRequested_ = false;
			return true;
		}
		return false;
	}

	Bool MenuBarPanel::ConsumeConsoleRequest()
	{
		if (consoleRequested_)
		{
			consoleRequested_ = false;
			return true;
		}
		return false;
	}

	Bool MenuBarPanel::ConsumeProfilerRequest()
	{
		if (profilerRequested_)
		{
			profilerRequested_ = false;
			return true;
		}
		return false;
	}

	Bool MenuBarPanel::ConsumeTodoListRequest()
	{
		if (todoListRequested_)
		{
			todoListRequested_ = false;
			return true;
		}
		return false;
	}

	Bool MenuBarPanel::ConsumeVersionRequest()
	{
		if (versionRequested_)
		{
			versionRequested_ = false;
			return true;
		}
		return false;
	}

	Bool MenuBarPanel::ConsumeConfigRequest()
	{
		if (configRequested_)
		{
			configRequested_ = false;
			return true;
		}
		return false;
	}

	Bool MenuBarPanel::ConsumeLayerSettingsRequest()
	{
		if (layerSettingsRequested_)
		{
			layerSettingsRequested_ = false;
			return true;
		}
		return false;
	}

	Bool MenuBarPanel::ConsumeAnimatorControllerRequest()
	{
		if (animatorControllerRequested_)
		{
			animatorControllerRequested_ = false;
			return true;
		}
		return false;
	}

	Bool MenuBarPanel::ConsumeTimelineRequest()
	{
		if (timelineRequested_)
		{
			timelineRequested_ = false;
			return true;
		}
		return false;
	}

	Bool MenuBarPanel::ConsumeBoneControllerRequest()
	{
		if (boneControllerRequested_)
		{
			boneControllerRequested_ = false;
			return true;
		}
		return false;
	}

	Bool MenuBarPanel::ConsumeMaterialViewerRequest()
	{
		if (materialViewerRequested_)
		{
			materialViewerRequested_ = false;
			return true;
		}
		return false;
	}

	Bool MenuBarPanel::ConsumeModelTransformRequest()
	{
		if (modelTransformRequested_)
		{
			modelTransformRequested_ = false;
			return true;
		}
		return false;
	}

	ViewMode MenuBarPanel::GetViewMode()const
	{
		return context_.viewportContext_.viewMode_;
	}

	void MenuBarPanel::BuildRuntime()
	{
		if (runtimeBuilder_.IsBuilding())
		{
			return;
		}

		SC_LOG_NOTICE("Runtimeビルドを開始します");
		runtimeBuilder_.BuildAsync(context_.worldContext_.resource_->ProjectRootPath());
	}

	void MenuBarPanel::RequestSceneSwitch(PendingSceneOp op, const std::filesystem::path& path, Uint32 assetID)
	{
		pendingSceneOp_ = op;
		pendingScenePath_ = path;
		pendingSceneAssetID_ = assetID;

		if (context_.worldContext_.world_->GetActors().empty())
		{
			ExecutePendingSceneOp();
		}
	}

	void MenuBarPanel::ExecutePendingSceneOp()
	{
		PendingSceneOp op = pendingSceneOp_;
		std::filesystem::path path = pendingScenePath_;
		Uint32 assetID = pendingSceneAssetID_;

		pendingSceneOp_ = PendingSceneOp::None;
		pendingScenePath_.clear();
		pendingSceneAssetID_ = 0;

		switch (op)
		{
		case PendingSceneOp::New:
			NewScene();
			break;
		case PendingSceneOp::OpenPath:
		{
			String raytracingSettingsJson;
			if (!Scene::Load(*context_.worldContext_.world_, *context_.worldContext_.resource_, path, &raytracingSettingsJson))
			{
				SC_LOG_WARNING("シーンの読み込みに失敗しました: {}", path.string());
				break;
			}
			context_.viewportContext_.raytracing_ = DeserializeRaytracingContext(raytracingSettingsJson);
			/// [JP] DLSS有効/モードはネイティブ解像度の再計算(Engine::MainLoop の
			///      resizeRequested_ 分岐)を経て初めて反映されるため、ここで立てる。
			context_.viewportContext_.resizeRequested_ = true;
			context_.sceneContext_.currentScenePath_ = path;
			context_.selectionContext_.selectedActor_ = Actor();
			context_.selectionContext_.selectedActors_.clear();
			SC_LOG_NOTICE("シーンを読み込みました: {}", path.string());
			break;
		}
		case PendingSceneOp::OpenAsset:
		{
			String raytracingSettingsJson;
			if (!Scene::Load(*context_.worldContext_.world_, *context_.worldContext_.resource_, assetID, &raytracingSettingsJson))
			{
				SC_LOG_WARNING("シーンの読み込みに失敗しました(assetID: {})", assetID);
				break;
			}
			context_.viewportContext_.raytracing_ = DeserializeRaytracingContext(raytracingSettingsJson);
			/// [JP] DLSS有効/モードはネイティブ解像度の再計算(Engine::MainLoop の
			///      resizeRequested_ 分岐)を経て初めて反映されるため、ここで立てる。
			context_.viewportContext_.resizeRequested_ = true;
			context_.sceneContext_.currentScenePath_ = context_.worldContext_.resource_->GetAsset(assetID)->fullpath_.c_str();
			context_.selectionContext_.selectedActor_ = Actor();
			context_.selectionContext_.selectedActors_.clear();
			SC_LOG_NOTICE("シーンを読み込みました: {}", context_.sceneContext_.currentScenePath_.string());
			break;
		}
		case PendingSceneOp::Exit:
			PostQuitMessage(0);
			break;
		default:
			break;
		}
	}

	void MenuBarPanel::NewScene()
	{
		context_.worldContext_.world_->DestroyActors();

		context_.sceneContext_.currentScenePath_.clear();
		context_.selectionContext_.selectedActor_ = Actor();
		context_.selectionContext_.selectedActors_.clear();

		SC_LOG_NOTICE("新規シーンを作成しました");
	}

	void MenuBarPanel::OpenScene()
	{
		std::filesystem::path sceneDir = context_.worldContext_.resource_->ProjectRootPath() / "UserProject" / "Assets" / "Scene";
		std::filesystem::create_directories(sceneDir);

		std::filesystem::path selectedPath;
		if (!FileDialog::OpenFile(selectedPath, sceneDir, L"Scene Files (*.scene)", L"*.scene"))
		{
			return;
		}

		RequestSceneSwitch(PendingSceneOp::OpenPath, selectedPath, 0);
	}

	void MenuBarPanel::SaveScene()
	{
		std::filesystem::path sceneDir = context_.worldContext_.resource_->ProjectRootPath() / "UserProject" / "Assets" / "Scene";
		std::filesystem::create_directories(sceneDir);

		std::filesystem::path savePath;
		if (!FileDialog::SaveFile(savePath, sceneDir, L"Scene Files (*.scene)", L"*.scene", L"scene"))
		{
			return;
		}

		if (!Scene::Save(*context_.worldContext_.world_, *context_.worldContext_.resource_, savePath, SerializeRaytracingContext(context_.viewportContext_.raytracing_)))
		{
			SC_LOG_WARNING("シーンの保存に失敗しました: {}", savePath.string());
			return;
		}

		context_.sceneContext_.currentScenePath_ = savePath;

		SC_LOG_NOTICE("シーンを保存しました: {}", savePath.string());
	}

	void MenuBarPanel::OverwriteSaveScene()
	{
		if (context_.sceneContext_.currentScenePath_.empty())
		{
			SaveScene();
			return;
		}

		if (!Scene::Save(*context_.worldContext_.world_, *context_.worldContext_.resource_, context_.sceneContext_.currentScenePath_, SerializeRaytracingContext(context_.viewportContext_.raytracing_)))
		{
			SC_LOG_WARNING("シーンの上書き保存に失敗しました: {}", context_.sceneContext_.currentScenePath_.string());
			return;
		}

		SC_LOG_NOTICE("シーンを上書き保存しました: {}", context_.sceneContext_.currentScenePath_.string());
	}
}
