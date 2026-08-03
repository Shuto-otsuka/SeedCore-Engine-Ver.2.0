#include <FoundationEngine/JobSystem/JobExecutor.h>
#include <FoundationEngine/JobSystem/JobTaskflow.h>
#include <FoundationEngine/JobSystem/JobTopology.h>

namespace SeedCore
{
	/**
	* [EN]
	* Constructs an executor with n worker threads (defaulting to the
	* hardware concurrency), optionally supplying a custom
	* JobWorkerInterface used to spawn each worker.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* n 個のワーカースレッド（デフォルトはハードウェア並行度）を持つ
	* エグゼキュータを構築する。各ワーカーの生成に使用する、カスタムの
	* JobWorkerInterface を任意で指定できる。
	*/
	JobExecutor::JobExecutor(Size n, ResourceRef<JobWorkerInterface> worker) :workers_(n), buffers_(std::bit_width(n)), notifier_(n)
	{
		if (n == 0)
		{

		}

#ifdef SC_DISABLE_EXCEPTION_HANDLING
		try
		{
#endif
			Spawn(n, std::move(worker));
#ifdef SC_DISABLE_EXCEPTION_HANDLING
		}
		catch (...)
		{
			Shutdown();
			std::rethrow_exception(std::current_exception());
		}
#endif
	}

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
	JobExecutor::~JobExecutor()
	{
		Shutdown();
	}

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
	JobFuture<void> JobExecutor::Run(JobTaskflow& taskflow)
	{
		return RunNumber(taskflow, 1, []() {});
	}

	/**
	* [EN]
	* Runs a moved-in taskflow's graph exactly once, returning a future
	* that completes when the run finishes.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* ムーブされた taskflow のグラフをちょうど 1 回実行し、実行完了時に
	* 完了する future を返す。
	*/
	JobFuture<void> JobExecutor::Run(JobTaskflow&& taskflow)
	{
		return RunNumber(std::move(taskflow), 1, []() {});
	}

	/**
	* [EN]
	* Runs taskflow's graph n times in sequence, returning a future that
	* completes when all n runs finish.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* taskflow のグラフを連続して n 回実行し、n 回すべての実行完了時に
	* 完了する future を返す。
	*/
	JobFuture<void> JobExecutor::RunNumber(JobTaskflow& taskflow, Size n)
	{
		return RunNumber(taskflow, n, []() {});
	}

	/**
	* [EN]
	* Runs a moved-in taskflow's graph n times in sequence, returning a
	* future that completes when all n runs finish.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* ムーブされた taskflow のグラフを連続して n 回実行し、n 回すべての
	* 実行完了時に完了する future を返す。
	*/
	JobFuture<void> JobExecutor::RunNumber(JobTaskflow&& taskflow, Size n)
	{
		return RunNumber(std::move(taskflow), n, []() {});
	}

	/**
	* [EN]
	* Blocks the calling thread until every currently outstanding
	* topology has finished.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 現在未完了のすべてのトポロジーが完了するまで、呼び出し元の
	* スレッドをブロックする。
	*/
	void JobExecutor::WaitForAll()
	{
		Size n = numberTopologies_.load(std::memory_order_acquire);
		while (n != 0)
		{
			numberTopologies_.wait(n, std::memory_order_acquire);
			n = numberTopologies_.load(std::memory_order_acquire);
		}
	}

	/**
	* [EN]
	* Returns the number of worker threads owned by this executor.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* このエグゼキュータが所有するワーカースレッドの数を返す。
	*/
	Size JobExecutor::NumberWorkers()const noexcept
	{
		return workers_.size();
	}

	/**
	* [EN]
	* Returns the number of threads currently parked waiting for work.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 現在、処理待ちでパークされているスレッドの数を返す。
	*/
	Size JobExecutor::NumberWaiters()const noexcept
	{
		return notifier_.count();
	}

	/**
	* [EN]
	* Returns the total number of stealable queues (worker queues plus
	* overflow buffer queues).
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 盗み取り可能なキューの総数（ワーカーキューとあふれ用バッファ
	* キューの合計）を返す。
	*/
	Size JobExecutor::NumberQueues()const noexcept
	{
		return workers_.size() + buffers_.size();
	}

