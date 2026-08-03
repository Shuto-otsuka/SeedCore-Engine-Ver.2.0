#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/JobSystem/JobDeclaretions.h>
#include <FoundationEngine/JobSystem/JobGraph.h>
#include <FoundationEngine/JobSystem/JobTask.h>

namespace SeedCore
{
	/**
	* [EN]
	* Public graph-construction API for a JobGraph: creates tasks
	* (dispatching to the right JobNode::NodeHandle alternative based on
	* the callable's signature), wires their dependencies, and manages
	* removal/linearization. JobTaskflow derives from this to expose
	* graph building; JobSubflow also derives from it to let a task
	* build its own nested subgraph at runtime.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* JobGraph に対する、公開されたグラフ構築 API。呼び出し可能
	* オブジェクトのシグネチャに応じて適切な JobNode::NodeHandle の
	* 選択肢へディスパッチしながらタスクを生成し、それらの依存関係を
	* 配線し、削除・直列化を管理する。JobTaskflow はこれを継承して
	* グラフ構築機能を公開する。JobSubflow も同様にこれを継承し、
	* タスクが実行時に自身のネストされたサブグラフを構築できるようにする。
	*/
	class FlowBuilder
	{
	private:
		friend class JobExecutor;

	public:
		/**
		* [EN]
		* Constructs a builder that creates/modifies tasks in graph.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* graph 内のタスクを生成・変更するビルダーを構築する。
		*/
		FlowBuilder(JobGraph& graph);

		/**
		* [EN]
		* Creates a new Static task from callable (a parameterless,
		* void-returning callable) and returns a handle to it.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* callable（引数なしで void を返す呼び出し可能オブジェクト）から
		* 新しい Static タスクを生成し、そのハンドルを返す。
		*/
		template<StaticTaskLike C>
		JobTask emplace(C&& callable)
		{
			return JobTask(graph_.emplace_back(JobNodeState::NONE, JobExceptionState::NONE, DefaultTaskParams{}, nullptr, nullptr, 0, std::in_place_type_t<JobNode::Static>{}, std::forward<C>(callable)));
		}

		/**
		* [EN]
		* Creates a new Runtime task from callable, selecting the
		* PreemptiveRuntime or NonpreemptiveRuntime alternative based on
		* which runtime reference type callable accepts, and returns a
		* handle to it.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* callable から新しい Runtime タスクを生成する。callable が
		* 受け取るランタイム参照の型に応じて PreemptiveRuntime または
		* NonpreemptiveRuntime の選択肢を選び、そのハンドルを返す。
		*/
		template<RuntimeTaskLike C>
		JobTask emplace(C&& callable)
		{
			if constexpr (std::is_invocable_v<C, JobPreemptiveRuntime&>)
			{
				return JobTask(graph_.emplace_back(JobNodeState::NONE, JobExceptionState::NONE, DefaultTaskParams{}, nullptr, nullptr, 0, std::in_place_type_t<JobNode::PreemptiveRuntime>{}, std::forward<C>(callable)));
			}
			else if constexpr (std::is_invocable_v<C, JobNonpreemptiveRuntime&>)
			{
				return JobTask(graph_.emplace_back(JobNodeState::NONE, JobExceptionState::NONE, DefaultTaskParams{}, nullptr, nullptr, 0, std::in_place_type_t<JobNode::NonpreemptiveRuntime>{}, std::forward<C>(callable)));
			}
		}

		/**
		* [EN]
		* Creates a new Subflow task from callable (which receives a
		* Subflow& builder to construct the nested graph at runtime) and
		* returns a handle to it.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* callable（実行時にネストされたグラフを構築するための Subflow&
		* ビルダーを受け取る）から新しい Subflow タスクを生成し、その
		* ハンドルを返す。
		*/
		template<SubflowTaskLike C>
		JobTask emplace(C&& callable)
		{
			return JobTask(graph_.emplace_back(JobNodeState::NONE, JobExceptionState::NONE, DefaultTaskParams{}, nullptr, nullptr, 0, std::in_place_type_t<JobNode::Subflow>{}, std::forward<C>(callable)));
		}

		/**
		* [EN]
		* Creates a new SingleCondition task from callable (returns the
		* index of the single successor to follow) and returns a handle to it.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* callable（辿るべき単一の後続インデックスを返す）から新しい
		* SingleCondition タスクを生成し、そのハンドルを返す。
		*/
		template<SingleConditionTaskLike C>
		JobTask emplace(C&& callable)
		{
			return JobTask(graph_.emplace_back(JobNodeState::NONE, JobExceptionState::NONE, DefaultTaskParams{}, nullptr, nullptr, 0, std::in_place_type_t<JobNode::SingleCondition>{}, std::forward<C>(callable)));
		}

		/**
		* [EN]
		* Creates a new MultiCondition task from callable (returns the
		* indices of every successor to follow) and returns a handle to it.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* callable（辿るべきすべての後続インデックスを返す）から新しい
		* MultiCondition タスクを生成し、そのハンドルを返す。
		*/
		template<MultiConditionTaskLike C>
		JobTask emplace(C&& callable)
		{
			return JobTask(graph_.emplace_back(JobNodeState::NONE, JobExceptionState::NONE, DefaultTaskParams{}, nullptr, nullptr, 0, std::in_place_type_t<JobNode::MultiCondition>{}, std::forward<C>(callable)));
		}

