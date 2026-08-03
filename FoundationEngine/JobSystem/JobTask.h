#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/JobSystem/JobDeclaretions.h>
#include <FoundationEngine/JobSystem/JobConcept.h>
#include <FoundationEngine/JobSystem/JobGraph.h>
#include <FoundationEngine/JobSystem/JobNode.h>

namespace SeedCore
{
	/// [EN] Identifies the kind of work a JobTask/JobNode performs, mirroring JobNode::NodeHandle's variant alternatives.
	/// [JP] JobTask/JobNode が行う処理の種類を表す。JobNode::NodeHandle のバリアントの選択肢に対応する。
	enum class JobTaskType :Int
	{
		/// [EN] No work has been assigned yet.
		/// [JP] まだ処理が割り当てられていない。
		PLACEHOLDER = 0,

		/// [EN] A plain static (parameterless) callable.
		/// [JP] 単純な静的（引数なし）の呼び出し可能オブジェクト。
		STATIC,

		/// [EN] Work that runs under a preemptive or non-preemptive runtime.
		/// [JP] プリエンプティブ、または非プリエンプティブなランタイム上で実行される処理。
		RUNTIME,

		/// [EN] A dynamically-built subflow.
		/// [JP] 動的に構築されるサブフロー。
		SUBFLOW,

		/// [EN] A conditional branch (single or multi successor selection).
		/// [JP] 条件分岐（単一または複数の後続選択）。
		CONDITION,

		/// [EN] A module wrapping an owned or adopted JobGraph.
		/// [JP] 所有または養子化された JobGraph を包むモジュール。
		MODULE,

		/// [EN] Sentinel marking an invalid/unrecognized task type.
		/// [JP] 無効・未認識のタスク種別を表す番兵値。
		UNDEFINED
	};

	/// [EN] Enumerates every concrete (non-sentinel) JobTaskType value, for iteration/lookup.
	/// [JP] （番兵値を除く）すべての具体的な JobTaskType 値を列挙する。走査・検索に用いる。
	inline constexpr auto JOB_TASK_TYPES = std::to_array<JobTaskType>
		({
			JobTaskType::PLACEHOLDER,
			JobTaskType::STATIC,
			JobTaskType::RUNTIME,
			JobTaskType::SUBFLOW,
			JobTaskType::CONDITION,
			JobTaskType::MODULE,
		});

	/**
	* [EN]
	* Returns the display name of type as a null-terminated string
	* (defined elsewhere).
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* type の表示名をヌル終端文字列として返す（定義は別の場所にある）。
	*/
	inline const Char* ToString(JobTaskType type);

	/// [EN] Satisfied when C is invocable with no arguments and returns void (a Static task callable).
	/// [JP] C が引数なしで呼び出し可能で void を返す場合に満たされる（Static タスクの呼び出し可能条件）。
	template<typename C>
	concept StaticTaskLike = std::invocable<C> && std::same_as<std::invoke_result_t<C>, void>;

	/// [EN] Compile-time boolean value mirroring StaticTaskLike.
	/// [JP] StaticTaskLike をミラーするコンパイル時のブール値。
	template<typename C>
	constexpr Bool IsStaticTaskValue = StaticTaskLike<C>;

	/// [EN] Satisfied when C is invocable with a Subflow& and returns void (a Subflow task callable).
	/// [JP] C が Subflow& を受け取って呼び出し可能で void を返す場合に満たされる（Subflow タスクの呼び出し可能条件）。
	template<typename C>
	concept SubflowTaskLike = std::invocable<C, Subflow&>&& std::same_as<std::invoke_result_t<C, Subflow&>, void>;

	/// [EN] Compile-time boolean value mirroring SubflowTaskLike.
	/// [JP] SubflowTaskLike をミラーするコンパイル時のブール値。
	template<typename C>
	constexpr Bool IsSubflowTaskValue = SubflowTaskLike<C>;

