#include <Editor/Editor/Panel/ConfigPanel.h>
#include <Editor/Editor/EditorContext.h>
#include <Editor/Editor/ImGui/ImGuiRenderer.h>
#include <FoundationEngine/Input/InputSystem.h>

namespace SeedCore
{
	namespace
	{
		/// [EN] Every InputSystem::Key entry, paired with its display label,
		///      for the "add key binding" combo in DrawInputBindingTab().
		/// [JP] InputSystem::Key の全項目を、表示ラベルと組にしたもの。
		///      DrawInputBindingTab() の「キーバインド追加」コンボで使う。
		constexpr std::pair<const Char*, InputSystem::Key> keyEntries[] =
		{
			{ "Backspace", InputSystem::Key::Backspace },
			{ "Tab", InputSystem::Key::Tab },
			{ "Enter", InputSystem::Key::Enter },
			{ "Shift", InputSystem::Key::Shift },
			{ "Control", InputSystem::Key::Control },
			{ "Alt", InputSystem::Key::Alt },
			{ "CapsLock", InputSystem::Key::CapsLock },
			{ "Escape", InputSystem::Key::Escape },
			{ "Space", InputSystem::Key::Space },
			{ "PageUp", InputSystem::Key::PageUp },
			{ "PageDown", InputSystem::Key::PageDown },
			{ "Home", InputSystem::Key::Home },
			{ "End", InputSystem::Key::End },
			{ "Left", InputSystem::Key::Left },
			{ "Up", InputSystem::Key::Up },
			{ "Right", InputSystem::Key::Right },
			{ "Down", InputSystem::Key::Down },
			{ "Delete", InputSystem::Key::Delete },
			{ "LeftShift", InputSystem::Key::LeftShift },
			{ "RightShift", InputSystem::Key::RightShift },
			{ "LeftControl", InputSystem::Key::LeftControl },
			{ "RightControl", InputSystem::Key::RightControl },
			{ "LeftAlt", InputSystem::Key::LeftAlt },
			{ "RightAlt", InputSystem::Key::RightAlt },
			{ "0", InputSystem::Key::Num0 }, { "1", InputSystem::Key::Num1 }, { "2", InputSystem::Key::Num2 },
			{ "3", InputSystem::Key::Num3 }, { "4", InputSystem::Key::Num4 }, { "5", InputSystem::Key::Num5 },
			{ "6", InputSystem::Key::Num6 }, { "7", InputSystem::Key::Num7 }, { "8", InputSystem::Key::Num8 },
			{ "9", InputSystem::Key::Num9 },
			{ "A", InputSystem::Key::A }, { "B", InputSystem::Key::B }, { "C", InputSystem::Key::C },
			{ "D", InputSystem::Key::D }, { "E", InputSystem::Key::E }, { "F", InputSystem::Key::F },
			{ "G", InputSystem::Key::G }, { "H", InputSystem::Key::H }, { "I", InputSystem::Key::I },
			{ "J", InputSystem::Key::J }, { "K", InputSystem::Key::K }, { "L", InputSystem::Key::L },
			{ "M", InputSystem::Key::M }, { "N", InputSystem::Key::N }, { "O", InputSystem::Key::O },
			{ "P", InputSystem::Key::P }, { "Q", InputSystem::Key::Q }, { "R", InputSystem::Key::R },
			{ "S", InputSystem::Key::S }, { "T", InputSystem::Key::T }, { "U", InputSystem::Key::U },
			{ "V", InputSystem::Key::V }, { "W", InputSystem::Key::W }, { "X", InputSystem::Key::X },
			{ "Y", InputSystem::Key::Y }, { "Z", InputSystem::Key::Z },
			{ "F1", InputSystem::Key::F1 }, { "F2", InputSystem::Key::F2 }, { "F3", InputSystem::Key::F3 },
			{ "F4", InputSystem::Key::F4 }, { "F5", InputSystem::Key::F5 }, { "F6", InputSystem::Key::F6 },
			{ "F7", InputSystem::Key::F7 }, { "F8", InputSystem::Key::F8 }, { "F9", InputSystem::Key::F9 },
			{ "F10", InputSystem::Key::F10 }, { "F11", InputSystem::Key::F11 }, { "F12", InputSystem::Key::F12 },
		};

