#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/JobSystem/JobDeclaretions.h>
#include <FoundationEngine/JobSystem/JobConcept.h>
#include <FoundationEngine/JobSystem/JobWorker.h>
#include <FoundationEngine/JobSystem/JobGraph.h>
#include <FoundationEngine/JobSystem/JobNode.h>
#include <FoundationEngine/JobSystem/JobTask.h>
#include <FoundationEngine/JobSystem/JobTaskflow.h>
#include <FoundationEngine/JobSystem/JobRuntime.h>
#include <FoundationEngine/JobSystem/WorkerQueue.h>
#include <FoundationEngine/JobSystem/WorkerCommon.h>
#include <FoundationEngine/Pool/ObjectPool.h>

namespace SeedCore
{
	class JobNodeBase;

	/**
	* [EN]
	* The job system's work-stealing scheduler: owns a pool of
	* JobWorker threads, each with its own Chase-Lev work-stealing
	* queue, plus a set of overflow buffer queues used when a worker's
	* own queue is full. Submitting a JobTaskflow via Run/RunUntil
	* schedules its graph across the worker pool; workers pull from
	* their own queue first, then steal from other workers/buffers when idle.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* ジョブシステムのワークスティーリング・スケジューラ。JobWorker
	* スレッドのプールを所有し、各ワーカーは独自の Chase-Lev
	* ワークスティーリングキューを持つ。加えて、ワーカー自身のキューが
	* 満杯になった際に使用する、あふれ用のバッファキュー群も持つ。
	* Run/RunUntil を通じて JobTaskflow を投入すると、そのグラフが
	* ワーカープール全体へスケジューリングされる。ワーカーはまず自身の
	* キューから取り出し、アイドル時には他のワーカー/バッファから
	* 盗み取り（steal）を行う。
	*/
	class SEEDCORE_API JobExecutor
	{
	private:
		friend class FlowBuilder;
		friend class JobSubflow;
		friend class JobPreemptiveRuntime;
		friend class JobNonpreemptiveRuntime;

	public:
		/**
		* [EN]
		* Constructs an executor with n worker threads (defaulting to the
		* hardware concurrency), optionally supplying a custom
		* JobWorkerInterface (e.g. for custom thread naming/affinity)
		* used to spawn each worker.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* n 個のワーカースレッド（デフォルトはハードウェア並行度）を持つ
		* エグゼキュータを構築する。各ワーカーの生成に使用する、カスタムの
		* JobWorkerInterface（スレッド名やアフィニティのカスタマイズ用など）
		* を任意で指定できる。
		*/
		explicit JobExecutor(Size n = std::thread::hardware_concurrency(), ResourceRef<JobWorkerInterface> worker = nullptr);

		/**
		* [EN]
		* Waits for all outstanding work to finish, then shuts down and
		* joins every worker thread.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 未完了のすべての処理を待ってから、全ワーカースレッドを
		* シャットダウンし join する。
		*/
		~JobExecutor();

		/**
		* [EN]
		* Runs taskflow's graph exactly once, returning a future that
		* completes when the run finishes.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* taskflow のグラフをちょうど 1 回実行し、実行完了時に完了する
		* future を返す。
		*/
		JobFuture<void> Run(JobTaskflow& taskflow);

		/**
		* [EN]
		* Runs a moved-in taskflow's graph exactly once, returning a
		* future that completes when the run finishes.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* ムーブされた taskflow のグラフをちょうど 1 回実行し、実行完了時に
		* 完了する future を返す。
		*/
		JobFuture<void> Run(JobTaskflow&& taskflow);

		/**
		* [EN]
		* Runs taskflow's graph exactly once, invoking callable after it finishes.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* taskflow のグラフをちょうど 1 回実行し、完了後に callable を
		* 呼び出す。
		*/
		template<typename C>
		JobFuture<void> Run(JobTaskflow& taskflow, C&& callable)
		{
			return RunNumber(taskflow, 1, std::forward<C>(callable));
		}