	/// [EN] Satisfied when C is invocable with a JobPreemptiveRuntime& or JobNonpreemptiveRuntime& and returns void (a Runtime task callable).
	/// [JP] C が JobPreemptiveRuntime& または JobNonpreemptiveRuntime& を受け取って呼び出し可能で void を返す場合に満たされる（Runtime タスクの呼び出し可能条件）。
	template<typename C>
	concept RuntimeTaskLike =
		(std::invocable<C, JobPreemptiveRuntime&> && std::same_as<std::invoke_result_t<C, JobPreemptiveRuntime&>, void>) ||
		(std::invocable<C, JobNonpreemptiveRuntime&> && std::same_as<std::invoke_result_t<C, JobNonpreemptiveRuntime&>, void>);

	/// [EN] Compile-time boolean value mirroring RuntimeTaskLike.
	/// [JP] RuntimeTaskLike をミラーするコンパイル時のブール値。
	template<typename C>
	constexpr Bool IsRuntimeTaskValue = RuntimeTaskLike<C>;

	/// [EN] Satisfied when C is invocable with no arguments and returns something convertible to Int (a SingleCondition task callable).
	/// [JP] C が引数なしで呼び出し可能で、戻り値が Int に変換可能な場合に満たされる（SingleCondition タスクの呼び出し可能条件）。
	template<typename C>
	concept SingleConditionTaskLike = std::invocable<C> && std::convertible_to<std::invoke_result_t<C>, Int>;

	/// [EN] Compile-time boolean value mirroring SingleConditionTaskLike.
	/// [JP] SingleConditionTaskLike をミラーするコンパイル時のブール値。
	template<typename C>
	constexpr Bool IsSingleConditionTaskValue = SingleConditionTaskLike<C>;

	/// [EN] Satisfied when C is invocable with no arguments and returns HybridArray<Int> (a MultiCondition task callable).
	/// [JP] C が引数なしで呼び出し可能で HybridArray<Int> を返す場合に満たされる（MultiCondition タスクの呼び出し可能条件）。
	template<typename C>
	concept MultiConditionTaskLike = std::invocable<C> && std::same_as<std::invoke_result_t<C>, HybridArray<Int>>;

	/// [EN] Compile-time boolean value mirroring MultiConditionTaskLike.
	/// [JP] MultiConditionTaskLike をミラーするコンパイル時のブール値。
	template<typename C>
	constexpr Bool IsMultiConditionTaskValue = MultiConditionTaskLike<C>;

	/**
	* [EN]
	* Lightweight, copyable handle to a single JobNode owned by a
	* JobGraph. Provides the public graph-building API: assigning work,
	* wiring dependencies, attaching semaphores, and querying execution
	* state. Does not own the underlying node; the node's lifetime is
	* managed by its owning JobGraph.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* JobGraph が所有する単一の JobNode への、軽量でコピー可能な
	* ハンドル。処理の割り当て、依存関係の配線、セマフォの付与、実行状態の
	* 問い合わせといった、公開されたグラフ構築 API を提供する。内部の
	* ノード自体は所有しない。ノードのライフタイムは、それを所有する
	* JobGraph によって管理される。
	*/
	class SEEDCORE_API JobTask
	{
	private:
		friend class FlowBuilder;
		friend class JobPreemptiveRuntime;
		friend class JobNonpreemptiveRuntime;
		friend class JobTaskflow;
		friend class JobTaskView;
		friend class JobExecutor;

	public:
		/**
		* [EN]
		* Default constructor: creates a handle pointing at no node.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* デフォルトコンストラクタ: どのノードも指さないハンドルを生成する。
		*/
		JobTask() = default;

		/**
		* [EN]
		* Copy-constructs, pointing at the same underlying node as rhs.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* rhs と同じ内部ノードを指すようにコピー構築する。
		*/
		JobTask(const JobTask& rhs);

		/**
		* [EN]
		* Copy-assigns, pointing at the same underlying node as rhs.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* rhs と同じ内部ノードを指すようにコピー代入する。
		*/
		JobTask& operator=(const JobTask& rhs);

		/**
		* [EN]
		* Resets this handle to point at no node.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* このハンドルをどのノードも指さない状態にリセットする。
		*/
		JobTask& operator=(std::nullptr_t null);

