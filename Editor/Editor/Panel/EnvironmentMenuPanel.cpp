#include <Editor/Editor/Panel/EnvironmentMenuPanel.h>
#include <Editor/Editor/EditorContext.h>

namespace SeedCore
{
	EnvironmentMenuPanel::EnvironmentMenuPanel(EditorContext& context) : context_(context)
	{
		/// No Code
	}

	void EnvironmentMenuPanel::DrawMenuItems()
	{
		if (ImGui::MenuItem("時刻(DaySystem)..."))
		{
			showDaySystemSettings_ = true;
		}

		if (ImGui::MenuItem("太陽(SunLight)..."))
		{
			showSunLightSettings_ = true;
		}

		if (ImGui::MenuItem("月(MoonLight)..."))
		{
			showMoonLightSettings_ = true;
		}

		if (ImGui::MenuItem("雨(Rain)..."))
		{
			showRainSettings_ = true;
		}

		if (ImGui::MenuItem("雪(Snow)..."))
		{
			showSnowSettings_ = true;
		}
	}

	void EnvironmentMenuPanel::DrawWindows()
	{
		DrawDaySystemSettingsWindow();
		DrawSunLightSettingsWindow();
		DrawMoonLightSettingsWindow();
		DrawRainSettingsWindow();
		DrawSnowSettingsWindow();
	}

	void EnvironmentMenuPanel::DrawDaySystemSettingsWindow()
	{
		if (!showDaySystemSettings_)
		{
			return;
		}

		ImVec2 center = ImGui::GetMainViewport()->GetCenter();
		ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
		ImGui::SetNextWindowSize(ImVec2(420, 0), ImGuiCond_Appearing);

		ImGuiWindowFlags flags =
			ImGuiWindowFlags_NoCollapse |
			ImGuiWindowFlags_NoDocking |
			ImGuiWindowFlags_AlwaysAutoResize;

		if (ImGui::Begin("環境：時刻(DaySystem)", &showDaySystemSettings_, flags))
		{
			ImGui::Checkbox("有効", &context_.viewportContext_.raytracing_.daySystemEnabled_);
			ImGui::TextDisabled("※有効時、シーンの DirectionalLight は無視され、太陽/月はここの設定と時刻から計算されます");

			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();

			ImGui::BeginDisabled(!context_.viewportContext_.raytracing_.daySystemEnabled_);

			ImGui::Checkbox("一時停止", &context_.viewportContext_.raytracing_.daySystem_.paused_);
			ImGui::SliderFloat("時刻(時)", &context_.viewportContext_.raytracing_.daySystem_.hourOfDay_, 0.0f, 24.0f, "%.2f");

			Int dayOfMonth = static_cast<Int>(context_.viewportContext_.raytracing_.daySystem_.dayOfMonth_);
			Int daysPerMonth = static_cast<Int>(context_.viewportContext_.raytracing_.daySystem_.daysPerMonth_);
			if (ImGui::SliderInt("月内の日", &dayOfMonth, 1, Max(daysPerMonth, 1)))
			{
				context_.viewportContext_.raytracing_.daySystem_.dayOfMonth_ = static_cast<Uint32>(dayOfMonth);
			}
			if (ImGui::SliderInt("1か月の日数", &daysPerMonth, 1, 365))
			{
				context_.viewportContext_.raytracing_.daySystem_.daysPerMonth_ = static_cast<Uint32>(daysPerMonth);
			}

			ImGui::SliderFloat("1日の実時間(分)", &context_.viewportContext_.raytracing_.daySystem_.dayLengthMinutes_, 0.1f, 1440.0f, "%.1f", ImGuiSliderFlags_Logarithmic);
			ImGui::SliderFloat("時間倍率", &context_.viewportContext_.raytracing_.daySystem_.timeScale_, 0.0f, 100.0f, "%.2f", ImGuiSliderFlags_Logarithmic);

			ImGui::EndDisabled();

			ImGui::Spacing();

			Float buttonWidth = 120.0f;
			ImGui::SetCursorPosX(ImGui::GetContentRegionAvail().x - buttonWidth + ImGui::GetCursorPosX());
			if (ImGui::Button("閉じる", ImVec2(buttonWidth, 0)))
			{
				showDaySystemSettings_ = false;
			}
		}
		ImGui::End();
	}