	/**
	* [EN]
	* Returns the number of topologies currently outstanding across the executor.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* エグゼキュータ全体で現在未完了となっているトポロジーの数を返す。
	*/
	Size JobExecutor::NumberTopologies()const noexcept
	{
		return numberTopologies_.load(std::memory_order_relaxed);
	}

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
	JobWorker* JobExecutor::ThisWorker()
	{
		auto iterator = thread2Worker_.find(std::this_thread::get_id());
		return iterator == thread2Worker_.end() ? nullptr : iterator->second;
	}

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
	Int JobExecutor::ThisWorkerID()const
	{
		auto index = thread2Worker_.find(std::this_thread::get_id());
		return index == thread2Worker_.end() ? -1 : static_cast<Int>(index->second->id_);
	}

	/**
	* [EN]
	* Signals every worker to stop and joins their threads.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 全ワーカーへ停止を通知し、それらのスレッドを join する。
	*/
	void JobExecutor::Shutdown()
	{
		WaitForAll();

		for (Size index = 0;index < workers_.size();++index)
		{
			workers_[index].done_.test_and_set(std::memory_order_relaxed);
		}

		notifier_.notify_all();

		for (auto& worker : workers_)
		{
			if (worker.thread_.joinable())
			{
				worker.thread_.join();
			}
		}
	}

	/**
	* [EN]
	* Spawns n worker threads (using workerInterface as the spawn
	* strategy if provided) and registers them in thread2Worker_.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* n 個のワーカースレッドを生成し（workerInterface が指定されていれば
	* それを生成戦略として使用する）、thread2Worker_ に登録する。
	*/
	void JobExecutor::Spawn(Size n, ResourceRef<JobWorkerInterface> workerInterface)
	{
		for (Size id = 0;id < n;++id)
		{
			workers_[id].thread_ = std::thread([&, id, workerInterface]()
				{
					auto& worker = workers_[id];

					worker.id_ = id;
					worker.stickyVictim_ = id;
					worker.rdgen_.Seed(static_cast<Uint32>(std::hash<std::thread::id>()(std::this_thread::get_id())));

					if (&worker)
					{
						workerInterface->SchedulerPrologue(worker);
					}

					JobNode* t = nullptr;
					std::exception_ptr ptr = nullptr;

#if SC_DISABLE_EXCEPTION_HANDLING
					try
					{
#endif
						while (1)
						{
							ExploitTask(worker, t);

							if (WaitForTask(worker, t) == false)
							{
								break;
							}
						}
#if SC_DISABLE_EXCEPTION_HANDLING
					}
					catch (...)
					{
						ptr = std::current_exception();
					}
#endif

					if (workerInterface)
					{
						workerInterface->SchedulerEpilogue(worker, ptr);
					}
			});

			thread2Worker_.emplace(workers_[id].thread_.get_id(), &workers_[id]);
		}
	}

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
	void JobExecutor::ExploitTask(JobWorker& worker, JobNode*& cache)
	{
		while (cache)
		{
			Invoke(worker, cache);
			cache = worker.wsq_.pop();
		}
	}

	/**
	* [EN]
	* Attempts to steal and invoke a task from another worker/buffer;
	* returns whether the caller should keep running (false means the
	* executor is shutting down).
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 他のワーカー/バッファからタスクを盗み取って実行することを試みる。
	* 呼び出し側が実行を継続すべきかどうかを返す（false はエグゼキュータが
	* シャットダウン中であることを意味する）。
	*/
	Bool JobExecutor::ExploreTask(JobWorker& worker, JobNode*& cache)
	{
		if (numberTopologies_.load(std::memory_order_relaxed) == 0)
		{
			return true;
		}

		const Size MAX_VICTIM = NumberQueues();
		const Size MAX_STEALS = ((MAX_VICTIM + 1) << 1);

		Size numberSteals = 0;
		Size victim = worker.stickyVictim_;

		while (true)
		{
			cache = (victim < workers_.size()) ? workers_[victim].wsq_.steal() : buffers_[victim - workers_.size()].queue_.steal();

			if (cache)
			{
				worker.stickyVictim_ = victim;
				break;
			}

			victim = worker.rdgen_() % (MAX_VICTIM - 1);
			if (victim >= worker.id_)
			{
				victim++;
			}

			if (++numberSteals > MAX_STEALS)
			{
				std::this_thread::yield();
				if (numberSteals > 150 + MAX_STEALS)
				{
					break;
				}
			}

			if (worker.done_.test(std::memory_order_relaxed))
			{
				return false;
			}
		}

		return true;
	}

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
	void JobExecutor::Schedule(JobWorker& worker, JobNode*& cache)
	{
		if (worker.wsq_.try_push(cache) == false)
		{
			Spill(cache);
		}
		notifier_.notify_one();
	}

