#include <FoundationEngine/JobSystem/JobTaskflow.h>

namespace SeedCore
{
	/**
	* [EN]
	* Constructs an empty taskflow with the given display name.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 指定された表示名で、空のタスクフローを構築する。
	*/
	JobTaskflow::JobTaskflow(const String& name) :FlowBuilder(graph_), name_(name)
	{
		/// No Code
	}

	/**
	* [EN]
	* Constructs an empty taskflow with no name.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 名前を持たない、空のタスクフローを構築する。
	*/
	JobTaskflow::JobTaskflow() :FlowBuilder(graph_)
	{
		/// No Code
	}

	/**
	* [EN]
	* Move-constructs, transferring rhs's graph, name, and pending topologies.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* rhs のグラフ、名前、保留中のトポロジーを移譲してムーブ構築する。
	*/
	JobTaskflow::JobTaskflow(JobTaskflow&& rhs) :FlowBuilder(graph_)
	{
		/// [EN] Lock rhs's mutex before reading its members, since another thread (e.g. an executor) may be concurrently enqueueing topologies onto it.
		/// [JP] rhs のメンバを読み取る前にその mutex_ をロックする。別スレッド（エグゼキュータなど）が並行してトポロジーを追加登録している可能性があるため。
		std::scoped_lock<std::mutex> lock(rhs.mutex_);
		name_ = std::move(rhs.name_);
		graph_ = std::move(rhs.graph_);
		topologies_ = std::move(rhs.topologies_);
	}

	/**
	* [EN]
	* Move-assigns, transferring rhs's graph, name, and pending topologies.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* rhs のグラフ、名前、保留中のトポロジーを移譲してムーブ代入する。
	*/
	JobTaskflow& JobTaskflow::operator=(JobTaskflow&& rhs)
	{
		if (this != &rhs)
		{
			/// [EN] Lock both mutexes together (deadlock-safe ordering via std::scoped_lock) since either side could be concurrently accessed by an executor thread.
			/// [JP] 両方の mutex_ を同時にロックする（std::scoped_lock によるデッドロック安全な順序）。どちらの側もエグゼキュータスレッドから並行アクセスされ得るため。
			std::scoped_lock<std::mutex, std::mutex> lock(mutex_, rhs.mutex_);
			name_ = std::move(rhs.name_);
			graph_ = std::move(rhs.graph_);
			topologies_ = std::move(rhs.topologies_);
		}
		return *this;
	}

	/**
	* [EN]
	* Returns the number of tasks (nodes) currently in the graph.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* グラフ内に現在存在するタスク（ノード）の数を返す。
	*/
	Size JobTaskflow::NumberTasks()const
	{
		return graph_.size();
	}

	/**
	* [EN]
	* Returns whether the graph currently has no tasks.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* グラフが現在タスクを持たないかどうかを返す。
	*/
	Bool JobTaskflow::Empty()const
	{
		return graph_.empty();
	}

	/**
	* [EN]
	* Sets the taskflow's display name.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* タスクフローの表示名を設定する。
	*/
	void JobTaskflow::Name(const String& name)
	{
		name_ = name;
	}

	/**
	* [EN]
	* Returns the taskflow's display name.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* タスクフローの表示名を返す。
	*/
	const String& JobTaskflow::Name()const
	{
		return name_;
	}

	/**
	* [EN]
	* Removes every task from the graph.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* グラフからすべてのタスクを削除する。
	*/
	void JobTaskflow::Clear()
	{
		graph_.clear();
	}

	/**
	* [EN]
	* Removes the precedence edge between from and to.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* from と to の間の先行関係エッジを削除する。
	*/
	void JobTaskflow::RemoveDependency(JobTask from, JobTask to)
	{
		/// [EN] Remove the edge from both sides: from's successor list and to's predecessor list.
		/// [JP] 両側からエッジを削除する: from の後続一覧と to の先行一覧の両方から。
		from.node_->RemoveSuccessors(to.node_);

		to.node_->RemovePredecessors(from.node_);
	}

	/**
	* [EN]
	* Returns a reference to the underlying JobGraph.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 内部の JobGraph への参照を返す。
	*/
	JobGraph& JobTaskflow::Graph()
	{
		return graph_;
	}

	/**
	* [EN]
	* Enqueues topologies for execution and returns the resulting queue size.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* topologies を実行キューへ追加し、追加後のキューサイズを返す。
	*/
	Size JobTaskflow::FetchEnqueue(ResourceRef<JobTopology> topologies)
	{
		std::lock_guard<std::mutex> lock(mutex_);

		/// [EN] Capture the size before insertion: the caller uses 0 to mean "this was the only pending topology, so schedule it immediately."
		/// [JP] 挿入前のサイズを取得する。呼び出し側は 0 を「これが唯一の保留中トポロジーだったので即座にスケジューリングする」という意味で使う。
		auto preSize = topologies_.size();
		topologies_.emplace(std::move(topologies));
		return preSize;
	}
}