		/**
		* [EN]
		* Runs a moved-in taskflow's graph exactly once, invoking
		* callable after it finishes.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* ムーブされた taskflow のグラフをちょうど 1 回実行し、完了後に
		* callable を呼び出す。
		*/
		template<typename C>
		JobFuture<void> Run(JobTaskflow&& taskflow, C&& callable)
		{
			return RunNumber(std::move(taskflow), 1, std::forward<C>(callable));
		}

		/**
		* [EN]
		* Runs taskflow's graph n times in sequence, returning a future
		* that completes when all n runs finish.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* taskflow のグラフを連続して n 回実行し、n 回すべての実行完了時に
		* 完了する future を返す。
		*/
		JobFuture<void> RunNumber(JobTaskflow& taskflow, Size n);

		/**
		* [EN]
		* Runs a moved-in taskflow's graph n times in sequence, returning
		* a future that completes when all n runs finish.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* ムーブされた taskflow のグラフを連続して n 回実行し、n 回すべての
		* 実行完了時に完了する future を返す。
		*/
		JobFuture<void> RunNumber(JobTaskflow&& taskflow, Size n);

		/**
		* [EN]
		* Runs taskflow's graph n times in sequence, invoking callable after the final run.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* taskflow のグラフを連続して n 回実行し、最後の実行後に callable
		* を呼び出す。
		*/
		template<typename C>
		JobFuture<void> RunNumber(JobTaskflow& taskflow, Size n, C&& callable)
		{
			return RunUntil(taskflow, [n]() mutable {return n-- == 0;}, std::forward<C>(callable));
		}

		/**
		* [EN]
		* Runs a moved-in taskflow's graph n times in sequence, invoking
		* callable after the final run.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* ムーブされた taskflow のグラフを連続して n 回実行し、最後の実行後に
		* callable を呼び出す。
		*/
		template<typename C>
		JobFuture<void> RunNumber(JobTaskflow&& taskflow, Size n, C&& callable)
		{
			return RunUntil(std::move(taskflow), [n]() mutable {return n-- == 0;}, std::forward<C>(callable));
		}

		/**
		* [EN]
		* Repeatedly runs taskflow's graph until predicate returns true
		* (checked after each pass), returning a future that completes
		* once the run stops.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* predicate が true を返すまで（各パス後にチェックしながら）
		* taskflow のグラフを繰り返し実行し、実行が停止した時点で完了する
		* future を返す。
		*/
		template<typename P>
		JobFuture<void> RunUntil(JobTaskflow& taskflow, P&& predicate)
		{
			return RunUntil(taskflow, std::forward<P>(predicate), []() {});
		}

		/**
		* [EN]
		* Repeatedly runs a moved-in taskflow's graph until predicate
		* returns true, returning a future that completes once the run stops.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* predicate が true を返すまで、ムーブされた taskflow のグラフを
		* 繰り返し実行し、実行が停止した時点で完了する future を返す。
		*/
		template<typename P>
		JobFuture<void> RunUntil(JobTaskflow&& taskflow, P&& predicate)
		{
			return RunUntil(std::move(taskflow), std::forward<P>(predicate), []() {});
		}