	void EnvironmentMenuPanel::DrawSunLightSettingsWindow()
	{
		if (!showSunLightSettings_)
		{
			return;
		}

		ImVec2 center = ImGui::GetMainViewport()->GetCenter();
		ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
		ImGui::SetNextWindowSize(ImVec2(420, 0), ImGuiCond_Appearing);

		ImGuiWindowFlags flags =
			ImGuiWindowFlags_NoCollapse |
			ImGuiWindowFlags_NoDocking |
			ImGuiWindowFlags_AlwaysAutoResize;

		if (ImGui::Begin("環境：太陽(SunLight)", &showSunLightSettings_, flags))
		{
			ImGui::Checkbox("有効", &context_.viewportContext_.raytracing_.sunLightEnabled_);
			ImGui::TextDisabled("※DaySystem も有効な場合のみ、シーンの太陽に反映されます");

			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();

			ImGui::BeginDisabled(!context_.viewportContext_.raytracing_.sunLightEnabled_);

			ImGui::ColorEdit3("地平線の色", context_.viewportContext_.raytracing_.sunLight_.horizonColor_);
			ImGui::ColorEdit3("天頂の色", context_.viewportContext_.raytracing_.sunLight_.zenithColor_);
			ImGui::SliderFloat("最大強度", &context_.viewportContext_.raytracing_.sunLight_.maxIntensity_, 0.0f, 20.0f, "%.2f");
			ImGui::SliderFloat("消灯する仰角(度)", &context_.viewportContext_.raytracing_.sunLight_.minElevationDegrees_, -20.0f, 10.0f, "%.1f");
			ImGui::SliderFloat("視半径", &context_.viewportContext_.raytracing_.sunLight_.angularRadius_, 0.005f, 0.2f, "%.3f");

			ImGui::EndDisabled();

			ImGui::Spacing();

			Float buttonWidth = 120.0f;
			ImGui::SetCursorPosX(ImGui::GetContentRegionAvail().x - buttonWidth + ImGui::GetCursorPosX());
			if (ImGui::Button("閉じる", ImVec2(buttonWidth, 0)))
			{
				showSunLightSettings_ = false;
			}
		}
		ImGui::End();
	}

	void EnvironmentMenuPanel::DrawMoonLightSettingsWindow()
	{
		if (!showMoonLightSettings_)
		{
			return;
		}

		ImVec2 center = ImGui::GetMainViewport()->GetCenter();
		ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
		ImGui::SetNextWindowSize(ImVec2(420, 0), ImGuiCond_Appearing);

		ImGuiWindowFlags flags =
			ImGuiWindowFlags_NoCollapse |
			ImGuiWindowFlags_NoDocking |
			ImGuiWindowFlags_AlwaysAutoResize;

		if (ImGui::Begin("環境：月(MoonLight)", &showMoonLightSettings_, flags))
		{
			ImGui::Checkbox("有効", &context_.viewportContext_.raytracing_.moonLightEnabled_);
			ImGui::TextDisabled("※DaySystem も有効な場合のみ、シーンの月に反映されます");

			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();

			ImGui::BeginDisabled(!context_.viewportContext_.raytracing_.moonLightEnabled_);

			ImGui::ColorEdit3("色", context_.viewportContext_.raytracing_.moonLight_.color_);
			ImGui::SliderFloat("最大強度", &context_.viewportContext_.raytracing_.moonLight_.maxIntensity_, 0.0f, 2.0f, "%.3f");
			ImGui::SliderFloat("視半径", &context_.viewportContext_.raytracing_.moonLight_.angularRadius_, 0.005f, 0.2f, "%.3f");

			ImGui::EndDisabled();

			ImGui::Spacing();

			Float buttonWidth = 120.0f;
			ImGui::SetCursorPosX(ImGui::GetContentRegionAvail().x - buttonWidth + ImGui::GetCursorPosX());
			if (ImGui::Button("閉じる", ImVec2(buttonWidth, 0)))
			{
				showMoonLightSettings_ = false;
			}
		}
		ImGui::End();
	}

