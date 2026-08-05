#include <Editor/Editor/Panel/ProfilerPanel.h>
#include <Editor/Editor/EditorContext.h>
#include <GraphicsEngine/Profiler/ProfilerStats.h>
#include <Psapi.h>

namespace SeedCore
{
	ProfilerPanel::ProfilerPanel(EditorContext& context) : context_(context)
	{
		/// No Code
	}

	void ProfilerPanel::Draw(const GpuProfiler& gpuProfiler)
	{
		ImGuiIO& io = ImGui::GetIO();

		fpsHistory_[historyOffset_] = io.Framerate;
		frameTimeHistory_[historyOffset_] = 1000.0f / io.Framerate;
		historyOffset_ = (historyOffset_ + 1) % historySize_;

		refreshTimer_ += io.DeltaTime;
		if (refreshTimer_ >= refreshInterval_)
		{
			displayFPS_ = io.Framerate;
			displayFrameTime_ = 1000.0f / io.Framerate;
			refreshTimer_ = 0.0f;
		}

		Float availWidth = ImGui::GetContentRegionAvail().x;
		Float graphWidth = (availWidth - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
		Float graphHeight = 80.0f;

		ImGui::SeparatorText("フレーム");

		Char fpsOverlay[32];
		snprintf(fpsOverlay, sizeof(fpsOverlay), "%.1f FPS", displayFPS_);

		ImGui::PlotLines("##FPS", fpsHistory_, historySize_, historyOffset_, fpsOverlay, 0.0f, 200.0f, ImVec2(graphWidth, graphHeight));

		ImGui::SameLine();

		Char ftOverlay[32];
		snprintf(ftOverlay, sizeof(ftOverlay), "%.2f ms", displayFrameTime_);

		ImGui::PlotLines("##FrameTime", frameTimeHistory_, historySize_, historyOffset_, ftOverlay, 0.0f, 50.0f, ImVec2(graphWidth, graphHeight));

		ImGui::SeparatorText("メモリ");

		PROCESS_MEMORY_COUNTERS_EX pmc{};
		pmc.cb = sizeof(pmc);
		if (GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc), sizeof(pmc)))
		{
			MEMORYSTATUSEX memStatus{};
			memStatus.dwLength = sizeof(memStatus);
			GlobalMemoryStatusEx(&memStatus);

			Float workingSetMB = static_cast<Float>(pmc.WorkingSetSize) / (1024.0f * 1024.0f);
			Float totalPhysicalMB = static_cast<Float>(memStatus.ullTotalPhys) / (1024.0f * 1024.0f);
			Float cpuRatio = (totalPhysicalMB > 0.0f) ? (workingSetMB / totalPhysicalMB) : 0.0f;

			Char cpuOverlay[64];
			snprintf(cpuOverlay, sizeof(cpuOverlay), "CPU: %.1f / %.0f MB", workingSetMB, totalPhysicalMB);

			ImGui::ProgressBar(cpuRatio, ImVec2(-1.0f, 0.0f), cpuOverlay);
		}

		if (context_.graphicsContext_.adapter_)
		{
			DXGI_QUERY_VIDEO_MEMORY_INFO localInfo{};
			context_.graphicsContext_.adapter_->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &localInfo);

			Float usedMB = static_cast<Float>(localInfo.CurrentUsage) / (1024.0f * 1024.0f);
			Float budgetMB = static_cast<Float>(localInfo.Budget) / (1024.0f * 1024.0f);
			Float ratio = (budgetMB > 0.0f) ? (usedMB / budgetMB) : 0.0f;

			Char vramOverlay[64];
			snprintf(vramOverlay, sizeof(vramOverlay), "VRAM: %.1f / %.1f MB", usedMB, budgetMB);

