#include <FoundationEngine/JobSystem/JobTask.h>
#include <FoundationEngine/JobSystem/Semaphore.h>

namespace SeedCore
{
	/**
	* [EN]
	* Copy-constructs, pointing at the same underlying node as rhs.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* rhs と同じ内部ノードを指すようにコピー構築する。
	*/
	JobTask::JobTask(const JobTask& rhs) :node_(rhs.node_)
	{
		/// No Code
	}

	/**
	* [EN]
	* Constructs a handle wrapping node directly.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* node を直接包むハンドルを構築する。
	*/
	JobTask::JobTask(JobNode* node) :node_(node)
	{
		/// No Code
	}

	/**
	* [EN]
	* Copy-assigns, pointing at the same underlying node as rhs.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* rhs と同じ内部ノードを指すようにコピー代入する。
	*/
	JobTask& JobTask::operator=(const JobTask& rhs)
	{
		node_ = rhs.node_;
		return *this;
	}

	/**
	* [EN]
	* Resets this handle to point at no node.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* このハンドルをどのノードも指さない状態にリセットする。
	*/
	JobTask& JobTask::operator=(std::nullptr_t null)
	{
		node_ = null;
		return *this;
	}

	/**
	* [EN]
	* Returns whether this and rhs refer to the same underlying node.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* this と rhs が同じ内部ノードを指しているかどうかを返す。
	*/
	Bool JobTask::operator==(const JobTask& rhs)const
	{
		return node_ == rhs.node_;
	}

	/**
	* [EN]
	* Negation of operator==.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* operator== の否定。
	*/
	Bool JobTask::operator!=(const JobTask& rhs)const
	{
		return node_ != rhs.node_;
	}

	/**
	* [EN]
	* Returns the underlying node's display name.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 内部ノードの表示名を返す。
	*/
	const String& JobTask::Name()const
	{
		return node_->name_;
	}

	/**
	* [EN]
	* Returns the number of successor tasks connected to this task.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* このタスクに接続されている後続タスクの数を返す。
	*/
	Size JobTask::NumberSuccessors()const
	{
		return node_->NumberSuccessors();
	}

	/**
	* [EN]
	* Returns the number of predecessor tasks connected to this task.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* このタスクに接続されている先行タスクの数を返す。
	*/
	Size JobTask::NumberPredecessors()const
	{
		return node_->NumberPredecessors();
	}

	/**
	* [EN]
	* Returns the number of strong (unconditional) dependencies this task has.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* このタスクが持つ強い（無条件の）依存関係の数を返す。
	*/
	Size JobTask::NumberStrongDependencies()const
	{
		return node_->NumberStrongDependencies();
	}

	/**
	* [EN]
	* Returns the number of weak (conditional-branch-only) dependencies this task has.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* このタスクが持つ弱い（条件分岐経由のみの）依存関係の数を返す。
	*/
	Size JobTask::NumberWeakDependencies()const
	{
		return node_->NumberWeakDependencies();
	}

	/**
	* [EN]
	* Sets the underlying node's display name and returns *this for chaining.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 内部ノードの表示名を設定し、メソッドチェーン用に *this を返す。
	*/
	JobTask& JobTask::Name(const String& name)
	{
		node_->name_ = name;
		return *this;
	}