	void EnvironmentMenuPanel::DrawRainSettingsWindow()
	{
		if (!showRainSettings_)
		{
			return;
		}

		ImVec2 center = ImGui::GetMainViewport()->GetCenter();
		ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
		ImGui::SetNextWindowSize(ImVec2(420, 0), ImGuiCond_Appearing);

		ImGuiWindowFlags flags =
			ImGuiWindowFlags_NoCollapse |
			ImGuiWindowFlags_NoDocking |
			ImGuiWindowFlags_AlwaysAutoResize;

		if (ImGui::Begin("環境：雨(Rain)", &showRainSettings_, flags))
		{
			ImGui::Checkbox("有効", &context_.viewportContext_.raytracing_.rainEnabled_);
			ImGui::TextDisabled("※実際に見える量は Weather コンポーネントの現在の雨量で自動的にスケールされます");

			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();

			ImGui::BeginDisabled(!context_.viewportContext_.raytracing_.rainEnabled_);

			ImGui::SliderFloat("密度", &context_.viewportContext_.raytracing_.rain_.density_, 0.0f, 1.0f, "%.2f");
			ImGui::SliderFloat("落下速度", &context_.viewportContext_.raytracing_.rain_.fallSpeed_, 1.0f, 30.0f, "%.1f");
			ImGui::SliderFloat("サイズ", &context_.viewportContext_.raytracing_.rain_.size_, 0.005f, 0.2f, "%.3f", ImGuiSliderFlags_Logarithmic);
			ImGui::SliderFloat("筋の長さ", &context_.viewportContext_.raytracing_.rain_.streakLength_, 0.0f, 2.0f, "%.2f");
			ImGui::SliderFloat("明るさ", &context_.viewportContext_.raytracing_.rain_.brightness_, 0.0f, 3.0f, "%.2f");
			ImGui::ColorEdit3("色", context_.viewportContext_.raytracing_.rain_.color_);
			ImGui::SliderFloat("スポーン範囲(半径)", &context_.viewportContext_.raytracing_.rain_.volumeRadius_, 2.0f, 60.0f, "%.1f");
			ImGui::SliderFloat("スポーン範囲(高さ)", &context_.viewportContext_.raytracing_.rain_.volumeHeight_, 2.0f, 40.0f, "%.1f");

			ImGui::EndDisabled();

			ImGui::Spacing();

			Float buttonWidth = 120.0f;
			ImGui::SetCursorPosX(ImGui::GetContentRegionAvail().x - buttonWidth + ImGui::GetCursorPosX());
			if (ImGui::Button("閉じる", ImVec2(buttonWidth, 0)))
			{
				showRainSettings_ = false;
			}
		}
		ImGui::End();
	}

	void EnvironmentMenuPanel::DrawSnowSettingsWindow()
	{
		if (!showSnowSettings_)
		{
			return;
		}

		ImVec2 center = ImGui::GetMainViewport()->GetCenter();
		ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
		ImGui::SetNextWindowSize(ImVec2(420, 0), ImGuiCond_Appearing);

		ImGuiWindowFlags flags =
			ImGuiWindowFlags_NoCollapse |
			ImGuiWindowFlags_NoDocking |
			ImGuiWindowFlags_AlwaysAutoResize;

		if (ImGui::Begin("環境：雪(Snow)", &showSnowSettings_, flags))
		{
			ImGui::Checkbox("有効", &context_.viewportContext_.raytracing_.snowEnabled_);
			ImGui::TextDisabled("※実際に見える量は Weather コンポーネントの現在の降雪量で自動的にスケールされます");

			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();

			ImGui::BeginDisabled(!context_.viewportContext_.raytracing_.snowEnabled_);

			ImGui::SliderFloat("密度", &context_.viewportContext_.raytracing_.snow_.density_, 0.0f, 1.0f, "%.2f");
			ImGui::SliderFloat("落下速度", &context_.viewportContext_.raytracing_.snow_.fallSpeed_, 0.1f, 10.0f, "%.2f");
			ImGui::SliderFloat("サイズ", &context_.viewportContext_.raytracing_.snow_.size_, 0.005f, 0.3f, "%.3f", ImGuiSliderFlags_Logarithmic);
			ImGui::SliderFloat("揺れ幅", &context_.viewportContext_.raytracing_.snow_.swayAmount_, 0.0f, 2.0f, "%.2f");
			ImGui::SliderFloat("明るさ", &context_.viewportContext_.raytracing_.snow_.brightness_, 0.0f, 3.0f, "%.2f");
			ImGui::ColorEdit3("色", context_.viewportContext_.raytracing_.snow_.color_);
			ImGui::SliderFloat("スポーン範囲(半径)", &context_.viewportContext_.raytracing_.snow_.volumeRadius_, 2.0f, 60.0f, "%.1f");
			ImGui::SliderFloat("スポーン範囲(高さ)", &context_.viewportContext_.raytracing_.snow_.volumeHeight_, 2.0f, 40.0f, "%.1f");

			ImGui::EndDisabled();

			ImGui::Spacing();

			Float buttonWidth = 120.0f;
			ImGui::SetCursorPosX(ImGui::GetContentRegionAvail().x - buttonWidth + ImGui::GetCursorPosX());
			if (ImGui::Button("閉じる", ImVec2(buttonWidth, 0)))
			{
				showSnowSettings_ = false;
			}
		}
		ImGui::End();
	}
}
