#include <Editor/Editor/Panel/GraphicsMenuPanel.h>
#include <Editor/Editor/EditorContext.h>

namespace SeedCore
{
	GraphicsMenuPanel::GraphicsMenuPanel(EditorContext& context) : context_(context), raytracingPanel_(context), environmentMenuPanel_(context)
	{
		/// No Code
	}

	void GraphicsMenuPanel::Draw()
	{
		if (ImGui::BeginMenu("グラフィックス"))
		{
			if (ImGui::BeginMenu("ビューモード"))
			{
				auto item = [&](const Char* label, ViewMode mode)
				{
					if (ImGui::MenuItem(label, nullptr, context_.viewportContext_.viewMode_ == mode))
					{
						context_.viewportContext_.viewMode_ = mode;
					}
				};

				item("ライティングあり", ViewMode::Lit);
				item("ライティングなし", ViewMode::Unlit);
				item("ワイヤーフレーム", ViewMode::Wireframe);
				item("深度", ViewMode::Depth);
				item("メッシュレット", ViewMode::Meshlet);

				if (ImGui::BeginMenu("バッファービジュアライゼーション"))
				{
					item("法線", ViewMode::Normal);
					item("ラフネス", ViewMode::Roughness);
					item("メタルネス", ViewMode::Metallic);
					item("エミッシブ", ViewMode::Emissive);
					item("モーションベクター", ViewMode::Velocity);
					ImGui::EndMenu();
				}

				if (ImGui::BeginMenu("レイトレーシング"))
				{
					item("反射（生）", ViewMode::ReflectionRaw);
					item("反射（デノイズ後）", ViewMode::ReflectionDenoised);
					ImGui::Separator();
					item("グローバルイルミネーション（生）", ViewMode::GlobalIlluminationRaw);
					item("グローバルイルミネーション（デノイズ後）", ViewMode::GlobalIlluminationDenoised);
					ImGui::Separator();
					item("シャドウ（生）", ViewMode::ShadowRaw);
					item("シャドウ（デノイズ後）", ViewMode::ShadowDenoised);
					ImGui::Separator();
					item("アンビエントオクルージョン（生）", ViewMode::AmbientOcclusionRaw);
					item("アンビエントオクルージョン（デノイズ後）", ViewMode::AmbientOcclusionDenoised);
					ImGui::EndMenu();
				}

				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("レイトレーシング"))
			{
				raytracingPanel_.DrawMenuItems();

				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("環境"))
			{
				environmentMenuPanel_.DrawMenuItems();

				ImGui::EndMenu();
			}

			ImGui::EndMenu();
		}

		raytracingPanel_.DrawWindows();
		environmentMenuPanel_.DrawWindows();
	}
}