		/**
		* [EN]
		* Returns whether this and rhs refer to the same underlying node.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* this と rhs が同じ内部ノードを指しているかどうかを返す。
		*/
		Bool operator==(const JobTask& rhs)const;

		/**
		* [EN]
		* Negation of operator==.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* operator== の否定。
		*/
		Bool operator!=(const JobTask& rhs)const;

		/**
		* [EN]
		* Returns the underlying node's display name.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 内部ノードの表示名を返す。
		*/
		const String& Name()const;

		/**
		* [EN]
		* Returns the number of successor tasks connected to this task.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* このタスクに接続されている後続タスクの数を返す。
		*/
		Size NumberSuccessors()const;

		/**
		* [EN]
		* Returns the number of predecessor tasks connected to this task.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* このタスクに接続されている先行タスクの数を返す。
		*/
		Size NumberPredecessors()const;

		/**
		* [EN]
		* Returns the number of strong (unconditional) dependencies this task has.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* このタスクが持つ強い（無条件の）依存関係の数を返す。
		*/
		Size NumberStrongDependencies()const;

		/**
		* [EN]
		* Returns the number of weak (conditional-branch-only) dependencies this task has.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* このタスクが持つ弱い（条件分岐経由のみの）依存関係の数を返す。
		*/
		Size NumberWeakDependencies()const;

		/**
		* [EN]
		* Sets the underlying node's display name and returns *this for chaining.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 内部ノードの表示名を設定し、メソッドチェーン用に *this を返す。
		*/
		JobTask& Name(const String& name);

		/**
		* [EN]
		* Assigns callable as this task's work, selecting the appropriate
		* NodeHandle alternative (Static/Runtime/Subflow/SingleCondition/
		* MultiCondition) based on which concept callable satisfies.
		* Returns *this for chaining.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* callable が満たすコンセプトに応じて適切な NodeHandle の選択肢
		* （Static/Runtime/Subflow/SingleCondition/MultiCondition）を選び、
		* このタスクの処理として割り当てる。メソッドチェーン用に *this を返す。
		*/
		template<typename C>
		JobTask& Work(C&& callable)
		{
			if constexpr (IsStaticTaskValue<C>)
			{
				node_->handle_.emplace<JobNode::Static>(std::forward<C>(callable));
			}
			else if constexpr (IsRuntimeTaskValue<C>)
			{
				node_->handle_.emplace<JobNode::PreemptiveRuntime>(std::forward<C>(callable));
			}
			else if constexpr (IsSubflowTaskValue<C>)
			{
				node_->handle_.emplace<JobNode::Subflow>(std::forward<C>(callable));
			}
			else if constexpr (IsSingleConditionTaskValue<C>)
			{
				node_->handle_.emplace<JobNode::SingleCondition>(std::forward<C>(callable));
			}
			else if constexpr (IsMultiConditionTaskValue<C>)
			{
				node_->handle_.emplace<JobNode::MultiCondition>(std::forward<C>(callable));
			}
			else
			{

			}
			return *this;
		}

		/**
		* [EN]
		* Turns this task into a module wrapping target's underlying
		* graph (owned externally). Returns *this for chaining.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* target の内部グラフ（外部が所有）を包むモジュールへこのタスクを
		* 変換する。メソッドチェーン用に *this を返す。
		*/
		template<GraphLike T>
		JobTask& Composed(T& target)
		{
			node_->handle_.emplace<JobNode::OwnedModule>(RetrieveGraph(target));
			return *this;
		}

		/**
		* [EN]
		* Turns this task into a module that takes ownership of graph
		* (moved in). Returns *this for chaining.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* graph の所有権を（ムーブで）引き受けるモジュールへこのタスクを
		* 変換する。メソッドチェーン用に *this を返す。
		*/
		JobTask& Adopt(JobGraph&& graph);

		/**
		* [EN]
		* Establishes this task as a predecessor of every task in tasks.
		* Returns *this for chaining.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* tasks の各タスクに対して、このタスクを先行タスクとして設定する。
		* メソッドチェーン用に *this を返す。
		*/
		template<typename... Ts>
		JobTask& Precede(Ts&&... tasks)
		{
			(node_->Precede(tasks.node_), ...);
			return *this;
		}

