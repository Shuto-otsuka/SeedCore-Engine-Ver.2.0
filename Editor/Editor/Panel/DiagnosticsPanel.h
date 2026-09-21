#pragma once
#include <FoundationEngine/Prelude.h>
#include <Editor/Editor/Panel/ConsolePanel.h>
#include <Editor/Editor/Panel/ProfilerPanel.h>

namespace SeedCore
{
	struct EditorContext;
	class ImGuiTexture;

	enum class DiagnosticsTab 
	{
		Console,
		Profiler
	};

	class DiagnosticsPanel
	{
	public:
		DiagnosticsPanel(EditorContext& context, ImGuiTexture& imguiTexture);
		~DiagnosticsPanel() = default;

		void Draw(const GpuProfiler& gpuProfiler);

		void ShowConsoleTab();

		void ShowProfilerTab();

	private:
		DiagnosticsTab currentTab_ = DiagnosticsTab::Console;

		Bool requestChange_ = false;

		ConsolePanel consolePanel_;

		ProfilerPanel profilerPanel_;
	};
}
