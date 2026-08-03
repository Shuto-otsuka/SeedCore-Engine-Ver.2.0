#pragma once
#include <FoundationEngine/Prelude.h>
#include <GraphicsEngine/Profiler/GpuProfiler.h>

namespace SeedCore
{
	struct EditorContext;

	class ProfilerPanel
	{
	public:
		ProfilerPanel(EditorContext& context);
		~ProfilerPanel() = default;

		/// [EN] gpuProfiler comes in as a parameter rather than being reached
		///      through EditorContext, so the panel's dependency on the renderer
		///      stays visible in the signature.
		/// [JP] gpuProfiler は EditorContext 経由ではなく引数で受け取る。
		///      パネルがレンダラーに依存していることをシグネチャに出すため。
		void Draw(const GpuProfiler& gpuProfiler);

	private:
		void DrawGpuTable(const GpuProfiler& gpuProfiler);

		EditorContext& context_;

		/// [EN] Latched copy of the GPU times. The values already lag the current
		///      frame, and refreshing them every frame makes the table unreadable.
		/// [JP] GPU 時間のラッチ済みコピー。値自体が数フレーム遅れて来る上、毎
		///      フレーム更新すると表がちらついて読めないため間引いて保持する。
		Float gpuDisplay_[static_cast<Uint32>(GpuProfileView::Count)][static_cast<Uint32>(GpuProfileScope::Count)] = {};
		Float gpuRefreshTimer_ = 0.0f;

		static constexpr Int historySize_ = 240;
		Float fpsHistory_[historySize_] = {};
		Float frameTimeHistory_[historySize_] = {};
		Int historyOffset_ = 0;

		Float displayFPS_ = 0.0f;
		Float displayFrameTime_ = 0.0f;
		Float refreshTimer_ = 0.0f;
		static constexpr Float refreshInterval_ = 0.5f;
	};
}