			ImGui::ProgressBar(ratio, ImVec2(-1.0f, 0.0f), vramOverlay);
		}

		ImGui::SeparatorText("レンダリング");

		if (ImGui::BeginTable("##RenderStats", 2))
		{
			ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 120.0f);
			ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::TextDisabled("ドローコール");
			ImGui::TableNextColumn();
			ImGui::Text("%u", ProfilerStats::drawCallCount_);

			ImGui::EndTable();
		}

		DrawGpuTable(gpuProfiler);
	}

	/**
	* [EN]
	* Per-pass GPU times, one column per view. The editor and the game view run
	* the same pass list against different cameras, so seeing them side by side
	* is what makes "the frame rate collapses once the game view is open"
	* attributable to a specific pass instead of guesswork.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* パス別 GPU 時間。ビューごとに列を分ける。エディタとゲームビューは同じパス列を
	* 別カメラで回すので、両者を並べて見られることが「ゲームビューを開くと FPS が
	* 落ちる」を特定のパスに帰着させる決め手になる(推測でなくなる)。
	*/
	void ProfilerPanel::DrawGpuTable(const GpuProfiler& gpuProfiler)
	{
		ImGui::SeparatorText("GPU（パス別）");

		if (!gpuProfiler.IsAvailable())
		{
			ImGui::TextDisabled("GPU タイムスタンプが利用できません");
			return;
		}

		/// [JP] 値は数フレーム遅れて返ってくるので、そのまま毎フレーム表示すると
		///      読めないほどちらつく。FPS 表示と同じ refreshInterval_ で間引く。
		gpuRefreshTimer_ += ImGui::GetIO().DeltaTime;
		Bool refresh = gpuRefreshTimer_ >= refreshInterval_;
		if (refresh)
		{
			gpuRefreshTimer_ = 0.0f;
		}

		const Uint32 viewCount = static_cast<Uint32>(GpuProfileView::Count);
		const Uint32 scopeCount = static_cast<Uint32>(GpuProfileScope::Count);

		if (refresh)
		{
			for (Uint32 viewIndex = 0; viewIndex < viewCount; viewIndex++)
			{
				for (Uint32 scopeIndex = 0; scopeIndex < scopeCount; scopeIndex++)
				{
					gpuDisplay_[viewIndex][scopeIndex] = gpuProfiler.GetMilliseconds(static_cast<GpuProfileView>(viewIndex), static_cast<GpuProfileScope>(scopeIndex));
				}
			}
		}

		if (ImGui::BeginTable("##GpuStats", 1 + static_cast<Int>(viewCount), ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit))
		{
			ImGui::TableSetupColumn("パス", ImGuiTableColumnFlags_WidthStretch);
			for (Uint32 viewIndex = 0; viewIndex < viewCount; viewIndex++)
			{
				ImGui::TableSetupColumn(GpuProfiler::ViewName(static_cast<GpuProfileView>(viewIndex)), ImGuiTableColumnFlags_WidthFixed, 78.0f);
			}
			ImGui::TableHeadersRow();

			Float totals[static_cast<Uint32>(GpuProfileView::Count)] = {};

			for (Uint32 scopeIndex = 0; scopeIndex < scopeCount; scopeIndex++)
			{
				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::TextDisabled("%s", GpuProfiler::ScopeName(static_cast<GpuProfileScope>(scopeIndex)));

				for (Uint32 viewIndex = 0; viewIndex < viewCount; viewIndex++)
				{
					ImGui::TableNextColumn();

					Float milliseconds = gpuDisplay_[viewIndex][scopeIndex];
					totals[viewIndex] += milliseconds;

					/// [JP] 走っていないパスは 0 が返るので、0.0 と表示するより
					///      「-」の方が「無効」と「速い」を混同しない。
					if (milliseconds <= 0.0f)
					{
						ImGui::TextDisabled("-");
						continue;
					}

					/// [JP] 重いパスを目で拾えるように色を付ける(1ms/4ms 基準)。
					if (milliseconds >= 4.0f)
					{
						ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.35f, 1.0f), "%.2f ms", milliseconds);
					}
					else if (milliseconds >= 1.0f)
					{
						ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.35f, 1.0f), "%.2f ms", milliseconds);
					}
					else
					{
						ImGui::Text("%.2f ms", milliseconds);
					}
				}
			}

			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text("合計");
			for (Uint32 viewIndex = 0; viewIndex < viewCount; viewIndex++)
			{
				ImGui::TableNextColumn();
				ImGui::Text("%.2f ms", totals[viewIndex]);
			}

			ImGui::EndTable();
		}

		ImGui::TextDisabled("※計測対象パスの合計です（フレーム全体ではありません）");
	}
}
