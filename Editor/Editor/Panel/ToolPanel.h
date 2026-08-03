#pragma once
#include <FoundationEngine/Prelude.h>
#include <Editor/Editor/Panel/ConsolePanel.h>
#include <Editor/Editor/Panel/ProfilerPanel.h>

namespace SeedCore
{
	struct EditorContext;
	class ImGuiTexture;

	enum class ToolTab 
	{
		Console,
		Profiler
	};

	class ToolPanel
	{
	public:
		ToolPanel(EditorContext& context, ImGuiTexture& imguiTexture);
		~ToolPanel() = default;

		/// [EN] gpuProfiler is only forwarded to ProfilerPanel — ToolPanel itself
		///      does not read it.
		/// [JP] gpuProfiler は ProfilerPanel へ渡すだけで、ToolPanel 自身は読まない。
		void Draw(const GpuProfiler& gpuProfiler);

		void ShowConsoleTab();

		void ShowProfilerTab();

	private:
		ToolTab currentTab_ = ToolTab::Console;

		Bool requestChange_ = false;

		ConsolePanel consolePanel_;

		ProfilerPanel profilerPanel_;
	};
}