	/**
	* [EN]
	* Schedules cache from a non-worker thread by spilling it to a
	* buffer, then clears cache.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* ワーカーでないスレッドから cache をバッファへ溢れさせることで
	* スケジューリングし、その後 cache をクリアする。
	*/
	void JobExecutor::Schedule(JobNode*& cache)
	{
		Spill(cache);
		notifier_.notify_one();
	}

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
	void JobExecutor::ScheduleGraph(JobWorker& worker, JobGraph& graph, JobTopology* topology, JobNodeBase* parent)
	{
		Size numberSources = SetUpGraph(graph, topology, parent);
		parent->joinCounter_.fetch_add(numberSources, std::memory_order_relaxed);
		BulkSchedule(worker, graph.begin(), numberSources);
	}

	/**
	* [EN]
	* Spills node into an overflow buffer queue.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* node をあふれ用バッファキューへ溢れさせる。
	*/
	void JobExecutor::Spill(JobNode* node)
	{
		auto buffer = (reinterpret_cast<uintptr_t>(node) >> 16) % buffers_.size();
		std::scoped_lock lock(buffers_[buffer].mutex_);
		buffers_[buffer].queue_.push(node);
	}

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
	void JobExecutor::SetUpTopology(JobWorker* worker, JobTopology* topology)
	{
		auto& graph = topology->taskflow_.graph_;
		Size numberSources = SetUpGraph(graph, topology, topology);
		topology->joinCounter_.store(numberSources, std::memory_order_relaxed);
		worker ? BulkSchedule(*worker, graph.begin(), numberSources) : BulkSchedule(graph.begin(), numberSources);
	}

	/**
	* [EN]
	* Initializes every node in graph's join counter under
	* topology/parent, returning the count of initially-runnable (zero
	* strong-dependency) nodes.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* topology/parent のもとで graph の各ノードの join カウンタを
	* 初期化し、初期時点で実行可能な（強依存数が 0 の）ノード数を返す。
	*/
	Size JobExecutor::SetUpGraph(JobGraph& graph, JobTopology* topology, JobNodeBase* parent)
	{
		auto first = graph.begin();
		auto last = graph.end();
		auto send = first;

		for (;first != last;++first)
		{
			auto node = *first;
			node->topology_ = topology;
			node->parent_ = parent;
			node->nstate_ = JobNodeState::NONE;
			node->estate_.store(JobExceptionState::NONE, std::memory_order_relaxed);
			node->SetUpJoinCounter();
			node->exceptionPtr_ = nullptr;

			if (node->NumberPredecessors() == 0)
			{
				std::iter_swap(send++, first);
			}
		}
		return send - graph.begin();
	}

	/**
	* [EN]
	* Finalizes a finished topology: re-runs it if its predicate says to
	* continue, otherwise fulfills its promise and decrements the
	* outstanding topology count (moving on to the next queued
	* topology, if any).
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 完了したトポロジーを終端処理する: 述語が続行を指示していれば
	* 再実行し、そうでなければ promise を満たして未完了トポロジー数を
	* デクリメントする（キューに次のトポロジーがあれば、それへ進む）。
	*/
	void JobExecutor::TearDownTopology(JobWorker& worker, JobTopology* topology, JobNode*& cache)
	{
		auto& flow = topology->taskflow_;

		if (!topology->Cancelled() && !topology->predicate_())
		{
			ScheduleGraph(worker, topology->taskflow_.graph_, topology, topology);
		}
		else
		{
			topology->onFinish_();

			if (std::unique_lock<std::mutex> lock(flow.mutex_);flow.topologies_.size() > 1)
			{
				auto fetchedTopology{ std::move(flow.topologies_.front()) };

				flow.topologies_.pop();
				topology = flow.topologies_.front().get();

				lock.unlock();

				fetchedTopology->CarryOutPromise();

				DecrementTopology();

				ScheduleGraph(worker, topology->taskflow_.graph_, topology, topology);
			}
			else
			{
				auto fetchedTopology{ std::move(flow.topologies_.front()) };

				flow.topologies_.pop();

				lock.unlock();

				fetchedTopology->CarryOutPromise();

				DecrementTopology();

				if (auto parent = fetchedTopology->parent_;parent)
				{
					if (parent->joinCounter_.fetch_sub(1, std::memory_order_acq_rel) == 1)
					{
						UpdateCache(worker, cache, static_cast<JobNode*>(parent));
					}
				}
			}
		}
	}

