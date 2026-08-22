#include <Editor/Editor/Panel/SpecMemoPanel.h>
#include <Editor/Editor/EditorContext.h>

namespace SeedCore
{
	SpecMemoPanel::SpecMemoPanel(EditorContext& context)
	{
		/// No Code
	}

	void SpecMemoPanel::Open()
	{
		show_ = true;
	}

	void SpecMemoPanel::Draw()
	{
		if (!show_)
		{
			return;
		}

		ImVec2 center = ImGui::GetMainViewport()->GetCenter();
		ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
		ImGui::SetNextWindowSize(ImVec2(600, 520), ImGuiCond_Appearing);

		ImGuiWindowFlags flags =
			ImGuiWindowFlags_NoCollapse |
			ImGuiWindowFlags_NoDocking;

		if (ImGui::Begin("Seed Core エンジンの仕様", &show_, flags))
		{
			ImGui::BeginChild("##SpecMemoScroll", ImVec2(0, -ImGui::GetFrameHeightWithSpacing()));

			ImGui::TextDisabled("開発環境");
			ImGui::Spacing();
			ImGui::BulletText("C++23（LanguageStandard: stdcpp23）");
			ImGui::BulletText("コンパイラ：Debug : MSVC（PlatformToolset v145） / Release : Clang（ClangCL）");
			ImGui::BulletText("シェーダー：HLSL Shader Model 6.6（DXC コンパイル）");

			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();

			ImGui::TextDisabled("座標系");
			ImGui::Spacing();
			ImGui::BulletText("左手座標系 / Y-Up / 前方 +Z");
			ImGui::BulletText("深度は Reversed-Z（Near : 1.0, Far : 0.0, DepthFunc : GREATER）");

			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();

			ImGui::TextDisabled("対応アセット形式");
			ImGui::Spacing();
			ImGui::BulletText("モデル：glTF（.gltf / .glb）を読み込み、独自キャッシュ形式（.crister）に変換");
			ImGui::BulletText("テクスチャ：DDS を優先し、非対応時は WIC 経由で PNG / JPG / BMP 等にフォールバック");
			ImGui::BulletText("フォント：TTF / OTF（MTSDF 方式でレンダリング）");
			ImGui::BulletText("動画：Media Foundation（.mp4）");
			ImGui::BulletText("音声：CRI ADX");

			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();

			ImGui::TextDisabled("使用している外部ライブラリ");
			ImGui::Spacing();

			static const LibraryEntry libraries[] = {
				{ "Dear ImGui",       "1.92.9 WIP", "エディター UI" },
				{ "nlohmann/json",    "3.12.0",     "シリアライズ（JSON）" },
				{ "JoltPhysics",      "5.6.0",      "物理演算" },
				{ "NVIDIA Streamline (DLSS)", "2.11.1", "アップスケーリング" },
				{ "CRI ADX",          "バージョン表記なし", "音声再生" },
				{ "TinyglTF",         "バージョン表記なし", "glTF モデル読み込み" },
				{ "DirectXTK12",      "バージョン表記なし", "数学ライブラリ / テクスチャロード" },
			};

			if (ImGui::BeginTable("##Libraries", 3, ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_RowBg))
			{
				ImGui::TableSetupColumn("ライブラリ", ImGuiTableColumnFlags_WidthStretch, 0.4f);
				ImGui::TableSetupColumn("バージョン", ImGuiTableColumnFlags_WidthStretch, 0.3f);
				ImGui::TableSetupColumn("用途", ImGuiTableColumnFlags_WidthStretch, 0.3f);
				ImGui::TableHeadersRow();

				for (const LibraryEntry& entry : libraries)
				{
					ImGui::TableNextRow();
					ImGui::TableNextColumn(); ImGui::Text("%s", entry.name_);
					ImGui::TableNextColumn(); ImGui::Text("%s", entry.version_);
					ImGui::TableNextColumn(); ImGui::Text("%s", entry.usage_);
				}

				ImGui::EndTable();
			}

			ImGui::EndChild();

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