		/**
		* [EN]
		* Repeatedly runs taskflow's graph until predicate returns true,
		* then invokes callable. If the graph is already empty or
		* predicate is already true, runs callable synchronously and
		* returns an already-completed future; otherwise builds a
		* JobTopology, enqueues it on taskflow, and (if it's the first
		* pending run) schedules it immediately.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* predicate が true を返すまで taskflow のグラフを繰り返し実行し、
		* その後 callable を呼び出す。グラフが既に空であるか predicate が
		* 既に true であれば、callable を同期的に実行し、完了済みの future
		* を返す。それ以外の場合は JobTopology を構築して taskflow へ
		* 追加登録し、（それが最初の保留中の実行であれば）直ちにスケジューリング
		* する。
		*/
		template<typename P, typename C>
		JobFuture<void> RunUntil(JobTaskflow& taskflow, P&& predicate, C&& callable)
		{
			if (taskflow.Empty() || predicate())
			{
				callable();
				std::promise<void> promise;
				promise.set_value();
				return JobFuture<void>(promise.get_future());
			}

			IncrementTopology();

			ResourceRef<JobTopology> topology = MakeRef<JobTopology>(taskflow, std::forward<P>(predicate), std::forward<C>(callable));

			JobFuture<void> future(topology->promise_.get_future(), MakeObserve(topology));

			if (taskflow.FetchEnqueue(topology) == 0)
			{
				SetUpTopology(ThisWorker(), topology.get());
			}

			return future;
		}

		/**
		* [EN]
		* Repeatedly runs a moved-in taskflow's graph until predicate
		* returns true, then invokes callable (see the lvalue overload
		* for the full behavior).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* predicate が true を返すまで、ムーブされた taskflow のグラフを
		* 繰り返し実行し、その後 callable を呼び出す（完全な挙動は
		* 左辺値版のオーバーロードを参照）。
		*/
		template<typename P, typename C>
		JobFuture<void> RunUntil(JobTaskflow&& taskflow, P&& predicate, C&& callable)
		{
			if (taskflow.Empty() || predicate())
			{
				callable();
				std::promise<void> promise;
				promise.set_value();
				return JobFuture<void>(promise.get_future());
			}

			IncrementTopology();

			ResourceRef<JobTopology> topology = MakeRef<JobTopology>(taskflow, std::forward<P>(predicate), std::forward<C>(callable));

			JobFuture<void> future(topology->promise_.get_future(), MakeObserve(topology));

			if (taskflow.FetchEnqueue(topology) == 0)
			{
				SetUpTopology(ThisWorker(), topology.get());
			}

			return future;
		}

		/**
		* [EN]
		* Runs target's graph synchronously on the calling worker thread
		* (must be called from within a running task), blocking until it completes.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 呼び出し元のワーカースレッド上で target のグラフを同期的に実行し
		* （実行中のタスク内から呼び出す必要がある）、完了するまでブロックする。
		*/
		template<typename T>
		void Corun(T& target)
		{
			JobWorker* worker = ThisWorker();
			if (worker == nullptr)
			{

			}

			JobNodeBase anchor;
			CorunGraph(*worker, RetrieveGraph(target), nullptr, &anchor);
		}

		/**
		* [EN]
		* Helps the calling worker thread process other scheduled work
		* until predicate returns true (must be called from within a running task).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* predicate が true を返すまで、呼び出し元のワーカースレッドに
		* 他のスケジュール済み処理を手伝わせる（実行中のタスク内から
		* 呼び出す必要がある）。
		*/
		template<typename P>
		void CorunUntil(P&& predicate)
		{
			JobWorker* worker = ThisWorker();
			if (worker == nullptr)
			{

			}

			CorunUntil(*worker, std::forward<P>(predicate));
		}

		/**
		* [EN]
		* Blocks the calling thread until every currently outstanding
		* topology (across all taskflows run on this executor) has finished.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* このエグゼキュータで実行された全タスクフローにわたる、現在未完了の
		* すべてのトポロジーが完了するまで、呼び出し元のスレッドをブロックする。
		*/
		void WaitForAll();

		/**
		* [EN]
		* Returns the number of worker threads owned by this executor.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* このエグゼキュータが所有するワーカースレッドの数を返す。
		*/
		Size NumberWorkers()const noexcept;

		/**
		* [EN]
		* Returns the number of threads currently parked waiting for work.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 現在、処理待ちでパークされているスレッドの数を返す。
		*/
		Size NumberWaiters()const noexcept;

