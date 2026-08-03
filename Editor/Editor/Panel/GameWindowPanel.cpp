#include <Editor/Editor/Panel/GameWindowPanel.h>
#include <FoundationEngine/Input/InputSystem.h>

namespace SeedCore
{
	namespace
	{
		/// [EN] Right-click (rotate) or middle-click (pan) held over the game
		///      viewport locks the OS cursor in place (hidden, re-anchored
		///      every frame) so a long drag never hits a monitor edge and
		///      stalls input, same as EditorWindowPanel. Begin only while
		///      hovering; the release check is unconditional so capture
		///      always ends even if hover state changed mid-drag. Shared by
		///      both the windowed and fullscreen game view paths below.
		/// [JP] ゲームビューポート上での右クリック（回転）/中クリック（パン）
		///      押下中は OS カーソルを固定する（EditorWindowPanel と同様）。
		///      開始は hover 中のみ、解除判定は無条件。ウィンドウ表示・全画面
		///      表示どちらのパスからも共有する。
		void UpdateFreeCameraMouseCapture(Bool hovered)
		{
			Bool rotateHeld = InputSystem::MouseState(InputSystem::MouseButton::Right, InputSystem::IsPressed);
			Bool panHeld = InputSystem::MouseState(InputSystem::MouseButton::Middle, InputSystem::IsPressed);

			if (hovered && (rotateHeld || panHeld) && !InputSystem::IsMouseCaptured())
			{
				InputSystem::BeginMouseCapture();
			}

			if (!rotateHeld && !panHeld && InputSystem::IsMouseCaptured())
			{
				InputSystem::EndMouseCapture();
			}
		}
	}

	GameWindowPanel::GameWindowPanel(CameraSystem& cameraSystem) : cameraSystem_(cameraSystem)
	{
		/// No Code
	}

	void GameWindowPanel::Draw(D3D12_GPU_DESCRIPTOR_HANDLE frameBufferHandle, Float toolbarHeight)
	{
		if ((ImGui::IsKeyDown(ImGuiKey_LeftCtrl) || ImGui::IsKeyDown(ImGuiKey_RightCtrl)) && ImGui::IsKeyPressed(ImGuiKey_F11))
		{
			fullscreen_ = !fullscreen_;
		}

		if (fullscreen_)
		{
			/// [EN] Fill the dockspace region only: keep the main menu bar and the
			///      play/stop toolbar. WorkPos/WorkSize exclude the menu bar; add the
			///      toolbar height to start below it (same region as DockSpaceBegin).
			/// [JP] ドックスペース領域だけを埋める: メインメニューと再生/停止ツール
			///      バーは残す。WorkPos/WorkSize はメニューバー分を除いた領域なので、
			///      ツールバー高さを足してその下から開始（DockSpaceBegin と同じ領域）。
			ImGuiViewport* viewport = ImGui::GetMainViewport();
			ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x, viewport->WorkPos.y + toolbarHeight));
			ImGui::SetNextWindowSize(ImVec2(viewport->WorkSize.x, viewport->WorkSize.y - toolbarHeight));

			/// [EN] No NoBringToFrontOnFocus here: the fullscreen game view is an
			///      overlay that must sit ON TOP of the dockspace host window,
			///      otherwise the host's background hides the game image.
			/// [JP] ここで NoBringToFrontOnFocus は付けない: 全画面ゲームビューは
			///      DockSpace ホストの前面に出す必要があるオーバーレイで、付けると
			///      ホストの背景に隠れてゲーム画像が見えなくなる。
			ImGuiWindowFlags fullFlags = ImGuiWindowFlags_NoTitleBar
				| ImGuiWindowFlags_NoResize
				| ImGuiWindowFlags_NoMove
				| ImGuiWindowFlags_NoScrollbar
				| ImGuiWindowFlags_NoScrollWithMouse
				| ImGuiWindowFlags_NoDocking
				| ImGuiWindowFlags_NoSavedSettings;

			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
			ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

