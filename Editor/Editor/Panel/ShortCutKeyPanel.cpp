#include <Editor/Editor/Panel/ShortCutKeyPanel.h>
#include <Editor/Editor/EditorContext.h>

namespace SeedCore
{
	namespace
	{
		struct ShortCutEntry
		{
			const Char* key_;
			const Char* description_;
		};

		void DrawShortCutTable(const Char* tableId, const ShortCutEntry* entries, Size count)
		{
			if (ImGui::BeginTable(tableId, 2, ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
			{
				ImGui::TableSetupColumn("キー", ImGuiTableColumnFlags_WidthStretch, 0.42f);
				ImGui::TableSetupColumn("動作", ImGuiTableColumnFlags_WidthStretch, 0.58f);
				ImGui::TableHeadersRow();

				for (Size entryIndex = 0; entryIndex < count; ++entryIndex)
				{
					ImGui::TableNextRow();
					ImGui::TableNextColumn(); ImGui::TextWrapped("%s", entries[entryIndex].key_);
					ImGui::TableNextColumn(); ImGui::TextWrapped("%s", entries[entryIndex].description_);
				}

				ImGui::EndTable();
			}
		}
	}

	ShortCutKeyPanel::ShortCutKeyPanel(EditorContext& context)
	{
		/// No Code
	}

	void ShortCutKeyPanel::Open()
	{
		show_ = true;
	}

	void ShortCutKeyPanel::Draw()
	{
		if (!show_)
		{
			return;
		}

		ImVec2 center = ImGui::GetMainViewport()->GetCenter();
		ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
		ImGui::SetNextWindowSize(ImVec2(680, 620), ImGuiCond_Appearing);

		ImGuiWindowFlags flags =
			ImGuiWindowFlags_NoCollapse |
			ImGuiWindowFlags_NoDocking;

		if (ImGui::Begin("ショートカットキー一覧", &show_, flags))
		{
			ImGui::TextDisabled("ファイル");
			ImGui::Spacing();

			static const ShortCutEntry fileEntries[] =
			{
				{ "Ctrl + N", "新規シーンを作成" },
				{ "Ctrl + O", "シーンを開く" },
				{ "Ctrl + Shift + S", "名前を付けてシーンを保存" },
				{ "Ctrl + S", "シーンを上書き保存" },
			};
			DrawShortCutTable("##FileShortCuts", fileEntries, std::size(fileEntries));

			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();

			ImGui::TextDisabled("ヒエラルキー");
			ImGui::Spacing();

			static const ShortCutEntry hierarchyEntries[] = 
			{
				{ "クリック", "単一選択" },
				{ "Ctrl + クリック", "個別選択の追加 / 解除" },
				{ "Shift + クリック", "範囲選択" },
				{ "ドラッグ（空白部分から）", "範囲選択（マーキー）" },
				{ "Actor を別の Actor へドラッグ&ドロップ", "親子付け替え" },
				{ "Actor をコンテンツドロワーへドラッグ&ドロップ", "Prefab として保存" },
				{ "Ctrl + D", "選択中の Actor を複製" },
				{ "Delete", "選択中の Actor を削除" },
			};
			DrawShortCutTable("##HierarchyShortCuts", hierarchyEntries, std::size(hierarchyEntries));

			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();

			ImGui::TextDisabled("コンテンツドロワー");
			ImGui::Spacing();

			static const ShortCutEntry contentsDrawerEntries[] =
			{
				{ "ダブルクリック", "アセットを外部アプリで開く" },
				{ "右クリック（アセット / フォルダ / 背景）", "コンテキストメニュー（切り取り・コピー・貼り付け・名前変更・削除 等）" },
				{ "マウス戻る / 進むボタン", "フォルダ移動履歴を前後に移動" },
				{ "アセットをヒエラルキーへドラッグ&ドロップ", "アセットをシーンに配置 / Prefab をインスタンス化" },
				{ "Actor をコンテンツドロワーへドラッグ&ドロップ", "Prefab として保存" },
			};
			DrawShortCutTable("##ContentsDrawerShortCuts", contentsDrawerEntries, std::size(contentsDrawerEntries));

			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();

			ImGui::TextDisabled("エディターカメラ（右クリック押下中）");
			ImGui::Spacing();

			static const ShortCutEntry cameraEntries[] = 
			{
				{ "右クリック（ドラッグ）", "視点の回転" },
				{ "W / A / S / D", "前後左右への移動" },
				{ "E / Q", "上 / 下への移動" },
				{ "マウスホイール", "視線方向への前後移動（ズーム）" },
			};
			DrawShortCutTable("##CameraShortCuts", cameraEntries, std::size(cameraEntries));

			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();

			ImGui::TextDisabled("ギズモ操作");
			ImGui::Spacing();

			static const ShortCutEntry gizmoEntries[] =
			{
				{ "Ctrl + Q", "ギズモを非表示（選択モード）" },
				{ "Ctrl + W", "移動（Translate）" },
				{ "Ctrl + E", "回転（Rotate）" },
				{ "Ctrl + R", "拡大縮小（Scale）" },
			};
			DrawShortCutTable("##GizmoShortCuts", gizmoEntries, std::size(gizmoEntries));

			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();

			ImGui::TextDisabled("再生制御");
			ImGui::Spacing();

			static const ShortCutEntry playEntries[] = 
			{
				{ "F5", "再生" },
				{ "F6", "一時停止 / 再開" },
				{ "F7", "停止" },
			};
			DrawShortCutTable("##PlayShortCuts", playEntries, std::size(playEntries));

			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();

			ImGui::TextDisabled("ウィンドウ / その他");
			ImGui::Spacing();

			static const ShortCutEntry miscEntries[] =
			{
				{ "Ctrl + F1", "ショートカットキー一覧を開く" },
				{ "Ctrl + F2", "仕様メモを開く" },
				{ "Ctrl + F3", "コンソールを開く" },
				{ "Ctrl + F4", "パフォーマンスプロファイラーを開く" },
				{ "Ctrl + F8", "ToDoリストを開く" },
				{ "Ctrl + F9", "バージョン情報を開く" },
				{ "Ctrl + F11", "ゲームウィンドウの全画面表示切り替え" },
				{ "Alt + F4", "エディターの終了（未保存の変更があれば確認）" },
			};
			DrawShortCutTable("##MiscShortCuts", miscEntries, std::size(miscEntries));

			ImGui::Spacing();

			Float buttonWidth = 120.0f;
			ImGui::SetCursorPosX(ImGui::GetContentRegionAvail().x - buttonWidth + ImGui::GetCursorPosX());
			if (ImGui::Button("閉じる", ImVec2(buttonWidth, 0)))
			{
				show_ = false;
			}
		}
		ImGui::End();
	}
}