	/**
	* [EN]
	* Finalizes a non-async node once it has finished: notifies
	* successors/parent and, if it was the graph's last node, tears down
	* the owning topology.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 非非同期ノードが完了した際の終端処理: 後続/親ノードへ通知し、
	* それがグラフ最後のノードであれば所有元のトポロジーを終端処理する。
	*/
	void JobExecutor::TearDownNonasync(JobWorker& worker, JobNode* node, JobNode*& cache)
	{
		if (auto parent = node->parent_;parent == node->topology_)
		{
			if (parent->joinCounter_.fetch_sub(1, std::memory_order_acq_rel) == 1)
			{
				TearDownTopology(worker, node->topology_, cache);
			}
		}
		else
		{
			auto state = parent->nstate_;
			if (parent->joinCounter_.fetch_sub(1, std::memory_order_acq_rel) == 1)
			{
				if (state & JobNodeState::PREEMPTED)
				{
					UpdateCache(worker, cache, static_cast<JobNode*>(parent));
				}
			}
		}
	}

	/**
	* [EN]
	* Invokes node then tears it down as a non-async node.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* node を実行した後、非非同期ノードとして終端処理する。
	*/
	void JobExecutor::TearDownInvoke(JobWorker& worker, JobNode* node, JobNode*& cache)
	{
		TearDownNonasync(worker, node, cache);
	}

	/**
	* [EN]
	* Increments the executor-wide outstanding topology count.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* エグゼキュータ全体の未完了トポロジー数をインクリメントする。
	*/
	void JobExecutor::IncrementTopology()
	{
		numberTopologies_.fetch_add(1, std::memory_order_relaxed);
	}

	/**
	* [EN]
	* Decrements the executor-wide outstanding topology count, waking
	* any thread blocked in WaitForAll if it just reached zero.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* エグゼキュータ全体の未完了トポロジー数をデクリメントし、それが
	* ちょうど 0 になった場合は WaitForAll でブロック中のスレッドを
	* 起床させる。
	*/
	void JobExecutor::DecrementTopology()
	{
		if (numberTopologies_.fetch_sub(1, std::memory_order_acq_rel) == 1)
		{
			numberTopologies_.notify_all();
		}
	}

