#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/JobSystem/JobDeclaretions.h>
#include <FoundationEngine/JobSystem/WorkerCommon.h>
#include <FoundationEngine/JobSystem/WorkerQueue.h>
#include <FoundationEngine/JobSystem/AtomicNotifier.h>
#include <FoundationEngine/JobSystem/NonblockingNotifier.h>
#include <FoundationEngine/Math/Random/Xorshift.h>
#include <FoundationEngine/Pool/ObjectPool.h>

namespace SeedCore
{
#ifdef SC_ENABLE_ATOMIC_NOTIFIER
	/// [EN] The eventcount notifier type used by JobExecutor, selected at compile time via SC_ENABLE_ATOMIC_NOTIFIER.
	/// [JP] JobExecutor が使用する eventcount notifier 型。SC_ENABLE_ATOMIC_NOTIFIER によりコンパイル時に選択される。
	using DefaultNotifier = AtomicNotifier;
#else
	/// [EN] The eventcount notifier type used by JobExecutor, selected at compile time via SC_ENABLE_ATOMIC_NOTIFIER.
	/// [JP] JobExecutor が使用する eventcount notifier 型。SC_ENABLE_ATOMIC_NOTIFIER によりコンパイル時に選択される。
	using DefaultNotifier = NonblockingNotifier;
#endif

	/**
	* [EN]
	* Per-thread state owned by JobExecutor: the OS thread itself, its
	* own bounded work-stealing queue, and the scratch state (steal
	* target, RNG) used by the scheduling loop. Only JobExecutor and a
	* few closely-related friend classes touch this directly; external
	* code observes it read-only through JobWorkerView.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* JobExecutor が所有する、スレッドごとの状態。OS スレッド自体、
	* 専用の有界ワークスティーリングキュー、およびスケジューリング
	* ループが使用する作業用状態（盗み取り対象、乱数生成器）を保持する。
	* JobExecutor といくつかの密接に関連する friend クラスのみが
	* これを直接操作する。外部コードは JobWorkerView を通じて読み取り
	* 専用で観測する。
	*/
	class JobWorker
	{
	private:
		friend class JobExecutor;
		friend class JobPreemptiveRuntime;
		friend class JobWorkerView;

		/// [EN] The bounded work-stealing queue type used to store this worker's own pending nodes.
		/// [JP] このワーカー自身の保留中ノードを格納する、有界ワークスティーリングキュー型。
		using WsqType = BoundedWorkerQueue<JobNode*>;

	public:
		/**
		* [EN]
		* Returns this worker's index within the owning executor's pool.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 所有元のエグゼキュータのプール内における、このワーカーの
		* インデックスを返す。
		*/
		inline Size ID()const;

		/**
		* [EN]
		* Returns the current number of nodes queued on this worker.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* このワーカーに現在キューイングされているノードの数を返す。
		*/
		inline Size QueueSize()const;

		/**
		* [EN]
		* Returns this worker's queue capacity.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* このワーカーのキュー容量を返す。
		*/
		inline Size QueueCapacity()const;

		/**
		* [EN]
		* Returns a reference to the underlying OS thread.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 内部の OS スレッドへの参照を返す。
		*/
		std::thread& Thread();

	private:
		/// [EN] Flag signaling this worker to stop once its current work is drained; set by JobExecutor::Shutdown.
		/// [JP] 現在の処理を消化し終えた時点で停止するようこのワーカーへ通知するフラグ。JobExecutor::Shutdown により設定される。
		alignas(SC_CACHELINE_SIZE)std::atomic_flag done_{};

		/// [EN] Index of this worker within the owning executor's pool.
		/// [JP] 所有元のエグゼキュータのプール内における、このワーカーのインデックス。
		Size id_;

		/// [EN] The victim worker/buffer index this worker will preferentially steal from next.
		/// [JP] このワーカーが次に優先的に盗み取りを行う、対象ワーカー/バッファのインデックス。
		Size stickyVictim_;

		/// [EN] Per-worker RNG, used to pick a random steal victim when the sticky victim yields nothing.
		/// [JP] ワーカーごとの乱数生成器。sticky な盗み取り対象から何も得られない場合に、ランダムな対象を選ぶために使う。
		Xorshift<Uint32> rdgen_;