		/**
		* [EN]
		* Returns the total number of stealable queues (worker queues
		* plus overflow buffer queues).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 盗み取り可能なキューの総数（ワーカーキューとあふれ用バッファ
		* キューの合計）を返す。
		*/
		Size NumberQueues()const noexcept;

		/**
		* [EN]
		* Returns the number of topologies currently outstanding across the executor.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* エグゼキュータ全体で現在未完了となっているトポロジーの数を返す。
		*/
		Size NumberTopologies()const noexcept;

		/**
		* [EN]
		* Returns the JobWorker associated with the calling thread, or
		* nullptr if the calling thread is not one of this executor's workers.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 呼び出し元のスレッドに関連付けられた JobWorker を返す。呼び出し元
		* スレッドがこのエグゼキュータのワーカーでなければ nullptr を返す。
		*/
		JobWorker* ThisWorker();

		/**
		* [EN]
		* Returns the index of the calling thread's worker, or -1 if the
		* calling thread is not one of this executor's workers.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 呼び出し元のスレッドのワーカーインデックスを返す。呼び出し元
		* スレッドがこのエグゼキュータのワーカーでなければ -1 を返す。
		*/
		Int ThisWorkerID()const;

	private:
		/**
		* [EN]
		* An overflow queue (used when a worker's own queue is full or
		* the pusher isn't a worker) guarded by its own mutex.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* あふれ用のキュー（ワーカー自身のキューが満杯の場合や、投入元が
		* ワーカーでない場合に使用される）。専用のミューテックスで保護される。
		*/
		struct Buffer
		{
			/// [EN] Guards concurrent pushes to queue_ from non-owning threads.
			/// [JP] 非所有スレッドからの queue_ への並行 push を保護する。
			std::mutex mutex_;

			/// [EN] The underlying unbounded work-stealing queue.
			/// [JP] 内部の無制限ワークスティーリングキュー。
			UnboundedWorkerQueue<JobNode*> queue_;
		};

		/// [EN] The pool of worker threads owned by this executor.
		/// [JP] このエグゼキュータが所有するワーカースレッドのプール。
		DynamicArray<JobWorker> workers_;

		/// [EN] Overflow buffer queues, one per shard, used when a worker's own queue is full.
		/// [JP] シャードごとに 1 つずつ存在する、あふれ用バッファキュー。ワーカー自身のキューが満杯の場合に使用される。
		DynamicArray<Buffer> buffers_;

		/// [EN] Eventcount notifier used to wake parked workers when new work becomes available.
		/// [JP] 新しい処理が発生した際にパーク中のワーカーを起床させる、eventcount notifier。
		alignas(SC_CACHELINE_SIZE)DefaultNotifier notifier_;

		/// [EN] Count of topologies currently outstanding across the executor.
		/// [JP] エグゼキュータ全体で現在未完了となっているトポロジーの数。
		alignas(SC_CACHELINE_SIZE)std::atomic<Size> numberTopologies_{ 0 };

		/// [EN] Maps a std::thread::id to the owning JobWorker, so ThisWorker() can identify the calling thread.
		/// [JP] std::thread::id をそれを所有する JobWorker へ対応付ける。ThisWorker() が呼び出し元スレッドを特定するために使う。
		std::unordered_map<std::thread::id, JobWorker*> thread2Worker_;

	private:
		/**
		* [EN]
		* Signals every worker to stop and joins their threads.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 全ワーカーへ停止を通知し、それらのスレッドを join する。
		*/
		void Shutdown();

		/**
		* [EN]
		* Spawns n worker threads (using worker as the spawn strategy if
		* provided) and registers them in thread2Worker_.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* n 個のワーカースレッドを生成し（worker が指定されていれば
		* それを生成戦略として使用する）、thread2Worker_ に登録する。
		*/
		void Spawn(Size n, ResourceRef<JobWorkerInterface> worker);