	/**
	* [EN]
	* Dispatches node to the appropriate InvokeXxxTask overload based on
	* its NodeHandle alternative, releases any semaphores it holds,
	* notifies successors that become runnable as a result, and tears
	* down node if it finished synchronously. Continues in a loop
	* (rather than recursing) via cache to process the last
	* newly-runnable successor without growing the call stack.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* node の NodeHandle の選択肢に応じて、適切な InvokeXxxTask
	* オーバーロードへディスパッチし、保持しているセマフォを解放し、
	* その結果実行可能になった後続ノードへ通知し、node が同期的に
	* 完了していれば終端処理する。呼び出しスタックを増やさないよう、
	* cache 経由で最後に新たに実行可能になった後続をループ（再帰では
	* なく）で処理し続ける。
	*/
	void JobExecutor::Invoke(JobWorker& worker, JobNode* node)
	{
#define SC_INVOKE_CONTINUATION() \
		if (cache) \
		{ \
			node = cache; \
			goto beginInvoke; \
		}

	beginInvoke:

		JobNode* cache = nullptr;

		if (node->nstate_ & JobNodeState::PREEMPTED)
		{
			goto invokeTask;
		}

		if (node->IsParentCancelled())
		{
			TearDownInvoke(worker, node, cache);
			SC_INVOKE_CONTINUATION();
			return;
		}

		if (node->semaphores_ && node->semaphores_->acquire_.empty())
		{
			HybridArray<JobNode*> waiters;
			if (!node->AcquireAll(waiters))
			{
				BulkSchedule(worker, waiters.begin(), waiters.size());
				return;
			}
		}

	invokeTask:

		HybridArray<Int> conds;

		switch (node->handle_.index())
		{
		case JobNode::STATIC:
		{
			InvokeStaticTask(worker, node);
		}
		break;
		case JobNode::PREEMPTIVE_RUNTIME:
		{
			if (InvokePreemptiveRuntimeTask(worker, node))
			{
				return;
			}
		}
		break;
		case JobNode::NONPREEMPTIVE_RUNTIME:
		{
			InvokeNonpreemptiveRuntimeTask(worker, node);
		}
		break;
		case JobNode::SUBFLOW:
		{
			if (InvokeSubflowTask(worker, node))
			{
				return;
			}
		}
		break;
		case JobNode::SINGLE_CONDITION:
		{
			InvokeSingleConditionTask(worker, node, conds);
		}
		break;
		case JobNode::MULTI_CONDITION:
		{
			InvokeMultiConditionTask(worker, node, conds);
		}
		break;
		case JobNode::OWNED_MODULE:
		{
			if (InvokeOwnedModuleTask(worker, node))
			{
				return;
			}
		}
		break;
		case JobNode::ADOPTED_MODULE:
		{
			if (InvokeAdoptedModuleTask(worker, node))
			{
				return;
			}
		}
		break;
		default:
			break;
		}

		if (node->semaphores_ && !node->semaphores_->release_.empty())
		{
			HybridArray<JobNode*> waiters;
			node->ReleaseAll(waiters);
			BulkSchedule(worker, waiters.begin(), waiters.size());
		}

		node->joinCounter_.fetch_add(node->nstate_ & JobNodeState::STRONG_DEPENDENCIES_MASK, std::memory_order_relaxed);

		switch (node->handle_.index())
		{
		case JobNode::SINGLE_CONDITION:
			[[fallthrough]];
		case JobNode::MULTI_CONDITION:
		{
			for (auto cond : conds)
			{
				if (cond >= 0 && static_cast<Size>(cond) < node->numberSuccessors_)
				{
					auto s = node->edges_[cond];
					s->joinCounter_.store(0, std::memory_order_relaxed);
					node->parent_->joinCounter_.fetch_add(1, std::memory_order_relaxed);
					UpdateCache(worker, cache, s);
				}
			}
		}
		break;
		default:
		{
			for (Size index = 0;index < node->numberSuccessors_;++index)
			{
				if (auto s = node->edges_[index];s->joinCounter_.fetch_sub(1, std::memory_order_acq_rel) == 1)
				{
					node->parent_->joinCounter_.fetch_add(1, std::memory_order_relaxed);
					UpdateCache(worker, cache, s);
				}
			}
		}
		break;
		}

		TearDownNonasync(worker, node, cache);
		SC_INVOKE_CONTINUATION();
	}

	/**
	* [EN]
	* Invokes a Static task's callable.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* Static タスクの呼び出し可能オブジェクトを実行する。
	*/
	void JobExecutor::InvokeStaticTask(JobWorker& worker, JobNode* node)
	{
		std::get_if<JobNode::Static>(&node->handle_)->work_();
	}

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
	void JobExecutor::InvokeNonpreemptiveRuntimeTask(JobWorker& worker, JobNode* node)
	{
		JobNonpreemptiveRuntime nonruntime(*this, worker);
		std::get_if<JobNode::NonpreemptiveRuntime>(&node->handle_)->work_(nonruntime);
	}

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
	void JobExecutor::InvokeSingleConditionTask(JobWorker& worker, JobNode* node, HybridArray<Int>& conds)
	{
		auto& work = std::get_if<JobNode::SingleCondition>(&node->handle_)->work_;
		conds = { work() };
	}

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
	void JobExecutor::InvokeMultiConditionTask(JobWorker& worker, JobNode* node, HybridArray<Int>& conds)
	{
		conds = std::get_if<JobNode::MultiCondition>(&node->handle_)->work_();
	}

