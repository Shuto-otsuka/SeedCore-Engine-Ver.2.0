#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	class D3D12CommandList;

	/// [EN] Which render pass ran the scope. The editor and the game view run
	///      the same pass list against different cameras, so they need separate
	///      slots — telling them apart is the whole point of this profiler.
	/// [JP] どのレンダーパスでスコープが走ったか。エディタとゲームビューは同じ
	///      パス列を別カメラで回すので別スロットが必要 — 両者を区別することが
	///      このプロファイラの主目的。
	enum class GpuProfileView : Uint32
	{
		Editor,
		Game,
		Canvas,
		Count
	};

	/// [EN] One entry per timed pass. Adding one is just a new enum value plus a
	///      name in GpuProfiler::ScopeName.
	/// [JP] 計測するパス1つにつき1エントリ。追加は enum 値と
	///      GpuProfiler::ScopeName の名前を足すだけ。
	enum class GpuProfileScope : Uint32
	{
		DepthPrepass,
		HiZBuild,
		GeometryBuffer,
		MaterialResolve,
		LightCluster,
		RaytraceShadow,
		RaytraceAmbientOcclusion,
		RaytraceSubsurfaceScattering,
		RaytraceReflection,
		RaytraceGlobalIllumination,
		VolumetricCloudScapes,
		VolumetricStar,
		WeatherParticle,
		VolumetricLight,
		SkyGenerate,
		Composite,
		Transparent,
		DlssRayReconstruction,
		PostProcess,
		Count
	};

	/**
	* [EN]
	* Per-pass GPU timing via D3D12 timestamp queries. Wrap a pass in
	* Begin/End and its wall-clock GPU cost shows up in GetMilliseconds a couple
	* of frames later.
	*
	* Two things about the layout are deliberate:
	*
	* The QUERY HEAP holds only one frame's worth of slots. Advance() records the
	* resolve for the frame that just finished at the head of the NEXT frame's
	* command list, which in GPU execution order lands after that frame's
	* EndQuery calls and before the new frame's — so reusing the slots every
	* frame is safe.
	*
	* The READBACK BUFFER, by contrast, is multi-buffered by frame count. The CPU
	* reads it every frame while the GPU may still be copying into it, so a
	* single region would be read while being written. Reads target the oldest
	* region, which is frameCount-1 frames old and therefore certainly complete.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* D3D12 タイムスタンプクエリによるパス別 GPU 計測。パスを Begin/End で囲むと、
	* 数フレーム後に GetMilliseconds でその GPU 実時間が読める。
	*
	* レイアウトで意図的な点が2つある:
	*
	* 【クエリヒープ】は1フレーム分のスロットしか持たない。Advance() は直前の
	* フレームの resolve を「次のフレームのコマンドリストの先頭」に記録するので、
	* GPU の実行順では そのフレームの EndQuery 群の後・新しいフレームの EndQuery
	* 群の前 に入る。よって毎フレーム同じスロットを使い回して安全。
	*
	* 対して【リードバックバッファ】はフレーム数ぶん多重化する。CPU が毎フレーム
	* 読む一方で GPU がまだコピー中の可能性があるため、1枚だと書き込み中の領域を
	* 読むことになる。読み出しは最も古い領域を対象にし、そこは frameCount-1
	* フレーム前=確実に完了済み。
	*
	* SEEDCORE_API が必要なのは、Editor が SeedCore.dll 越しにこのクラスを直接
	* 呼ぶため(ProfilerPanel)。Renderer に付いていないのは Editor が Renderer を
	* 直接触らないから — 付ける基準は「DLL の外から呼ぶかどうか」。
	*/
	class SEEDCORE_API GpuProfiler
	{
	public:
		GpuProfiler() = default;
		~GpuProfiler() = default;

		/// [EN] frameCount is raised to at least 3 regardless of the swapchain's
		///      buffer count, so a readback is always at least two frames behind
		///      the GPU rather than racing it on a 2-buffer setup.
		/// [JP] frameCount はスワップチェインのバッファ数に関わらず最低 3 へ
		///      引き上げる。2枚構成でも読み出しが GPU と競合せず、常に2フレーム
		///      以上遅れた領域を読むようにするため。
		void Create(ID3D12Device* device, ID3D12CommandQueue* commandQueue, Uint32 frameCount);

		/// [EN] Call once per frame, at the top of the frame's command list.
		///      Resolves the previous frame's timestamps, reads back the oldest
		///      completed frame, and rotates the ring.
		/// [JP] 毎フレーム1回、そのフレームのコマンドリストの先頭で呼ぶ。前
		///      フレームのタイムスタンプを resolve し、完了済みの最古フレームを
		///      読み戻し、リングを回す。
		void Advance(D3D12CommandList* cmdList);

		void Begin(D3D12CommandList* cmdList, GpuProfileView view, GpuProfileScope scope);

		void End(D3D12CommandList* cmdList, GpuProfileView view, GpuProfileScope scope);

		/// [EN] Milliseconds for the scope, or 0 when it did not run in the frame
		///      that was read back (a disabled pass records nothing, and its
		///      stale slots must not be reported as a time).
		/// [JP] スコープのミリ秒。読み戻したフレームで走っていなければ 0
		///      (無効なパスは何も記録しないので、古いスロットの残骸を時間として
		///      報告してはいけない)。
		[[nodiscard]] Float GetMilliseconds(GpuProfileView view, GpuProfileScope scope)const;

		/// [EN] Sum of every scope recorded for the view. Not the view's total
		///      frame time — only the passes that are actually wrapped.
		/// [JP] そのビューで記録された全スコープの合計。ビューのフレーム時間
		///      そのものではなく、Begin/End で囲んだパスの合計。
		[[nodiscard]] Float GetViewMilliseconds(GpuProfileView view)const;

		[[nodiscard]] Bool IsAvailable()const;

		[[nodiscard]] static const Char* ScopeName(GpuProfileScope scope);

		[[nodiscard]] static const Char* ViewName(GpuProfileView view);

	private:
		static constexpr Uint32 viewCount_ = static_cast<Uint32>(GpuProfileView::Count);
		static constexpr Uint32 scopeCount_ = static_cast<Uint32>(GpuProfileScope::Count);

		/// [EN] Two timestamps (begin/end) per view-scope pair.
		/// [JP] ビュー×スコープ 1組につきタイムスタンプ2つ(開始/終了)。
		static constexpr Uint32 slotCount_ = viewCount_ * scopeCount_ * 2;

		static constexpr Uint32 maxFrameCount_ = 4;

		/// [EN] recorded_ is a bitmask over scopes, so the scope count must fit
		///      in a Uint32.
		/// [JP] recorded_ はスコープのビットマスクなので、スコープ数が Uint32 に
		///      収まる必要がある。
		static_assert(scopeCount_ <= 32, "GpuProfileScope が 32 を超えたので recorded_ のビット幅を広げてください");

		[[nodiscard]] Uint32 BeginSlot(GpuProfileView view, GpuProfileScope scope)const;

		Microsoft::WRL::ComPtr<ID3D12QueryHeap> queryHeap_;
		Microsoft::WRL::ComPtr<ID3D12Resource> readbackResource_;

		Uint64 timestampFrequency_ = 0;
		Uint32 frameCount_ = 0;

		/// [EN] Readback region currently being written by this frame's resolve.
		/// [JP] 今フレームの resolve が書き込むリードバック領域。
		Uint32 writeFrame_ = 0;

		/// [EN] Advance() count — the first call has nothing to resolve yet.
		/// [JP] Advance() の回数。初回はまだ resolve するものが無い。
		Uint64 advanceCount_ = 0;

		Uint32 recorded_[maxFrameCount_][viewCount_] = {};

		Float milliseconds_[viewCount_][scopeCount_] = {};

		Bool available_ = false;
	};

	/**
	* [EN]
	* RAII wrapper around GpuProfiler::Begin/End, for passes whose body is a
	* block rather than a single call.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* GpuProfiler::Begin/End の RAII ラッパ。パスの中身が1行の呼び出しではなく
	* ブロックになっている場合に使う。
	*
	* SEEDCORE_API は付けていない。今のところ Renderer からしか使わない=DLL の
	* 内側で完結しているため。Editor 等 DLL の外から使うようになったら付けること
	* (付け忘れると LNK2019 になる)。
	*/
	class GpuProfileScopeGuard
	{
	public:
		GpuProfileScopeGuard(GpuProfiler& profiler, D3D12CommandList* cmdList, GpuProfileView view, GpuProfileScope scope);
		~GpuProfileScopeGuard();

	private:
		GpuProfiler& profiler_;
		D3D12CommandList* cmdList_ = nullptr;
		GpuProfileView view_ = GpuProfileView::Editor;
		GpuProfileScope scope_ = GpuProfileScope::DepthPrepass;
	};
}