		/**
		* [EN]
		* Creates one task per entry in callables (each dispatched via
		* the single-callable emplace overloads) and returns them as a tuple.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* callables の各要素に対して 1 つずつタスクを生成し（それぞれ単一の
		* callable 版 emplace オーバーロード経由でディスパッチされる）、
		* それらをタプルとして返す。
		*/
		template<typename... C>
			requires(sizeof...(C) > 1)
		auto emplace(C&&... callables)
		{
			return std::make_tuple(emplace(std::forward<C>(callables))...);
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
		void Erase(JobTask task);

		/**
		* [EN]
		* Creates a new module task wrapping callable's underlying graph
		* (owned externally) and returns a handle to it.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* callable の内部グラフ（外部が所有）を包む新しいモジュールタスクを
		* 生成し、そのハンドルを返す。
		*/
		template<GraphLike C>
		JobTask Composed(C& callable)
		{
			return JobTask(graph_.emplace_back(JobNodeState::NONE, JobExceptionState::NONE, DefaultTaskParams{}, nullptr, nullptr, 0, std::in_place_type_t<JobNode::OwnedModule>{}, std::forward<C>(callable)));
		}

		/**
		* [EN]
		* Creates a new module task that takes ownership of graph (moved
		* in) and returns a handle to it.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* graph の所有権を（ムーブで）引き受ける新しいモジュールタスクを
		* 生成し、そのハンドルを返す。
		*/
		JobTask Adopt(JobGraph&& graph);

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
		JobTask Placeholder();

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
		void Linearize(DynamicArray<JobTask>& keys);

		/**
		* [EN]
		* Chains every task in keys into a straight-line sequence (see
		* the DynamicArray overload), from an initializer list.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* keys の各タスクを直線的な順序に連結する（DynamicArray
		* オーバーロードを参照）。初期化子リストから受け取る。
		*/
		void Linearize(std::initializer_list<JobTask>& keys);

	protected:
		/// [EN] The graph this builder creates/modifies tasks in.
		/// [JP] このビルダーがタスクを生成・変更する対象のグラフ。
		JobGraph& graph_;

	private:
		/**
		* [EN]
		* Shared implementation for Linearize: chains any range-like
		* container of JobTask into a straight-line sequence.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* Linearize の共通実装: JobTask の任意の範囲的コンテナを直線的な
		* 順序に連結する。
		*/
		template<typename L>
		void Linearize(L& keys)
		{
			auto iterator = keys.begin();
			auto end = keys.end();

			if (iterator == end)
			{
				return;
			}

			auto next = iterator;

			for (++next;next != end;++next, ++iterator)
			{
				iterator->node_->Precede(next->node_);
			}
		}
	};

	/**
	* [EN]
	* FlowBuilder specialization passed to a Subflow task's callable at
	* runtime, letting it build the task's nested subgraph and control
	* whether that subgraph joins (waits for completion) immediately or
	* is retained for later reuse.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* Subflow タスクの呼び出し可能オブジェクトへ実行時に渡される
	* FlowBuilder の特殊化。タスクのネストされたサブグラフを構築し、
	* そのサブグラフを即座に join（完了を待機）するか、後で再利用する
	* ために保持するかを制御できるようにする。
	*/
	class JobSubflow :public FlowBuilder
	{
	private:
		friend class JobExecutor;
		friend class FlowBuilder;

	public:
		/**
		* [EN]
		* Blocks until this subflow's subgraph finishes executing.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* このサブフローのサブグラフの実行が完了するまでブロックする。
		*/
		void Join();

		/**
		* [EN]
		* Returns whether this subflow's subgraph is currently joinable
		* (has not already been joined).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* このサブフローのサブグラフが現在 join 可能か（まだ join されて
		* いないか）を返す。
		*/
		Bool Joinable()const noexcept;

		/**
		* [EN]
		* Returns a reference to the JobExecutor running this subflow.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* このサブフローを実行している JobExecutor への参照を返す。
		*/
		JobExecutor& Executor()noexcept;

		/**
		* [EN]
		* Returns a reference to this subflow's underlying graph.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* このサブフローの内部グラフへの参照を返す。
		*/
		JobGraph& Graph();

		/**
		* [EN]
		* Sets whether this subflow's subgraph should be retained (kept
		* intact for reuse on the next execution) instead of being
		* cleared after it finishes.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* このサブフローのサブグラフを、完了後にクリアするのではなく
		* 保持する（次回実行時の再利用のためそのまま残す）かどうかを
		* 設定する。
		*/
		void Retain(Bool flag)noexcept;

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
		Bool Retain()const;

	private:
		/**
		* [EN]
		* Constructs a subflow builder for node's subgraph, running under
		* executor/worker.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* executor/worker のもとで実行される、node のサブグラフに対する
		* サブフロービルダーを構築する。
		*/
		JobSubflow(JobExecutor& executor, JobWorker& worker, JobNode* node, JobGraph& graph);

		/**
		* [EN]
		* Default construction is disabled: a JobSubflow must always be
		* bound to an executor, worker, and node.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* デフォルト構築は禁止されている: JobSubflow は常に executor、
		* worker、node に紐づいている必要がある。
		*/
		JobSubflow() = delete;

		/**
		* [EN]
		* Copy construction is disabled.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* コピー構築は禁止されている。
		*/
		JobSubflow(const JobSubflow&) = delete;

		/**
		* [EN]
		* Move construction is disabled.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* ムーブ構築は禁止されている。
		*/
		JobSubflow(JobSubflow&&) = delete;

		/// [EN] The executor running this subflow.
		/// [JP] このサブフローを実行しているエグゼキュータ。
		JobExecutor& executor_;

		/// [EN] The worker thread currently executing this subflow.
		/// [JP] このサブフローを現在実行しているワーカースレッド。
		JobWorker& worker_;

		/// [EN] The Subflow-type node that owns this subflow's subgraph.
		/// [JP] このサブフローのサブグラフを所有する、Subflow 種別のノード。
		JobNode* node_;
	};
}