	/**
	* [EN]
	* Invokes a Subflow task's builder callable on first entry to
	* construct the subgraph and schedules it; on the resuming call
	* (after the subgraph has finished), clears the subgraph unless it
	* was marked to be retained. Returns whether the node should be
	* considered async (i.e. suspended awaiting the subgraph).
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 初回の呼び出しでは Subflow タスクのビルダー呼び出し可能オブジェクトを
	* 実行してサブグラフを構築し、それをスケジューリングする。再開時の
	* 呼び出し（サブグラフ完了後）では、保持するようマークされていない
	* 限りサブグラフをクリアする。ノードを非同期（すなわちサブグラフの
	* 完了を待って中断中）として扱うべきかどうかを返す。
	*/
	Bool JobExecutor::InvokeSubflowTask(JobWorker& worker, JobNode* node)
	{
		auto& handle = *std::get_if<JobNode::Subflow>(&node->handle_);
		auto& graph = handle.subgraph_;

		if ((node->nstate_ & JobNodeState::PREEMPTED) == 0)
		{
			JobSubflow subflow(*this, worker, node, graph);

			if (subflow.Joinable() && !graph.empty())
			{
				node->nstate_ |= JobNodeState::PREEMPTED;

				ScheduleGraph(worker, graph, node->topology_, node);
				return true;
			}
		}
		else
		{
			node->nstate_ &= ~JobNodeState::PREEMPTED;
		}

		if ((node->nstate_ & JobNodeState::RETAIN_SUBFLOW) == 0)
		{
			graph.clear();
		}

		return false;
	}

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
	Bool JobExecutor::InvokeOwnedModuleTask(JobWorker& worker, JobNode* node)
	{
		return InvokeModuleTaskImplementation(worker, node, std::get_if<JobNode::OwnedModule>(&node->handle_)->graph_);
	}

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
	Bool JobExecutor::InvokeAdoptedModuleTask(JobWorker& worker, JobNode* node)
	{
		return InvokeModuleTaskImplementation(worker, node, std::get_if<JobNode::AdoptedModule>(&node->handle_)->graph_);
	}

	/**
	* [EN]
	* Shared implementation for OwnedModule/AdoptedModule: on first
	* entry schedules graph as node's subgraph; on the resuming call,
	* simply clears the preempted flag. Returns whether the node should
	* be considered async.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* OwnedModule/AdoptedModule の共通実装: 初回の呼び出しでは graph を
	* node のサブグラフとしてスケジューリングする。再開時の呼び出しでは
	* プリエンプトフラグを単純にクリアする。ノードを非同期として扱う
	* べきかどうかを返す。
	*/
	Bool JobExecutor::InvokeModuleTaskImplementation(JobWorker& worker, JobNode* node, JobGraph& graph)
	{
		if (graph.empty())
		{
			return false;
		}

		if ((node->nstate_ & JobNodeState::PREEMPTED) == 0)
		{
			node->nstate_ |= JobNodeState::PREEMPTED;
			ScheduleGraph(worker, graph, node->topology_, node);
			return true;
		}

		node->nstate_ &= ~JobNodeState::PREEMPTED;

		return false;
	}

	/**
	* [EN]
	* Invokes a PreemptiveRuntime task's callable by delegating to
	* InvokeRuntimeTaskImplementation. Returns whether the node should
	* be considered async.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* InvokeRuntimeTaskImplementation へ委譲して PreemptiveRuntime
	* タスクの呼び出し可能オブジェクトを実行する。ノードを非同期として
	* 扱うべきかどうかを返す。
	*/
	Bool JobExecutor::InvokePreemptiveRuntimeTask(JobWorker& worker, JobNode* node)
	{
		return InvokeRuntimeTaskImplementation(worker, node, std::get_if<JobNode::PreemptiveRuntime>(&node->handle_)->work_);
	}

