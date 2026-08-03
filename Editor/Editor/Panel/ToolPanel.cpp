#include <Editor/Editor/Panel/ToolPanel.h>

namespace SeedCore
{
	ToolPanel::ToolPanel(EditorContext& context, ImGuiTexture& imguiTexture) : consolePanel_(imguiTexture), profilerPanel_(context)
	{
		/// No Code
	}

	void ToolPanel::Draw(const GpuProfiler& gpuProfiler)
	{
		ImGuiID dockspaceID = ImGui::GetID("ScDockSpace");
		ImGui::SetNextWindowDockID(dockspaceID, ImGuiCond_FirstUseEver);

		if (ImGui::Begin("ツール"))
		{
			if (ImGui::BeginTabBar("##ToolTabs"))
			{
				ImGuiTabItemFlags consoleFlags = (requestChange_ && currentTab_ == ToolTab::Console) ? ImGuiTabItemFlags_SetSelected : 0;
				ImGuiTabItemFlags profilerFlags = (requestChange_ && currentTab_ == ToolTab::Profiler) ? ImGuiTabItemFlags_SetSelected : 0;

				if (ImGui::BeginTabItem("コンソール###ConsoleTabID", nullptr, consoleFlags))
				{
					requestChange_ = false;
					consolePanel_.Draw();
					ImGui::EndTabItem();
				}

				if (ImGui::BeginTabItem("プロファイラー###ProfilerTabID", nullptr, profilerFlags))
				{
					requestChange_ = false;
					profilerPanel_.Draw(gpuProfiler);
					ImGui::EndTabItem();
				}

				ImGui::EndTabBar();
			}
		}
		ImGui::End();
	}

	void ToolPanel::ShowConsoleTab()
	{
		requestChange_ = true;
		currentTab_ = ToolTab::Console;
	}

	void ToolPanel::ShowProfilerTab()
	{
		requestChange_ = true;
		currentTab_ = ToolTab::Profiler;
	}
}
