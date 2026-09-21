#include <Editor/Editor/Panel/DiagnosticsPanel.h>

namespace SeedCore
{
	DiagnosticsPanel::DiagnosticsPanel(EditorContext& context, ImGuiTexture& imguiTexture) : consolePanel_(imguiTexture), profilerPanel_(context)
	{
		/// No Code
	}

	void DiagnosticsPanel::Draw(const GpuProfiler& gpuProfiler)
	{
		ImGuiID dockspaceID = ImGui::GetID("ScDockSpace");
		ImGui::SetNextWindowDockID(dockspaceID, ImGuiCond_FirstUseEver);

		if (ImGui::Begin("診断"))
		{
			if (ImGui::BeginTabBar("##DiagnosticsTabs"))
			{
				ImGuiTabItemFlags consoleFlags = (requestChange_ && currentTab_ == DiagnosticsTab::Console) ? ImGuiTabItemFlags_SetSelected : 0;
				ImGuiTabItemFlags profilerFlags = (requestChange_ && currentTab_ == DiagnosticsTab::Profiler) ? ImGuiTabItemFlags_SetSelected : 0;

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

	void DiagnosticsPanel::ShowConsoleTab()
	{
		requestChange_ = true;
		currentTab_ = DiagnosticsTab::Console;
	}

	void DiagnosticsPanel::ShowProfilerTab()
	{
		requestChange_ = true;
		currentTab_ = DiagnosticsTab::Profiler;
	}
}