		/**
		* [EN]
		* Pops and invokes tasks from worker's own queue as long as any
		* remain, updating cache with the last one instead of invoking it immediately.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* worker 自身のキューに残っている限りタスクを取り出して実行する。
		* 最後の 1 つは即座に実行せず cache に保持する。
		*/
		void ExploitTask(JobWorker& worker, JobNode*& cache);

		/**
		* [EN]
		* Attempts to steal and invoke a task from another worker/buffer;
		* returns whether one was found.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 他のワーカー/バッファからタスクを盗み取って実行することを試みる。
		* 見つかったかどうかを返す。
		*/
		Bool ExploreTask(JobWorker& worker, JobNode*& cache);

		/**
		* [EN]
		* Pushes cache onto worker's own queue (or a buffer if full) and
		* notifies a waiting worker, then clears cache.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* cache を worker 自身のキュー（満杯であればバッファ）へ push し、
		* 待機中のワーカーへ通知した後、cache をクリアする。
		*/
		void Schedule(JobWorker& worker, JobNode*& cache);

		/**
		* [EN]
		* Schedules cache from a non-worker thread by spilling it to a buffer, then clears cache.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* ワーカーでないスレッドから cache をバッファへ溢れさせることで
		* スケジューリングし、その後 cache をクリアする。
		*/
		void Schedule(JobNode*& cache);

		/**
		* [EN]
		* Schedules every runnable (zero strong-dependency) node in graph
		* under topology/parent.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* graph 内の実行可能な（強依存数が 0 の）全ノードを、topology/parent
		* のもとでスケジューリングする。
		*/
		void ScheduleGraph(JobWorker& worker, JobGraph& graph, JobTopology* topology, JobNodeBase* parent);

		/**
		* [EN]
		* Spills node into an overflow buffer queue.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* node をあふれ用バッファキューへ溢れさせる。
		*/
		void Spill(JobNode* node);

		/**
		* [EN]
		* Initializes topology's graph (see SetUpGraph) and schedules its
		* initially-runnable nodes.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* topology のグラフを初期化し（SetUpGraph を参照）、初期時点で
		* 実行可能なノードをスケジューリングする。
		*/
		void SetUpTopology(JobWorker* worker, JobTopology* topology);

		/**
		* [EN]
		* Initializes every node in graph's join counter under topology/
		* parent, returning the count of initially-runnable (zero
		* strong-dependency) nodes.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* topology/parent のもとで graph の各ノードの join カウンタを
		* 初期化し、初期時点で実行可能な（強依存数が 0 の）ノード数を返す。
		*/
		Size SetUpGraph(JobGraph& graph, JobTopology* topology, JobNodeBase* parent);

		/**
		* [EN]
		* Finalizes a finished topology: re-runs it if its predicate says
		* to continue, otherwise fulfills its promise and decrements the
		* outstanding topology count.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 完了したトポロジーを終端処理する: 述語が続行を指示していれば
		* 再実行し、そうでなければ promise を満たして未完了トポロジー数を
		* デクリメントする。
		*/
		void TearDownTopology(JobWorker& worker, JobTopology* topology, JobNode*& cache);

		/**
		* [EN]
		* Finalizes a non-async node once it has finished: notifies
		* successors/parent and, if it was the graph's last node, tears
		* down the owning topology.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 非非同期ノードが完了した際の終端処理: 後続/親ノードへ通知し、
		* それがグラフ最後のノードであれば所有元のトポロジーを終端処理する。
		*/
		void TearDownNonasync(JobWorker& worker, JobNode* node, JobNode*& cache);

		/**
		* [EN]
		* Invokes node then tears it down as a non-async node.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* node を実行した後、非非同期ノードとして終端処理する。
		*/
		void TearDownInvoke(JobWorker& worker, JobNode* node, JobNode*& cache);

		/**
		* [EN]
		* Increments the executor-wide outstanding topology count.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* エグゼキュータ全体の未完了トポロジー数をインクリメントする。
		*/
		void IncrementTopology();