	/**
	* [EN]
	* Turns this task into a module that takes ownership of graph (moved
	* in). Returns *this for chaining.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* graph の所有権を（ムーブで）引き受けるモジュールへこのタスクを
	* 変換する。メソッドチェーン用に *this を返す。
	*/
	JobTask& JobTask::Adopt(JobGraph&& graph)
	{
		node_->handle_.emplace<JobNode::AdoptedModule>(std::move(graph));
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
	JobTask& JobTask::Data(void* data)
	{
		node_->data_ = data;
		return *this;
	}

	/**
	* [EN]
	* Resets this handle to point at no node.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* このハンドルをどのノードも指さない状態にリセットする。
	*/
	void JobTask::Reset()
	{
		node_ = nullptr;
	}

	/**
	* [EN]
	* Clears only the assigned work (NodeHandle), leaving dependencies
	* and semaphores intact.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 割り当てられた処理（NodeHandle）のみをクリアし、依存関係と
	* セマフォはそのまま維持する。
	*/
	void JobTask::ResetWork()
	{
		node_->handle_.emplace<std::monostate>();
	}

	/**
	* [EN]
	* Returns whether this handle points at no node.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* このハンドルがどのノードも指していないかどうかを返す。
	*/
	Bool JobTask::Empty()const
	{
		return node_ == nullptr;
	}

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
	Bool JobTask::HasWork()const
	{
		return node_ ? node_->handle_.index() != 0 : false;
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
	Size JobTask::HashValue()const
	{
		return std::hash<JobNode*>{}(node_);
	}

	/**
	* [EN]
	* Returns the kind of work this task currently performs, derived
	* from the underlying node's active NodeHandle alternative.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 内部ノードの現在有効な NodeHandle の選択肢から導出される、この
	* タスクが現在行っている処理の種類を返す。
	*/
	JobTaskType JobTask::Type()const
	{
		switch (node_->handle_.index())
		{
		case JobNode::PLACEHOLDER:
			return JobTaskType::PLACEHOLDER;
		case JobNode::STATIC:
			return JobTaskType::STATIC;
		case JobNode::PREEMPTIVE_RUNTIME:
			[[fallthrough]];
		case JobNode::NONPREEMPTIVE_RUNTIME:
			return JobTaskType::RUNTIME;
		case JobNode::SUBFLOW:
			return JobTaskType::SUBFLOW;
		case JobNode::SINGLE_CONDITION:
			[[fallthrough]];
		case JobNode::MULTI_CONDITION:
			return JobTaskType::CONDITION;
		case JobNode::OWNED_MODULE:
			[[fallthrough]];
		case JobNode::ADOPTED_MODULE:
			return JobTaskType::MODULE;
		default:
			return JobTaskType::UNDEFINED;
		}
	}

	/**
	* [EN]
	* Registers semaphore to be acquired before this task may execute.
	* Returns *this for chaining.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* このタスクが実行可能になる前に獲得すべきセマフォとして
	* semaphore を登録する。メソッドチェーン用に *this を返す。
	*/
	JobTask& JobTask::Acquire(Semaphore& semaphore)
	{
		if (!node_->semaphores_)
		{
			node_->semaphores_ = std::make_unique<JobNode::Semaphores>();
		}
		node_->semaphores_->release_.push_back(&semaphore);
		return *this;
	}

	/**
	* [EN]
	* Returns the underlying node's user-data pointer.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 内部ノードのユーザーデータポインタを返す。
	*/
	void* JobTask::Data()const
	{
		return node_->data_;
	}

	/**
	* [EN]
	* Returns the exception propagated to this task, if any.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* このタスクに伝播した例外があれば、それを返す。
	*/
	std::exception_ptr JobTask::ExceptionPtr()const
	{
		return node_ ? node_->exceptionPtr_ : nullptr;
	}

	/**
	* [EN]
	* Returns whether an exception has been propagated to this task.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* このタスクに例外が伝播しているかどうかを返す。
	*/
	Bool JobTask::HasExceptionPtr()const
	{
		return node_ ? (node_->exceptionPtr_ != nullptr) : false;
	}
}

namespace SeedCore
{
	/**
	* [EN]
	* Constructs a view referring to node directly.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* node を直接参照するビューを構築する。
	*/
	JobTaskView::JobTaskView(const JobNode& node) :node_(node)
	{
		/// No Code
	}

	/**
	* [EN]
	* Returns the underlying node's display name.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 内部ノードの表示名を返す。
	*/
	const String& JobTaskView::Name()const
	{
		return node_.name_;
	}

	/**
	* [EN]
	* Returns the number of successor nodes connected to this node.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* このノードに接続されている後続ノードの数を返す。
	*/
	Size JobTaskView::NumberSuccessors()const
	{
		return node_.NumberSuccessors();
	}

	/**
	* [EN]
	* Returns the number of predecessor nodes connected to this node.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* このノードに接続されている先行ノードの数を返す。
	*/
	Size JobTaskView::NumberPredecessors()const
	{
		return node_.NumberPredecessors();
	}

	/**
	* [EN]
	* Returns the number of strong (unconditional) dependencies this node has.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* このノードが持つ強い（無条件の）依存関係の数を返す。
	*/
	Size JobTaskView::NumberStrongDependencies()const
	{
		return node_.NumberStrongDependencies();
	}

	/**
	* [EN]
	* Returns the number of weak (conditional-branch-only) dependencies this node has.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* このノードが持つ弱い（条件分岐経由のみの）依存関係の数を返す。
	*/
	Size JobTaskView::NumberWeakDependencies()const
	{
		return node_.NumberWeakDependencies();
	}

	/**
	* [EN]
	* Returns the kind of work this node currently performs, derived
	* from the underlying node's active NodeHandle alternative.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 内部ノードの現在有効な NodeHandle の選択肢から導出される、この
	* ノードが現在行っている処理の種類を返す。
	*/
	JobTaskType JobTaskView::Type()const
	{
		switch (node_.handle_.index())
		{
		case JobNode::PLACEHOLDER:
			return JobTaskType::PLACEHOLDER;
		case JobNode::STATIC:
			return JobTaskType::STATIC;
		case JobNode::PREEMPTIVE_RUNTIME:
			[[fallthrough]];
		case JobNode::NONPREEMPTIVE_RUNTIME:
			return JobTaskType::RUNTIME;
		case JobNode::SUBFLOW:
			return JobTaskType::SUBFLOW;
		case JobNode::SINGLE_CONDITION:
			[[fallthrough]];
		case JobNode::MULTI_CONDITION:
			return JobTaskType::CONDITION;
		case JobNode::OWNED_MODULE:
			[[fallthrough]];
		case JobNode::ADOPTED_MODULE:
			return JobTaskType::MODULE;
		default:
			return JobTaskType::UNDEFINED;
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
	Size JobTaskView::HashValue()const
	{
		return std::hash<const JobNode*>{}(&node_);
	}
}

namespace SeedCore
{
	/**
	* [EN]
	* Returns the display name of type as a null-terminated string, or
	* "Undefined" if type is out of range.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* type の表示名をヌル終端文字列として返す。type が範囲外であれば
	* "Undefined" を返す。
	*/
	inline const Char* ToString(JobTaskType type)
	{
		static constexpr StaticArray<const Char*, 7> names =
		{
			"Placeholder",
			"Static",
			"Runtime",
			"Subflow",
			"Condition",
			"Module",
			"Async"
		};

		const auto index = std::to_underlying(type);
		if (index >= names.size())
		{
			return "Undefined";
		}

		return names[index];
	}
}