	/**
	* [EN]
	* Invokes function against node's preemptive runtime state: on
	* first entry, marks the node preempted/anchored and calls
	* function(runtime); if the join counter didn't reach zero during
	* the call (i.e. the task scheduled sub-work and is still waiting),
	* keeps the node suspended and returns true. Returns whether the
	* node should be considered async.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* node のプリエンプティブランタイム状態に対して function を実行する:
	* 初回の呼び出しでは、ノードをプリエンプト/アンカー済みとマークし
	* function(runtime) を呼び出す。呼び出し中に join カウンタが 0 に
	* 達しなかった場合（すなわちタスクがサブ処理をスケジューリングし、
	* まだ待機中の場合）は、ノードを中断したままにして true を返す。
	* ノードを非同期として扱うべきかどうかを返す。
	*/
	Bool JobExecutor::InvokeRuntimeTaskImplementation(JobWorker& worker, JobNode* node, std::function<void(JobPreemptiveRuntime&)>& function)
	{
		if ((node->nstate_ & JobNodeState::PREEMPTED) == 0)
		{
			JobPreemptiveRuntime runtime(*this, worker, node);

			node->nstate_ |= (JobNodeState::PREEMPTED | JobNodeState::IMPLICITLY_ANCHORED);

			function(runtime);

			if (node->joinCounter_.fetch_sub(1, std::memory_order_acq_rel) == 1)
			{
				node->nstate_ &= ~(JobNodeState::PREEMPTED | JobNodeState::IMPLICITLY_ANCHORED);
			}
			else
			{
				return true;
			}
		}
		else
		{
			node->nstate_ &= ~(JobNodeState::PREEMPTED | JobNodeState::IMPLICITLY_ANCHORED);
		}

		return false;
	}

	/**
	* [EN]
	* Overload of InvokeRuntimeTaskImplementation for callables that
	* additionally receive a resume/first-call flag: calls
	* function(runtime, false) on first entry (with the join counter
	* pre-incremented to avoid a race with concurrently-scheduled
	* sub-work), then function(runtime, true) once the node's
	* dependents have all completed. Returns whether the node should be
	* considered async.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 再開/初回呼び出しフラグを追加で受け取る呼び出し可能オブジェクト
	* 向けの InvokeRuntimeTaskImplementation オーバーロード: 初回の
	* 呼び出しでは function(runtime, false) を呼ぶ（並行してスケジュール
	* されるサブ処理との競合を避けるため、join カウンタを事前に
	* インクリメントしておく）。ノードの依存先がすべて完了した時点で
	* function(runtime, true) を呼ぶ。ノードを非同期として扱うべきかを
	* 返す。
	*/
	Bool JobExecutor::InvokeRuntimeTaskImplementation(JobWorker& worker, JobNode* node, std::function<void(JobPreemptiveRuntime&, Bool)>& function)
	{
		JobPreemptiveRuntime runtime(*this, worker, node);

		if ((node->nstate_ & JobNodeState::PREEMPTED) == 0)
		{
			node->nstate_ |= (JobNodeState::PREEMPTED | JobNodeState::IMPLICITLY_ANCHORED);
			node->joinCounter_.fetch_add(1, std::memory_order_release);

			function(runtime, false);

			if (node->joinCounter_.fetch_sub(1, std::memory_order_acq_rel) == 1)
			{
				node->nstate_ &= ~(JobNodeState::PREEMPTED | JobNodeState::IMPLICITLY_ANCHORED);
			}
			else
			{
				return true;
			}
		}
		else
		{
			node->nstate_ &= ~(JobNodeState::PREEMPTED | JobNodeState::IMPLICITLY_ANCHORED);
		}

		function(runtime, true);

		return false;
	}