		/**
		* [EN]
		* Decrements the executor-wide outstanding topology count.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* エグゼキュータ全体の未完了トポロジー数をデクリメントする。
		*/
		void DecrementTopology();

		/**
		* [EN]
		* Dispatches node to the appropriate InvokeXxxTask overload based
		* on its NodeHandle alternative, then handles any resulting exception.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* node の NodeHandle の選択肢に応じて、適切な InvokeXxxTask
		* オーバーロードへディスパッチし、発生した例外があれば処理する。
		*/
		void Invoke(JobWorker& worker, JobNode* node);

		/**
		* [EN]
		* Invokes a Static task's callable.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* Static タスクの呼び出し可能オブジェクトを実行する。
		*/
		void InvokeStaticTask(JobWorker& worker, JobNode* node);

		/**
		* [EN]
		* Invokes a NonpreemptiveRuntime task's callable, passing it a
		* JobNonpreemptiveRuntime handle.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* NonpreemptiveRuntime タスクの呼び出し可能オブジェクトを、
		* JobNonpreemptiveRuntime ハンドルを渡して実行する。
		*/
		void InvokeNonpreemptiveRuntimeTask(JobWorker& worker, JobNode* node);

		/**
		* [EN]
		* Invokes a SingleCondition task's callable, appending the single
		* resulting successor index to conds.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* SingleCondition タスクの呼び出し可能オブジェクトを実行し、
		* 結果として得られる単一の後続インデックスを conds へ追加する。
		*/
		void InvokeSingleConditionTask(JobWorker& worker, JobNode* node, HybridArray<Int>& conds);

		/**
		* [EN]
		* Invokes a MultiCondition task's callable, appending every
		* resulting successor index to conds.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* MultiCondition タスクの呼び出し可能オブジェクトを実行し、
		* 結果として得られる全後続インデックスを conds へ追加する。
		*/
		void InvokeMultiConditionTask(JobWorker& worker, JobNode* node, HybridArray<Int>& conds);

		/**
		* [EN]
		* Invokes a Subflow task's builder callable, scheduling the
		* resulting subgraph. Returns whether the node should be
		* considered async (i.e. not yet finished).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* Subflow タスクのビルダー呼び出し可能オブジェクトを実行し、結果の
		* サブグラフをスケジューリングする。ノードを非同期（まだ完了して
		* いない）として扱うべきかどうかを返す。
		*/
		Bool InvokeSubflowTask(JobWorker& worker, JobNode* node);

		/**
		* [EN]
		* Handles an OwnedModule task by delegating to
		* InvokeModuleTaskImplementation with the externally-owned graph.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 外部が所有するグラフを渡して InvokeModuleTaskImplementation へ
		* 委譲することで、OwnedModule タスクを処理する。
		*/
		Bool InvokeOwnedModuleTask(JobWorker& worker, JobNode* node);

		/**
		* [EN]
		* Handles an AdoptedModule task by delegating to
		* InvokeModuleTaskImplementation with the adopted (owned) graph.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 養子化（所有）されたグラフを渡して InvokeModuleTaskImplementation
		* へ委譲することで、AdoptedModule タスクを処理する。
		*/
		Bool InvokeAdoptedModuleTask(JobWorker& worker, JobNode* node);

		/**
		* [EN]
		* Shared implementation for OwnedModule/AdoptedModule: schedules
		* graph as node's subgraph. Returns whether the node should be
		* considered async.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* OwnedModule/AdoptedModule の共通実装: graph を node のサブグラフ
		* としてスケジューリングする。ノードを非同期として扱うべきかどうかを
		* 返す。
		*/
		Bool InvokeModuleTaskImplementation(JobWorker& worker, JobNode* node, JobGraph& graph);

		/**
		* [EN]
		* Invokes a PreemptiveRuntime task's callable, passing it a
		* JobPreemptiveRuntime handle that can yield/suspend execution.
		* Returns whether the node should be considered async.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* PreemptiveRuntime タスクの呼び出し可能オブジェクトを、実行を
		* yield/中断できる JobPreemptiveRuntime ハンドルを渡して実行する。
		* ノードを非同期として扱うべきかどうかを返す。
		*/
		Bool InvokePreemptiveRuntimeTask(JobWorker& worker, JobNode* node);