			if (ImGui::Begin("##GameFullscreen", nullptr, fullFlags))
			{
				/// [JP] 全画面でも通常ビューと同じく Free カメラ入力を処理する
				///      （Free モード＆ホバー中）。これが無いと全画面でカメラが動かない。
				Bool fullscreenHovered = ImGui::IsWindowHovered();
				UpdateFreeCameraMouseCapture(cameraSystem_.GetMode() == CameraSystem::Mode::Free && fullscreenHovered);

				if (cameraSystem_.GetMode() == CameraSystem::Mode::Free && fullscreenHovered)
				{
					Float deltaTime = ImGui::GetIO().DeltaTime;
					cameraSystem_.UpdateFreeCameraInput(deltaTime);
				}

				ImVec2 regionSize = ImGui::GetContentRegionAvail();

				constexpr Float aspectRatio = 16.0f / 9.0f;
				Float imageWidth = regionSize.x;
				Float imageHeight = regionSize.x / aspectRatio;

				if (imageHeight > regionSize.y)
				{
					imageHeight = regionSize.y;
					imageWidth = regionSize.y * aspectRatio;
				}

				imageWidth = ImFloor(imageWidth);
				imageHeight = ImFloor(imageHeight);
				Float offsetX = ImFloor((regionSize.x - imageWidth) * 0.5f);
				Float offsetY = ImFloor((regionSize.y - imageHeight) * 0.5f);

				ImGui::SetCursorPos(ImVec2(offsetX, offsetY));
				ImGui::Image(ImTextureID(frameBufferHandle.ptr), ImVec2(imageWidth, imageHeight));
				imageHovered_ = ImGui::IsItemHovered();
			}
			else
			{
				imageHovered_ = false;
			}
			ImGui::End();
			ImGui::PopStyleVar(3);
			return;
		}

		ImGuiID dockspaceID = ImGui::GetID("ScDockSpace");
		ImGui::SetNextWindowDockID(dockspaceID, ImGuiCond_FirstUseEver);

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 8.0f));

		if (ImGui::Begin("ゲームビュー"))
		{
			Int currentMode = static_cast<Int>(cameraSystem_.GetMode());
			ImGui::RadioButton("Free", &currentMode, 0);
			ImGui::SameLine();
			ImGui::RadioButton("User", &currentMode, 1);
			cameraSystem_.SetMode(static_cast<CameraSystem::Mode>(currentMode));

			Bool windowedHovered = ImGui::IsWindowHovered();
			UpdateFreeCameraMouseCapture(cameraSystem_.GetMode() == CameraSystem::Mode::Free && windowedHovered);

			if (cameraSystem_.GetMode() == CameraSystem::Mode::Free && windowedHovered)
			{
				Float deltaTime = ImGui::GetIO().DeltaTime;
				cameraSystem_.UpdateFreeCameraInput(deltaTime);
			}

			ImVec2 regionSize = ImGui::GetContentRegionAvail();

			if (regionSize.x > 0.0f && regionSize.y > 0.0f)
			{
				constexpr Float aspectRatio = 16.0f / 9.0f;
				Float imageWidth = regionSize.x;
				Float imageHeight = regionSize.x / aspectRatio;

				if (imageHeight > regionSize.y)
				{
					imageHeight = regionSize.y;
					imageWidth = regionSize.y * aspectRatio;
				}

				imageWidth = ImFloor(imageWidth);
				imageHeight = ImFloor(imageHeight);
				Float offsetX = ImFloor((regionSize.x - imageWidth) * 0.5f);
				Float offsetY = ImFloor((regionSize.y - imageHeight) * 0.5f);

				ImVec2 cursorPos = ImVec2(ImGui::GetCursorPosX() + offsetX, ImGui::GetCursorPosY() + offsetY);
				ImGui::SetCursorPos(cursorPos);

				ImVec2 screenPos = ImGui::GetCursorScreenPos();
				screenPos.x = IM_ROUND(screenPos.x);
				screenPos.y = IM_ROUND(screenPos.y);
				ImGui::SetCursorScreenPos(screenPos);

				ImGui::PushStyleVar(ImGuiStyleVar_ImageBorderSize, 0.0f);
				ImGui::Image(ImTextureID(frameBufferHandle.ptr), ImVec2(imageWidth, imageHeight));
				ImGui::PopStyleVar();
				imageHovered_ = ImGui::IsItemHovered();

				ImVec2 borderMin = ImVec2(screenPos.x - 1.0f, screenPos.y - 1.0f);
				ImVec2 borderMax = ImVec2(screenPos.x + imageWidth + 1.0f, screenPos.y + imageHeight + 1.0f);
				ImGui::GetWindowDrawList()->AddRect(borderMin, borderMax, ImGui::GetColorU32(ImGuiCol_Border));
			}
			else
			{
				imageHovered_ = false;
			}
		}
		else
		{
			imageHovered_ = false;
		}

		ImGui::End();
		ImGui::PopStyleVar();
	}
}