		/**
		* [EN]
		* Establishes this task as a successor of every task in tasks.
		* Returns *this for chaining.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* tasks の各タスクに対して、このタスクを後続タスクとして設定する。
		* メソッドチェーン用に *this を返す。
		*/
		template<typename... Ts>
		JobTask& Succeed(Ts&&... tasks)
		{
			(tasks.node_->Precede(node_), ...);
			return *this;
		}

		/**
		* [EN]
		* Removes the precedence edges between this task and every task
		* in tasks (as predecessors of this task). Returns *this for chaining.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* このタスクと tasks の各タスク（このタスクの先行タスクとして）
		* との間の先行関係エッジを削除する。メソッドチェーン用に *this を返す。
		*/
		template<typename... Ts>
		JobTask& RemovePredecessors(Ts&&... tasks)
		{
			(tasks.node_->RemoveSuccessors(node_), ...);
			(node_->RemovePredecessors(tasks.node_), ...);
			return *this;
		}

		/**
		* [EN]
		* Removes the precedence edges between this task and every task
		* in tasks (as successors of this task). Returns *this for chaining.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* このタスクと tasks の各タスク（このタスクの後続タスクとして）
		* との間の先行関係エッジを削除する。メソッドチェーン用に *this を返す。
		*/
		template<typename... Ts>
		JobTask& RemoveSuccessors(Ts&&... tasks)
		{
			(node_->RemoveSuccessors(tasks.node_), ...);
			(tasks.node_->RemovePredecessors(node_), ...);
			return *this;
		}

		/**
		* [EN]
		* Registers every semaphore in [first, last) to be released once
		* this task finishes executing. Returns *this for chaining.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* [first, last) の各セマフォを、このタスクの実行完了時に解放される
		* ものとして登録する。メソッドチェーン用に *this を返す。
		*/
		template<typename I>
		JobTask& Release(I first, I last)
		{
			if (!node_->semaphores_)
			{
				node_->semaphores_ = std::make_unique<JobNode::Semaphores>();
			}
			node_->semaphores_->release_.reserve(node_->semaphores_->release_.size() + std::distance(first, last));
			for (auto s = first;s != last;++s)
			{
				node_->semaphores_->release_.push_back(&(*s));
			}
			return *this;
		}

		/**
		* [EN]
		* Registers semaphore to be acquired before this task may
		* execute. Returns *this for chaining.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* このタスクが実行可能になる前に獲得すべきセマフォとして
		* semaphore を登録する。メソッドチェーン用に *this を返す。
		*/
		JobTask& Acquire(Semaphore& semaphore);

		/**
		* [EN]
		* Registers every semaphore in [first, last) to be acquired
		* before this task may execute. Returns *this for chaining.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* [first, last) の各セマフォを、このタスクが実行可能になる前に
		* 獲得すべきものとして登録する。メソッドチェーン用に *this を返す。
		*/
		template<typename I>
		JobTask& Acquire(I first, I last)
		{
			if (!node_->semaphores_)
			{
				node_->semaphores_ = std::make_unique<JobNode::Semaphores>();
			}
			node_->semaphores_->acquire_.reserve(node_->semaphores_->acquire_.size() + std::distance(first.last));
			for (auto s = first;s != last;++s)
			{
				node_->semaphores_->acquire_.push_back(&(*s));
			}
			return *this;
		}

		/**
		* [EN]
		* Sets the underlying node's user-data pointer and returns *this for chaining.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 内部ノードのユーザーデータポインタを設定し、メソッドチェーン用に
		* *this を返す。
		*/
		JobTask& Data(void* data);

		/**
		* [EN]
		* Resets the underlying node to its default (placeholder) state,
		* clearing work, dependencies, and semaphores.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 内部ノードをデフォルト（プレースホルダー）状態にリセットし、
		* 処理・依存関係・セマフォをクリアする。
		*/
		void Reset();