		/**
		* [EN]
		* Shared implementation invoking function against node's
		* preemptive runtime state. Returns whether the node should be
		* considered async.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* node のプリエンプティブランタイム状態に対して function を実行する
		* 共通実装。ノードを非同期として扱うべきかどうかを返す。
		*/
		Bool InvokeRuntimeTaskImplementation(JobWorker& worker, JobNode* node, std::function<void(JobPreemptiveRuntime&)>& function);

		/**
		* [EN]
		* Overload of InvokeRuntimeTaskImplementation for callables that
		* additionally receive a resume/first-call flag.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 再開/初回呼び出しフラグを追加で受け取る呼び出し可能オブジェクト
		* 向けの InvokeRuntimeTaskImplementation オーバーロード。
		*/
		Bool InvokeRuntimeTaskImplementation(JobWorker& worker, JobNode* node, std::function<void(JobPreemptiveRuntime&, Bool)>& function);

		/**
		* [EN]
		* Captures the exception currently in flight and propagates it up
		* through node's parent chain / owning topology.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 現在発生している例外を捕捉し、node の親チェーン/所有元の
		* トポロジーへ伝播させる。
		*/
		void ProcessException(JobWorker& worker, JobNode* node);

		/**
		* [EN]
		* Updates cache with node, first scheduling whatever was
		* previously cached (see Schedule).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* cache を node で更新する。その前に、以前 cache されていたものを
		* スケジューリングする（Schedule を参照）。
		*/
		void UpdateCache(JobWorker& worker, JobNode*& cache, JobNode* node);

		/**
		* [EN]
		* Runs graph to completion synchronously on worker (used by Corun
		* and subflow execution), under topology/parent.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* worker 上で graph を同期的に完了まで実行する（Corun やサブフロー
		* 実行で使用される）。topology/parent のもとで実行する。
		*/
		void CorunGraph(JobWorker& worker, JobGraph& graph, JobTopology* topology, JobNodeBase* parent);

		/**
		* [EN]
		* Blocks worker until a task becomes available (parking it via
		* the notifier if necessary), storing it in cache. Returns
		* whether a task was obtained (false typically means the executor is shutting down).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* タスクが利用可能になるまで worker をブロックし（必要なら
		* notifier 経由でパークする）、cache へ格納する。タスクを取得
		* できたかどうかを返す（false は通常、エグゼキュータがシャット
		* ダウン中であることを意味する）。
		*/
		Bool WaitForTask(JobWorker& worker, JobNode*& cache);

		/**
		* [EN]
		* Work-stealing loop run by a corouning worker: exploits its own
		* queue, then explores (steals from) other queues, until
		* stopPredicate returns true.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* Corun 中のワーカーが実行するワークスティーリングループ:
		* stopPredicate が true を返すまで、自身のキューを消費し、その後
		* 他のキューを探索（盗み取り）する。
		*/
		template<typename P>
		void CorunUntil(JobWorker& worker, P&& stopPredicate)
		{
			const Size MAX_VICTIM = NumberQueues();
			const Size MAX_STEALS = ((MAX_VICTIM + 1) << 1);

		exploit:

			while (!stopPredicate())
			{
				if (auto t = worker.wsq_.pop();t)
				{
					Invoke(worker, t);
				}
				else
				{
					Size numberSteals = 0;
					Size victim = worker.stickyVictim_;

				explore:

					t = (victim < workers_.size()) ? workers_[victim].wsq_.steal() : buffers_[victim - workers_.size()].queue_.steal();

					if (t)
					{
						Invoke(worker, t);
						worker.stickyVictim_ = victim;
						goto exploit;
					}
					else if (!stopPredicate())
					{
						if (++numberSteals > MAX_STEALS)
						{
							std::this_thread::yield();
						}
						victim = worker.rdgen_() % MAX_VICTIM;
						goto explore;
					}
					else
					{
						break;
					}
				}
			}
		}