		/// [EN] Every commonly-used SDL_GamepadButton, paired with its display
		///      label, for the "add gamepad binding" combo below.
		/// [JP] よく使う SDL_GamepadButton の一覧を、表示ラベルと組にしたもの。
		///      下の「ゲームパッドバインド追加」コンボで使う。
		constexpr std::pair<const Char*, SDL_GamepadButton> gamepadButtonEntries[] =
		{
			{ "A", InputSystem::GamepadButton::A },
			{ "B", InputSystem::GamepadButton::B },
			{ "X", InputSystem::GamepadButton::X },
			{ "Y", InputSystem::GamepadButton::Y },
			{ "LeftShoulder", InputSystem::GamepadButton::LeftShoulder },
			{ "RightShoulder", InputSystem::GamepadButton::RightShoulder },
			{ "LeftStick", InputSystem::GamepadButton::LeftStick },
			{ "RightStick", InputSystem::GamepadButton::RightStick },
			{ "DPadUp", InputSystem::GamepadButton::DPadUp },
			{ "DPadDown", InputSystem::GamepadButton::DPadDown },
			{ "DPadLeft", InputSystem::GamepadButton::DPadLeft },
			{ "DPadRight", InputSystem::GamepadButton::DPadRight },
			{ "Start", InputSystem::GamepadButton::Start },
			{ "Back", InputSystem::GamepadButton::Back },
		};

		const Char* KeyLabel(InputSystem::Key key)
		{
			for (const auto& entry : keyEntries)
			{
				if (entry.second == key)
				{
					return entry.first;
				}
			}
			return "?";
		}

		const Char* GamepadButtonLabel(SDL_GamepadButton button)
		{
			for (const auto& entry : gamepadButtonEntries)
			{
				if (entry.second == button)
				{
					return entry.first;
				}
			}
			return "?";
		}

