#include <FoundationEngine/JobSystem/FlowBuilder.h>
#include <FoundationEngine/JobSystem/JobExecutor.h>
#include <FoundationEngine/Log/Exeption.h>

namespace SeedCore
{
	/**
	* [EN]
	* Constructs a builder that creates/modifies tasks in graph.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* graph 内のタスクを生成・変更するビルダーを構築する。
	*/
	FlowBuilder::FlowBuilder(JobGraph& graph) :graph_(graph)
	{
		/// No Code
	}

	/**
	* [EN]
	* Removes task from the graph.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* task をグラフから削除する。
	*/
	void FlowBuilder::Erase(JobTask task)
	{
		if (!task.node_)
		{
			return;
		}

		/// [EN] Unlink task from every successor's predecessor list (the front portion of edges_).
		/// [JP] task を各後続の先行一覧（edges_ の前方部分）から切り離す。
		for (Size index = 0;index < task.node_->numberSuccessors_;++index)
		{
			task.node_->edges_[index]->RemovePredecessors(task.node_);
		}

		/// [EN] Unlink task from every predecessor's successor list (the remaining portion of edges_).
		/// [JP] task を各先行の後続一覧（edges_ の残り部分）から切り離す。
		for (Size index = task.node_->numberSuccessors_;index < task.node_->edges_.size();++index)
		{
			task.node_->edges_[index]->RemoveSuccessors(task.node_);
		}

		graph_.erase(task.node_);
	}

	/**
	* [EN]
	* Creates a new module task that takes ownership of graph (moved in)
	* and returns a handle to it.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* graph の所有権を（ムーブで）引き受ける新しいモジュールタスクを
	* 生成し、そのハンドルを返す。
	*/
	JobTask FlowBuilder::Adopt(JobGraph&& graph)
	{
		return JobTask(graph_.emplace_back(JobNodeState::NONE, JobExceptionState::NONE, DefaultTaskParams{}, nullptr, nullptr, 0, std::in_place_type_t<JobNode::AdoptedModule>{}, std::move(graph)));
	}

	/**
	* [EN]
	* Creates a new task with no assigned work (a placeholder) and
	* returns a handle to it.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 処理が割り当てられていない新しいタスク（プレースホルダー）を
	* 生成し、そのハンドルを返す。
	*/
	JobTask FlowBuilder::Placeholder()
	{
		auto node = graph_.emplace_back(JobNodeState::NONE, JobExceptionState::NONE, DefaultTaskParams{}, nullptr, nullptr, 0, std::in_place_type_t<JobNode::Placeholder>{});
		return JobTask(node);
	}

	/**
	* [EN]
	* Chains every task in keys into a straight-line sequence,
	* establishing a precedence edge between each consecutive pair.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* keys の各タスクを直線的な順序に連結し、連続する各ペアの間に
	* 先行関係エッジを設定する。
	*/
	void FlowBuilder::Linearize(DynamicArray<JobTask>& keys)
	{
		this->Linearize<DynamicArray<JobTask>>(keys);
	}

	/**
	* [EN]
	* Chains every task in keys into a straight-line sequence, from an
	* initializer list.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* keys の各タスクを直線的な順序に連結する。初期化子リストから受け取る。
	*/
	void FlowBuilder::Linearize(std::initializer_list<JobTask>& keys)
	{
		this->Linearize<std::initializer_list<JobTask>>(keys);
	}
}

namespace SeedCore
{
	/**
	* [EN]
	* Constructs a subflow builder for node's subgraph, running under
	* executor/worker, and clears the graph as a fresh start for this run.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* executor/worker のもとで実行される、node のサブグラフに対する
	* サブフロービルダーを構築し、この実行のための新規状態として
	* graph をクリアする。
	*/
	JobSubflow::JobSubflow(JobExecutor& executor, JobWorker& worker, JobNode* node, JobGraph& graph) :FlowBuilder(graph), executor_(executor), worker_(worker), node_(node)
	{
		/// [EN] Clear any joined/retain flags left over from a previous run of this node's subflow.
		/// [JP] このノードのサブフローの前回実行から残っている、joined/retain フラグをクリアする。
		node_->nstate_ &= ~(JobNodeState::JOINED_SUBFLOW | JobNodeState::RETAIN_SUBFLOW);

		/// [EN] Start with an empty subgraph so the user's builder callable populates it fresh each time.
		/// [JP] 空のサブグラフから開始し、ユーザーのビルダー呼び出し可能オブジェクトが毎回新規に構築できるようにする。
		graph.clear();
	}

	/**
	* [EN]
	* Blocks until this subflow's subgraph finishes executing.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* このサブフローのサブグラフの実行が完了するまでブロックする。
	*/
	void JobSubflow::Join()
	{
		if (!Joinable())
		{
			SC_THROW("", true);
		}

		/// [EN] Synchronously run the subgraph to completion on the calling worker, helping process other work while waiting.
		/// [JP] 呼び出し元のワーカー上でサブグラフを同期的に完了まで実行する。待機中は他の処理を手伝う。
		executor_.CorunGraph(worker_, graph_, node_->topology_, node_);

		node_->nstate_ |= JobNodeState::JOINED_SUBFLOW;
	}

	/**
	* [EN]
	* Returns whether this subflow's subgraph is currently joinable (has
	* not already been joined).
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* このサブフローのサブグラフが現在 join 可能か（まだ join されて
	* いないか）を返す。
	*/
	Bool JobSubflow::Joinable()const noexcept
	{
		return !(node_->nstate_ & JobNodeState::JOINED_SUBFLOW);
	}

	/**
	* [EN]
	* Returns a reference to the JobExecutor running this subflow.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* このサブフローを実行している JobExecutor への参照を返す。
	*/
	JobExecutor& JobSubflow::Executor()noexcept
	{
		return executor_;
	}

	/**
	* [EN]
	* Returns a reference to this subflow's underlying graph.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* このサブフローの内部グラフへの参照を返す。
	*/
	JobGraph& JobSubflow::Graph()
	{
		return graph_;
	}

	/**
	* [EN]
	* Sets whether this subflow's subgraph should be retained instead of
	* being cleared after it finishes.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* このサブフローのサブグラフを、完了後にクリアするのではなく
	* 保持するかどうかを設定する。
	*/
	void JobSubflow::Retain(Bool flag)noexcept
	{
		if (flag)
		{
			node_->nstate_ |= JobNodeState::RETAIN_SUBFLOW;
		}
		else
		{
			node_->nstate_ |= ~JobNodeState::RETAIN_SUBFLOW;
		}
	}

	/**
	* [EN]
	* Returns whether this subflow's subgraph is currently set to be retained.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* このサブフローのサブグラフが現在保持される設定になっているかを
	* 返す。
	*/
	Bool JobSubflow::Retain()const
	{
		return node_->nstate_ & JobNodeState::RETAIN_SUBFLOW;
	}
}