		/// [EN] The underlying OS thread running this worker's scheduling loop.
		/// [JP] このワーカーのスケジューリングループを実行する、内部の OS スレッド。
		std::thread thread_;

		/// [EN] This worker's own bounded work-stealing queue.
		/// [JP] このワーカー自身の有界ワークスティーリングキュー。
		WsqType wsq_;
	};

	/**
	* [EN]
	* Read-only, non-owning view onto a JobWorker, exposing its
	* identity and queue statistics without granting access to its
	* internal scheduling state.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* JobWorker に対する読み取り専用・非所有のビュー。内部の
	* スケジューリング状態へのアクセスは許可せず、識別情報とキュー統計
	* のみを公開する。
	*/
	class JobWorkerView
	{
	private:
		friend class JobExecutor;

	public:
		/**
		* [EN]
		* Returns the underlying worker's index within its executor's pool.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 内部のワーカーの、所属エグゼキュータのプール内でのインデックスを
		* 返す。
		*/
		Size ID()const;

		/**
		* [EN]
		* Returns the underlying worker's current queue size.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 内部のワーカーの、現在のキューサイズを返す。
		*/
		Size QueueSize()const;

		/**
		* [EN]
		* Returns the underlying worker's queue capacity.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 内部のワーカーのキュー容量を返す。
		*/
		Size QueueCapacity()const;

	private:
		/**
		* [EN]
		* Constructs a view referring to worker directly.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* worker を直接参照するビューを構築する。
		*/
		JobWorkerView(const JobWorker& worker);

		/**
		* [EN]
		* Copy-constructs, referring to the same underlying worker as the source view.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* コピー元のビューと同じ内部ワーカーを参照するようにコピー構築する。
		*/
		JobWorkerView(const JobWorkerView&) = default;

		/// [EN] The worker this view refers to.
		/// [JP] このビューが参照するワーカー。
		const JobWorker& worker_;
	};

	/**
	* [EN]
	* Hook interface allowing user code to run custom logic at the start
	* and end of each worker thread's lifetime (e.g. thread naming,
	* thread-local setup, affinity, per-thread exception logging).
	* Passed to JobExecutor's constructor.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 各ワーカースレッドのライフタイムの開始時・終了時に、ユーザーコードが
	* 独自の処理（スレッド名の設定、スレッドローカルの初期化、
	* アフィニティ設定、スレッドごとの例外ログなど）を実行できるようにする
	* フック用インターフェース。JobExecutor のコンストラクタへ渡す。
	*/
	class JobWorkerInterface
	{
	public:
		/**
		* [EN]
		* Virtual destructor; uses the compiler-generated default.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 仮想デストラクタ。コンパイラ生成のデフォルトを使用する。
		*/
		virtual ~JobWorkerInterface() = default;

		/**
		* [EN]
		* Invoked once at the start of worker's thread, before it begins
		* processing tasks.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* worker のスレッド開始時、タスクの処理を始める前に一度だけ
		* 呼び出される。
		*/
		virtual void SchedulerPrologue(JobWorker& worker) = 0;

		/**
		* [EN]
		* Invoked once at the end of worker's thread, after it stops
		* processing tasks; ptr holds any uncaught exception that
		* propagated out of the scheduling loop, or nullptr if none.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* worker のスレッド終了時、タスクの処理を停止した後に一度だけ
		* 呼び出される。ptr にはスケジューリングループから伝播した
		* 未捕捉の例外が入る（無ければ nullptr）。
		*/
		virtual void SchedulerEpilogue(JobWorker& worker, std::exception_ptr ptr) = 0;
	};

	/**
	* [EN]
	* Constructs a ResourceRef<T> (a JobWorkerInterface implementation)
	* from args, for passing into JobExecutor's constructor.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* args から ResourceRef<T>（JobWorkerInterface の実装）を構築する。
	* JobExecutor のコンストラクタへ渡すために使う。
	*/
	template<typename T, typename... Args>
	ResourceRef<T> MakeWorkerInterface(Args&&... args)
	{
		return MakeRef<T>(std::forward<Args>(args)...);
	}
}