		/// [EN] Renders items as a row of pill-shaped "chip" buttons (bound
		///      value + label), wrapping onto a new line once the row would
		///      overflow the window's content width, matching imgui_demo's
		///      button-wrapping pattern. Clicking a chip queues it for
		///      removal via onRemove, applied only after every chip has been
		///      drawn - removing mid-loop would shrink/relocate items while
		///      still iterating it.
		/// [JP] items を、丸みを帯びた「チップ」ボタン（値+ラベル）の行として
		///      描画する。行がウィンドウの内容幅を超えたら imgui_demo の
		///      ボタン折り返しパターンと同様に自動改行する。チップをクリック
		///      すると onRemove 経由での削除を予約し、全チップの描画が終わって
		///      から適用する - ループ中に削除すると items を走査中に縮む/
		///      ずれるため。
		template<typename T, typename LabelFunc, typename RemoveFunc>
		void DrawChipList(const DynamicArray<T>& items, LabelFunc&& labelFunc, RemoveFunc&& onRemove)
		{
			if (items.empty())
			{
				ImGui::TextDisabled("(なし)");
				return;
			}

			ImGuiStyle& style = ImGui::GetStyle();
			Float windowVisibleX2 = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;

			DynamicArray<T> toRemove;

			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.24f, 0.47f, 0.85f, 0.55f));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.85f, 0.30f, 0.30f, 0.80f));
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.85f, 0.30f, 0.30f, 0.95f));
			ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 10.0f);

			for (Size index = 0; index < items.size(); ++index)
			{
				std::string label = labelFunc(items[index]) + "  ×";

				ImGui::PushID(static_cast<Int>(index));
				if (ImGui::SmallButton(label.c_str()))
				{
					toRemove.push_back(items[index]);
				}
				ImGui::PopID();

				if (index + 1 < items.size())
				{
					std::string nextLabel = labelFunc(items[index + 1]) + "  ×";
					Float nextWidth = ImGui::CalcTextSize(nextLabel.c_str()).x + style.FramePadding.x * 2.0f;
					Float lastButtonX2 = ImGui::GetItemRectMax().x;
					Float nextButtonX2 = lastButtonX2 + style.ItemSpacing.x + nextWidth;
					if (nextButtonX2 < windowVisibleX2)
					{
						ImGui::SameLine();
					}
				}
			}

			ImGui::PopStyleVar();
			ImGui::PopStyleColor(3);

			for (const T& item : toRemove)
			{
				onRemove(item);
			}
		}
	}

	ConfigPanel::ConfigPanel(EditorContext& context) : context_(context)
	{
		initialScenePathBuffer_.resize(512);
		newActionBuffer_.resize(128);
	}

	void ConfigPanel::Open()
	{
		editorConfig_.Load();
		gameConfig_.Load();

		/// [JP] gameConfig_ は GameConfig.scg(独立した設定ファイル)からの値で、
		///      現在読み込まれているシーンの実際の DLSS/解像度設定
		///      (context_.viewportContext_.raytracing_/context_.viewportContext_.outputResolution_)とは無関係に
		///      ずれ得る。パネルを開く時点の実際の値で上書きし、表示を
		///      現在の状態と一致させる。
		gameConfig_.useDlss_ = context_.viewportContext_.raytracing_.dlssRayReconstructionEnabled_;
		gameConfig_.dlssMode_ = context_.viewportContext_.raytracing_.dlssMode_;
		gameConfig_.resolution_ = context_.viewportContext_.outputResolution_;

		std::fill(initialScenePathBuffer_.begin(), initialScenePathBuffer_.end(), '\0');
		std::string initialScenePath = gameConfig_.initialScenePath_.str();
		std::copy(initialScenePath.begin(), initialScenePath.end(), initialScenePathBuffer_.begin());

		show_ = true;
	}

	Bool ConfigPanel::DrawEditorConfigTab()
	{
		Bool changed = false;

		ImGui::TextDisabled("エディターカメラ");
		ImGui::Spacing();

		changed |= ImGui::DragFloat3("視点(Eye)", &editorConfig_.cameraEye_.x, 0.1f);
		changed |= ImGui::DragFloat3("注視点(Focus)", &editorConfig_.cameraFocus_.x, 0.1f);
		changed |= ImGui::DragFloat3("上方向(Up)", &editorConfig_.cameraUp_.x, 0.01f);
		changed |= ImGui::DragFloat("画角(FOV)", &editorConfig_.cameraFov_, 0.5f, 1.0f, 179.0f, "%.1f");

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		ImGui::TextDisabled("カメラ操作速度");
		ImGui::Spacing();

		changed |= ImGui::DragFloat("移動", &editorConfig_.cameraMoveSpeed_, 0.1f, 0.1f, 200.0f, "%.1f");
		changed |= ImGui::DragFloat("回転", &editorConfig_.cameraRotateSpeed_, 0.01f, 0.01f, 2.0f, "%.2f");
		changed |= ImGui::DragFloat("ズーム", &editorConfig_.cameraScrollSpeed_, 0.1f, 0.1f, 100.0f, "%.1f");
		changed |= ImGui::DragFloat("パン", &editorConfig_.cameraPanSpeed_, 0.001f, 0.001f, 1.0f, "%.3f");
		changed |= ImGui::DragFloat("Shift倍率", &editorConfig_.cameraShiftSpeedMultiplier_, 0.1f, 1.0f, 20.0f, "x%.1f");

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		ImGui::TextDisabled("ImGui");
		ImGui::Spacing();

		if (ImGui::DragFloat("文字サイズ倍率", &editorConfig_.fontScale_, 0.01f, 0.5f, 3.0f, "%.2f"))
		{
			changed = true;
			if (context_.graphicsContext_.imgui_)
			{
				context_.graphicsContext_.imgui_->FontScale(editorConfig_.fontScale_);
			}
		}

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		ImGui::TextDisabled("最後に開いていたシーン");
		ImGui::Spacing();

		std::string lastScenePath = editorConfig_.lastScenePath_.str();
		ImGui::TextWrapped("%s", lastScenePath.empty() ? "(なし)" : lastScenePath.c_str());

		return changed;
	}

	Bool ConfigPanel::DrawGameConfigTab()
	{
		Bool changed = false;

		ImGui::TextDisabled("ウィンドウ");
		ImGui::Spacing();

		Int32 windowWidth = static_cast<Int32>(gameConfig_.windowWidth_);
		if (ImGui::DragInt("幅", &windowWidth, 1.0f, 320, 7680))
		{
			gameConfig_.windowWidth_ = static_cast<Uint32>(windowWidth);
			changed = true;
		}

		Int32 windowHeight = static_cast<Int32>(gameConfig_.windowHeight_);
		if (ImGui::DragInt("高さ", &windowHeight, 1.0f, 240, 4320))
		{
			gameConfig_.windowHeight_ = static_cast<Uint32>(windowHeight);
			changed = true;
		}

		changed |= ImGui::Checkbox("フルスクリーン", &gameConfig_.fullscreen_);

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		ImGui::TextDisabled("解像度");
		ImGui::Spacing();

		const Char* resolutionLabels[] = { "640x360 (HHD)", "1280x720 (HD)", "1920x1080 (FHD)", "2560x1440 (QHD)", "3840x2160 (4K)", "7680x4320 (8K)" };
		Int32 resolutionIndex = static_cast<Int32>(gameConfig_.resolution_);

		if (ImGui::Combo("出力解像度", &resolutionIndex, resolutionLabels, IM_ARRAYSIZE(resolutionLabels)))
		{
			gameConfig_.resolution_ = static_cast<ResolutionPreset>(resolutionIndex);
			changed = true;
			context_.viewportContext_.outputResolution_ = gameConfig_.resolution_;
			context_.viewportContext_.resizeRequested_ = true;
		}

		ImGui::TextDisabled("(エディターのレンダー解像度に即時反映されます)");

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		ImGui::TextDisabled("DLSS");
		ImGui::Spacing();

		if (ImGui::Checkbox("DLSSを使用する", &gameConfig_.useDlss_))
		{
			changed = true;
			context_.viewportContext_.raytracing_.dlssRayReconstructionEnabled_ = gameConfig_.useDlss_;
			context_.viewportContext_.resizeRequested_ = true;
		}

		const Char* dlssModeLabels[] = { "最高性能", "バランス", "最高画質", "超高性能", "DLAA" };
		Int32 dlssModeIndex = static_cast<Int32>(gameConfig_.dlssMode_);

		ImGui::BeginDisabled(!gameConfig_.useDlss_);
		if (ImGui::Combo("パフォーマンス", &dlssModeIndex, dlssModeLabels, IM_ARRAYSIZE(dlssModeLabels)))
		{
			gameConfig_.dlssMode_ = static_cast<DlssMode>(dlssModeIndex);
			changed = true;
			context_.viewportContext_.raytracing_.dlssMode_ = gameConfig_.dlssMode_;
			context_.viewportContext_.resizeRequested_ = true;
		}
		ImGui::EndDisabled();

		ImGui::TextDisabled("(エディターのDLSS-RRプレビューに即時反映されます。解像度も自動調整されます)");

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		ImGui::TextDisabled("起動シーン");
		ImGui::Spacing();

		if (ImGui::InputText("初回シーン", initialScenePathBuffer_.data(), initialScenePathBuffer_.capacity()))
		{
			gameConfig_.initialScenePath_ = String(std::string(initialScenePathBuffer_.c_str()));
			changed = true;
		}

		return changed;
	}

	void ConfigPanel::DrawInputBindingTab()
	{
		ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 6.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 10.0f));

		ImGui::BeginChild("##NewAction", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY);
		{
			ImGui::TextDisabled("新しいアクション");
			ImGui::Spacing();

			ImGui::SetNextItemWidth(220.0f);
			ImGui::InputTextWithHint("##NewActionName", "例: Jump, Move ...", newActionBuffer_.data(), newActionBuffer_.capacity());

			ImGui::SameLine();
			Bool canAdd = newActionBuffer_.c_str()[0] != '\0';
			ImGui::BeginDisabled(!canAdd);
			if (ImGui::Button("追加"))
			{
				InputSystem::RegisterAction(String(std::string(newActionBuffer_.c_str())));
				InputSystem::SaveBindings();
				std::fill(newActionBuffer_.begin(), newActionBuffer_.end(), '\0');
			}
			ImGui::EndDisabled();
		}
		ImGui::EndChild();

		ImGui::Spacing();

		String actionToRemove;
		for (const String& action : InputSystem::GetActionNames())
		{
			ImGui::PushID(action.c_str());

			ImGui::BeginChild("##ActionCard", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY);
			{
				ImGui::TextColored(ImVec4(0.95f, 0.75f, 0.30f, 1.0f), "%s", action.c_str());

				Float deleteWidth = ImGui::CalcTextSize("削除").x + ImGui::GetStyle().FramePadding.x * 2.0f;
				ImGui::SameLine();
				ImGui::SetCursorPosX(ImGui::GetWindowContentRegionMax().x - deleteWidth);
				if (ImGui::SmallButton("削除"))
				{
					actionToRemove = action;
				}

				ImGui::Spacing();

				ImGui::SeparatorText("キー");
				ImGui::PushID("Keys");
				DrawChipList(InputSystem::GetBoundKeys(action),
					[](InputSystem::Key key) { return std::string(KeyLabel(key)); },
					[&action](InputSystem::Key key)
					{
						InputSystem::UnbindKey(action, key);
						InputSystem::SaveBindings();
					});
				ImGui::SetNextItemWidth(160.0f);
				if (ImGui::BeginCombo("##Add", "+ キーを追加"))
				{
					for (const auto& entry : keyEntries)
					{
						if (ImGui::Selectable(entry.first))
						{
							InputSystem::BindKey(action, entry.second);
							InputSystem::SaveBindings();
						}
					}
					ImGui::EndCombo();
				}
				ImGui::PopID();

				ImGui::SeparatorText("ゲームパッドボタン");
				ImGui::PushID("Pad");
				DrawChipList(InputSystem::GetBoundGamepadButtons(action),
					[](SDL_GamepadButton button) { return std::string(GamepadButtonLabel(button)); },
					[&action](SDL_GamepadButton button)
					{
						InputSystem::UnbindGamepadButton(action, button);
						InputSystem::SaveBindings();
					});
				ImGui::SetNextItemWidth(160.0f);
				if (ImGui::BeginCombo("##Add", "+ ボタンを追加"))
				{
					for (const auto& entry : gamepadButtonEntries)
					{
						if (ImGui::Selectable(entry.first))
						{
							InputSystem::BindGamepadButton(action, entry.second);
							InputSystem::SaveBindings();
						}
					}
					ImGui::EndCombo();
				}
				ImGui::PopID();

				ImGui::SeparatorText("移動軸 (WASD/矢印キー)");
				ImGui::PushID("AxisKeys");
				DrawChipList(InputSystem::GetBoundAxisKeys(action),
					[](const InputSystem::DirectionalKeys& keys)
					{
						return std::string(KeyLabel(keys.up_)) + "/" + KeyLabel(keys.left_) + "/" + KeyLabel(keys.down_) + "/" + KeyLabel(keys.right_);
					},
					[&action](const InputSystem::DirectionalKeys& keys)
					{
						InputSystem::UnbindAxisKeys(action, keys);
						InputSystem::SaveBindings();
					});

				InputSystem::DirectionalKeys wasd{ InputSystem::Key::W, InputSystem::Key::S, InputSystem::Key::A, InputSystem::Key::D };
				if (ImGui::SmallButton("+ WASD"))
				{
					InputSystem::BindAxisKeys(action, wasd);
					InputSystem::SaveBindings();
				}
				ImGui::SameLine();

				InputSystem::DirectionalKeys arrows{ InputSystem::Key::Up, InputSystem::Key::Down, InputSystem::Key::Left, InputSystem::Key::Right };
				if (ImGui::SmallButton("+ 矢印キー"))
				{
					InputSystem::BindAxisKeys(action, arrows);
					InputSystem::SaveBindings();
				}
				ImGui::PopID();

				ImGui::SeparatorText("移動軸 (アナログスティック)");
				ImGui::PushID("Sticks");
				DrawChipList(InputSystem::GetBoundSticks(action),
					[](InputSystem::StickSide side) { return std::string(side == InputSystem::StickSide::Left ? "左スティック" : "右スティック"); },
					[&action](InputSystem::StickSide side)
					{
						InputSystem::UnbindStick(action, side);
						InputSystem::SaveBindings();
					});

				if (ImGui::SmallButton("+ 左スティック"))
				{
					InputSystem::BindStick(action, InputSystem::StickSide::Left);
					InputSystem::SaveBindings();
				}
				ImGui::SameLine();
				if (ImGui::SmallButton("+ 右スティック"))
				{
					InputSystem::BindStick(action, InputSystem::StickSide::Right);
					InputSystem::SaveBindings();
				}
				ImGui::PopID();
			}
			ImGui::EndChild();

			ImGui::Spacing();

			ImGui::PopID();
		}

		if (!actionToRemove.str().empty())
		{
			InputSystem::RemoveAction(actionToRemove);
			InputSystem::SaveBindings();
		}

		ImGui::PopStyleVar(2);
	}

	void ConfigPanel::Draw()
	{
		if (!show_)
		{
			return;
		}

		ImVec2 center = ImGui::GetMainViewport()->GetCenter();
		ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
		ImGui::SetNextWindowSize(ImVec2(560, 620), ImGuiCond_Appearing);

		ImGuiWindowFlags flags =
			ImGuiWindowFlags_NoCollapse |
			ImGuiWindowFlags_NoDocking;

		if (ImGui::Begin("エンジン/ゲーム構成設定", &show_, flags))
		{
			if (ImGui::BeginTabBar("##ConfigTabs"))
			{
				if (ImGui::BeginTabItem("エンジン設定"))
				{
					if (DrawEditorConfigTab())
					{
						editorConfig_.Save();
					}
					ImGui::EndTabItem();
				}

				if (ImGui::BeginTabItem("ゲーム設定"))
				{
					if (DrawGameConfigTab())
					{
						gameConfig_.Save();
					}
					ImGui::EndTabItem();
				}

				if (ImGui::BeginTabItem("入力設定"))
				{
					DrawInputBindingTab();
					ImGui::EndTabItem();
				}

				ImGui::EndTabBar();
			}
		}
		ImGui::End();
	}
}