		/**
		* [EN]
		* Bulk-pushes n items starting at first onto worker's own queue,
		* spilling whatever doesn't fit, then notifies n waiters.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* first から始まる n 個の要素を worker 自身のキューへ一括 push し、
		* 収まらなかった分をあふれさせた後、n 個分の待機者へ通知する。
		*/
		template<typename I>
		void BulkSchedule(JobWorker& worker, I first, Size n)
		{
			if (n == 0)
			{
				return;
			}

			if (auto num = worker.wsq_.try_bulk_push(first, n);num != n)
			{
				BulkSpill(first, n - num);
			}
			notifier_.notify_count(n);
		}

		/**
		* [EN]
		* Bulk-schedules n items starting at first from a non-worker
		* thread by spilling all of them, then notifies n waiters.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* ワーカーでないスレッドから、first で始まる n 個の要素をすべて
		* あふれさせることで一括スケジューリングし、n 個分の待機者へ通知する。
		*/
		template<typename I>
		void BulkSchedule(I first, Size n)
		{
			if (n == 0)
			{
				return;
			}

			BulkSpill(first, n);
			notifier_.notify_count(n);
		}

		/**
		* [EN]
		* Bulk-pushes n items starting at first into a single overflow
		* buffer chosen by hashing the first item.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 先頭要素をハッシュして選んだ単一のあふれ用バッファへ、first から
		* 始まる n 個の要素を一括 push する。
		*/
		template<typename I>
		void BulkSpill(I first, Size n)
		{
			auto buffer = ((reinterpret_cast<uintptr_t>(*first) * 2654435761ULL) >> 32) % buffers_.size();
			std::scoped_lock lock(buffers_[buffer].mutex_);
			buffers_[buffer].queue_.bulk_push(first, n);
		}

		/**
		* [EN]
		* Bulk-pushes n items starting at first, distributing them
		* round-robin across every overflow buffer starting from a
		* hash-chosen offset (spreads load more evenly than BulkSpill).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* first から始まる n 個の要素を、ハッシュで選んだオフセットから
		* 開始して全あふれ用バッファへラウンドロビンで分配し一括 push する
		* （BulkSpill よりも負荷をより均等に分散する）。
		*/
		template<typename I>
		void BulkSpillRoundRobin(I first, Size n)
		{
			const Size buffer = buffers_.size();
			const Size start = ((reinterpret_cast<uintptr_t>(*first) * 2654435761ULL) >> 32) % buffer;
			const Size perBuffer = (n + buffer - 1) / buffer;
			Size remaining = n;
			for (Size index = 0;index < buffer && remaining>0;++index)
			{
				Size b = (start + index) % buffer;
				Size chunk = Min(perBuffer, remaining);

				{
					std::scoped_lock lock(buffers_[b].mutex_);
					buffers_[b].queue_.bulk_push(first, chunk);
				}

				remaining -= chunk;
			}
		}

		/**
		* [EN]
		* Fixed-capacity (N) variant of UpdateCache: buffers node into
		* array instead of scheduling immediately, flushing the batch via
		* BulkSchedule once it reaches N entries.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* UpdateCache の固定容量（N）版: node を即座にスケジューリングせず
		* array へ溜め込み、N 個に達した時点で BulkSchedule によって
		* バッチを送出する。
		*/
		template<Size N>
		void BulkUpdateCache(JobWorker& worker, JobNode*& cache, JobNode* node, StaticArray<JobNode*, N>& array, Size& n)
		{
			if (cache)
			{
				array[n++] = cache;
				if (n == N)
				{
					BulkSchedule(worker, array, n);
					n = 0;
				}
			}
			cache = node;
		}
	};
}