	/**
	* [EN]
	* Captures the exception currently in flight and propagates it up
	* through node's parent chain (flagging each as EXCEPTION), storing
	* it on the nearest implicitly-anchored ancestor that hasn't already
	* caught one, or on node itself if none is found.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 現在発生している例外を捕捉し、node の親チェーンを遡って伝播させ
	* （各ノードへ EXCEPTION フラグを立てる）、まだ例外を捕捉していない
	* 最も近い暗黙的にアンカーされた祖先へ格納する。見つからなければ
	* node 自身へ格納する。
	*/
	void JobExecutor::ProcessException(JobWorker& worker, JobNode* node)
	{
		JobNodeBase* explicitAnchor = node;
		JobNodeBase* implicitAnchor = nullptr;

		while (explicitAnchor)
		{
			explicitAnchor->estate_.fetch_or(JobExceptionState::EXCEPTION, std::memory_order_relaxed);
			if (implicitAnchor == nullptr && (explicitAnchor->nstate_ & JobNodeState::IMPLICITLY_ANCHORED))
			{
				implicitAnchor = explicitAnchor;
			}
			explicitAnchor = explicitAnchor->parent_;
		}

		constexpr static auto flag = JobExceptionState::EXCEPTION | JobExceptionState::CAUGHT;

		if (implicitAnchor)
		{
			if ((implicitAnchor->estate_.fetch_or(flag, std::memory_order_relaxed) & JobExceptionState::CAUGHT) == 0)
			{
				implicitAnchor->exceptionPtr_ = std::current_exception();
				return;
			}
		}

		node->exceptionPtr_ = std::current_exception();
	}

	/**
	* [EN]
	* Updates cache with node, first scheduling whatever was previously
	* cached (see Schedule).
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* cache を node で更新する。その前に、以前 cache されていたものを
	* スケジューリングする（Schedule を参照）。
	*/
	void JobExecutor::UpdateCache(JobWorker& worker, JobNode*& cache, JobNode* node)
	{
		if (cache)
		{
			Schedule(worker, cache);
		}
		cache = node;
	}

	/**
	* [EN]
	* Runs graph to completion synchronously on worker (used by Corun
	* and subflow execution), under topology/parent: schedules its
	* initially-runnable nodes, then helps process other scheduled work
	* until parent's dependents have all completed.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* worker 上で graph を同期的に完了まで実行する（Corun やサブフロー
	* 実行で使用される）。topology/parent のもとで、初期時点で実行可能な
	* ノードをスケジューリングし、parent の依存先がすべて完了するまで
	* 他のスケジュール済み処理の実行を手伝う。
	*/
	void JobExecutor::CorunGraph(JobWorker& worker, JobGraph& graph, JobTopology* topology, JobNodeBase* parent)
	{
		if (graph.empty())
		{
			return;
		}

		{
			ScheduleGraph(worker, graph, topology, parent);
			CorunUntil(worker, [parent]()->Bool {return parent->joinCounter_.load(std::memory_order_acquire) == 0;});
		}
	}

	/**
	* [EN]
	* Blocks worker until a task becomes available (parking it via the
	* notifier if necessary), storing it in cache. Returns whether a
	* task was obtained (false typically means the executor is shutting down).
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* タスクが利用可能になるまで worker をブロックし（必要なら notifier
	* 経由でパークする）、cache へ格納する。タスクを取得できたかどうかを
	* 返す（false は通常、エグゼキュータがシャットダウン中であることを
	* 意味する）。
	*/
	Bool JobExecutor::WaitForTask(JobWorker& worker, JobNode*& cache)
	{
	exploreTask:

		if (ExploreTask(worker, cache) == false)
		{
			return false;
		}

		if (cache)
		{
			return true;
		}

		notifier_.prepare_wait(worker.id_);

		if (numberTopologies_.load(std::memory_order_relaxed) == 0)
		{
			if (worker.done_.test(std::memory_order_relaxed))
			{
				notifier_.cancel_wait(worker.id_);
				return false;
			}
			notifier_.commit_wait(worker.id_);
			goto exploreTask;
		}

		for (Size buffer = 0;buffer < buffers_.size();++buffer)
		{
			if (!buffers_[buffer].queue_.empty())
			{
				notifier_.cancel_wait(worker.id_);
				worker.stickyVictim_ = buffer + workers_.size();
				goto exploreTask;
			}
		}

		for (Size workerIndex = 0;workerIndex < workers_.size() - 1;++workerIndex)
		{
			if (Size victim = workerIndex + (workerIndex > worker.id_);!workers_[victim].wsq_.empty())
			{
				notifier_.cancel_wait(worker.id_);
				worker.stickyVictim_ = victim;
				goto exploreTask;
			}
		}

		if (worker.done_.test(std::memory_order_relaxed))
		{
			notifier_.cancel_wait(worker.id_);
			return false;
		}

		notifier_.commit_wait(worker.id_);
		goto exploreTask;
	}
}