		/**
		* [EN]
		* Clears only the assigned work (NodeHandle), leaving
		* dependencies and semaphores intact.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 割り当てられた処理（NodeHandle）のみをクリアし、依存関係と
		* セマフォはそのまま維持する。
		*/
		void ResetWork();

		/**
		* [EN]
		* Returns whether this handle points at no node.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* このハンドルがどのノードも指していないかどうかを返す。
		*/
		Bool Empty()const;

		/**
		* [EN]
		* Returns whether this task has been assigned any work (its
		* NodeHandle is not the Placeholder alternative).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* このタスクに何らかの処理が割り当てられているか（NodeHandle が
		* Placeholder 以外であるか）を返す。
		*/
		Bool HasWork()const;

		/**
		* [EN]
		* Invokes visitor(JobTask) for every successor of this task.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* このタスクの各後続タスクに対して visitor(JobTask) を呼び出す。
		*/
		template<typename V>
		void EachSuccessor(V&& visitor)const
		{
			for (Size index = node_->numberSuccessors_;index < node_->edges_.size();++index)
			{
				visitor(JobTask(node_->edges_[index]));
			}
		}

		/**
		* [EN]
		* Invokes visitor(JobTask) for every predecessor of this task.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* このタスクの各先行タスクに対して visitor(JobTask) を呼び出す。
		*/
		template<typename V>
		void EachPredecessor(V&& visitor)const
		{
			for (Size index = node_->numberSuccessors_;index < node_->edges_.size();++index)
			{
				visitor(JobTask(node_->edges_[index]));
			}
		}

		/**
		* [EN]
		* If this task is a Subflow, invokes visitor(JobTask) for every
		* node currently in its nested subgraph; otherwise does nothing.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* このタスクが Subflow であれば、そのネストされたサブグラフ内の
		* 各ノードに対して visitor(JobTask) を呼び出す。そうでなければ
		* 何もしない。
		*/
		template<typename V>
		void EachSubflowTask(V&& visitor)const
		{
			if (auto ptr = std::get_if<JobNode::Subflow>(&node_->handle_);ptr)
			{
				for (auto iterator = ptr->subgraph_.begin();iterator != ptr->subgraph_.end();++iterator)
				{
					visitor(JobTask(*iterator));
				}
			}
		}

		/**
		* [EN]
		* Returns a hash value identifying the underlying node.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 内部ノードを識別するハッシュ値を返す。
		*/
		Size HashValue()const;

		/**
		* [EN]
		* Returns the kind of work this task currently performs.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* このタスクが現在行っている処理の種類を返す。
		*/
		JobTaskType Type()const;

		/**
		* [EN]
		* Returns the underlying node's user-data pointer.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 内部ノードのユーザーデータポインタを返す。
		*/
		void* Data()const;

		/**
		* [EN]
		* Returns the exception propagated to this task, if any.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* このタスクに伝播した例外があれば、それを返す。
		*/
		std::exception_ptr ExceptionPtr()const;

		/**
		* [EN]
		* Returns whether an exception has been propagated to this task.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* このタスクに例外が伝播しているかどうかを返す。
		*/
		Bool HasExceptionPtr()const;

	private:
		/**
		* [EN]
		* Constructs a handle wrapping node directly.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* node を直接包むハンドルを構築する。
		*/
		JobTask(JobNode* node);

		/// [EN] The node this handle refers to; nullptr if empty.
		/// [JP] このハンドルが参照するノード。空であれば nullptr。
		JobNode* node_ = nullptr;
	};

	/**
	* [EN]
	* Read-only, non-owning view onto a JobNode, exposing the same
	* query surface as JobTask (name, dependency counts, iteration,
	* type/hash) but without the graph-building/mutation API. Used where
	* callers should be able to inspect a node without being able to modify it.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* JobNode に対する読み取り専用・非所有のビュー。JobTask と同じ
	* 問い合わせ用インターフェース（名前・依存関係数・走査・種別/ハッシュ）
	* を公開するが、グラフ構築・変更用の API は持たない。呼び出し側が
	* ノードを変更できないようにしつつ参照だけはできるようにしたい場合に
	* 使う。
	*/
	class JobTaskView
	{
	private:
		friend class JobExecutor;

	public:
		/**
		* [EN]
		* Returns the underlying node's display name.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 内部ノードの表示名を返す。
		*/
		const String& Name()const;

		/**
		* [EN]
		* Returns the number of successor nodes connected to this node.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* このノードに接続されている後続ノードの数を返す。
		*/
		Size NumberSuccessors()const;

		/**
		* [EN]
		* Returns the number of predecessor nodes connected to this node.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* このノードに接続されている先行ノードの数を返す。
		*/
		Size NumberPredecessors()const;

		/**
		* [EN]
		* Returns the number of strong (unconditional) dependencies this node has.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* このノードが持つ強い（無条件の）依存関係の数を返す。
		*/
		Size NumberStrongDependencies()const;

		/**
		* [EN]
		* Returns the number of weak (conditional-branch-only) dependencies this node has.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* このノードが持つ弱い（条件分岐経由のみの）依存関係の数を返す。
		*/
		Size NumberWeakDependencies()const;

		/**
		* [EN]
		* Invokes visitor(JobTaskView) for every successor of this node.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* このノードの各後続ノードに対して visitor(JobTaskView) を呼び出す。
		*/
		template<typename V>
		void EachSuccessor(V&& visitor)const
		{
			for (Size index = 0;index < node_.numberSuccessors_;++index)
			{
				visitor(JobTaskView(*node_.edges_[index]));
			}
		}

		/**
		* [EN]
		* Invokes visitor(JobTaskView) for every predecessor of this node.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* このノードの各先行ノードに対して visitor(JobTaskView) を呼び出す。
		*/
		template<typename V>
		void EachPredecessor(V&& visitor)const
		{
			for (Size index = 0;index < node_.numberSuccessors_;++index)
			{
				visitor(JobTaskView(*node_.edges_[index]));
			}
		}

		/**
		* [EN]
		* Returns the kind of work this node currently performs.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* このノードが現在行っている処理の種類を返す。
		*/
		JobTaskType Type()const;

		/**
		* [EN]
		* Returns a hash value identifying the underlying node.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 内部ノードを識別するハッシュ値を返す。
		*/
		Size HashValue()const;

	private:
		/**
		* [EN]
		* Constructs a view referring to node directly.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* node を直接参照するビューを構築する。
		*/
		JobTaskView(const JobNode& node);

		/**
		* [EN]
		* Copy-constructs, referring to the same underlying node as the source view.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* コピー元のビューと同じ内部ノードを参照するようにコピー構築する。
		*/
		JobTaskView(const JobTaskView&) = default;

		/// [EN] The node this view refers to.
		/// [JP] このビューが参照するノード。
		const JobNode& node_;
	};
}

namespace std
{
	/**
	* [EN]
	* std::hash specialization for JobTask, delegating to
	* JobTask::HashValue so JobTask can be used as a key in unordered
	* containers.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* JobTask 向けの std::hash 特殊化。JobTask::HashValue へ委譲し、
	* JobTask を unordered コンテナのキーとして使用できるようにする。
	*/
	template<>
	struct hash<SeedCore::JobTask>
	{
		/**
		* [EN]
		* Returns task's hash value.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* task のハッシュ値を返す。
		*/
		auto operator()(const SeedCore::JobTask& task)const noexcept
		{
			return task.HashValue();
		}
	};

	/**
	* [EN]
	* std::hash specialization for JobTaskView, delegating to
	* JobTaskView::HashValue so JobTaskView can be used as a key in
	* unordered containers.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* JobTaskView 向けの std::hash 特殊化。JobTaskView::HashValue へ
	* 委譲し、JobTaskView を unordered コンテナのキーとして使用できる
	* ようにする。
	*/
	template<>
	struct hash<SeedCore::JobTaskView>
	{
		/**
		* [EN]
		* Returns taskView's hash value.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* taskView のハッシュ値を返す。
		*/
		auto operator()(const SeedCore::JobTaskView& taskView)const noexcept
		{
			return taskView.HashValue();
		}
	};
}
