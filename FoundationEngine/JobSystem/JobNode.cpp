#include <FoundationEngine/JobSystem/JobNode.h>

namespace SeedCore
{
	/**
	* [EN]
	* Constructs, storing a reference to the externally-owned graph.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 外部が所有するグラフへの参照を保持して構築する。
	*/
	JobNode::OwnedModule::OwnedModule(JobGraph& graph) :graph_(graph)
	{
		/// No Code
	}

	/**
	* [EN]
	* Constructs, moving graph into this module's ownership.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* graph をこのモジュールの所有権へムーブして構築する。
	*/
	JobNode::AdoptedModule::AdoptedModule(JobGraph&& graph) :graph_(std::move(graph))
	{
		/// No Code
	}
}

namespace SeedCore
{
	/**
	* [EN]
	* Returns the number of successor nodes connected to this node.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* このノードに接続されている後続ノードの数を返す。
	*/
	Size JobNode::NumberSuccessors()const
	{
		return numberSuccessors_;
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
	Size JobNode::NumberPredecessors()const
	{
		/// [EN] edges_ stores successors in the front portion and predecessors in the remainder, so predecessor count is the total minus the successor count.
		/// [JP] edges_ は前方に後続、残りに先行を格納しているため、先行ノード数は全体数から後続ノード数を引いた値になる。
		return edges_.size() - numberSuccessors_;
	}

	/**
	* [EN]
	* Returns the number of strong dependencies (e.g. unconditional
	* predecessors) this node has.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* このノードが持つ強い依存関係（条件分岐を伴わない先行ノードなど）の
	* 数を返す。
	*/
	Size JobNode::NumberStrongDependencies()const
	{
		/// [EN] Among the predecessor portion of edges_, count those that are NOT conditioners (i.e. unconditional/strong dependencies).
		/// [JP] edges_ の先行ノード部分のうち、条件分岐（conditioner）でないもの（＝無条件・強い依存関係）の数を数える。
		Size n = 0;
		for (Size index = numberSuccessors_;index < edges_.size();index++)
		{
			n += edges_[index]->IsConditioner();
		}
		return n;
	}

	/**
	* [EN]
	* Returns the number of weak dependencies (e.g. predecessors reached
	* only through a conditional branch) this node has.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* このノードが持つ弱い依存関係（条件分岐を経由してのみ到達される
	* 先行ノードなど）の数を返す。
	*/
	Size JobNode::NumberWeakDependencies()const
	{
		/// [EN] Among the predecessor portion of edges_, count those that ARE conditioners (i.e. dependencies reached only via a conditional branch, hence "weak").
		/// [JP] edges_ の先行ノード部分のうち、条件分岐（conditioner）であるもの（＝条件分岐を経由してのみ到達される、「弱い」依存関係）の数を数える。
		Size n = 0;
		for (Size index = numberSuccessors_;index < edges_.size();index++)
		{
			n += !edges_[index]->IsConditioner();
		}
		return n;
	}

	/**
	* [EN]
	* Returns this node's display name.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* このノードの表示名を返す。
	*/
	const String& JobNode::Name()const
	{
		return name_;
	}

	/**
	* [EN]
	* Returns whether this node's parent has been cancelled, meaning
	* this node should not proceed with execution.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* このノードの親がキャンセルされているかどうかを返す。
	* キャンセルされている場合、このノードは実行を進めるべきではない。
	*/
	Bool JobNode::IsParentCancelled()const
	{
		/// [EN] Cancelled if this node's topology is flagged CANCELLED/EXCEPTION, or if its parent node is flagged CANCELLED/EXCEPTION.
		/// [JP] このノードが属するトポロジーが CANCELLED/EXCEPTION フラグを持つ場合、または親ノードが CANCELLED/EXCEPTION フラグを持つ場合にキャンセル済みとなる。
		return (topology_ && topology_->estate_.load(std::memory_order_relaxed) & (JobExceptionState::CANCELLED | JobExceptionState::EXCEPTION)) || (parent_ && (parent_->estate_.load(std::memory_order_relaxed) & (JobExceptionState::CANCELLED | JobExceptionState::EXCEPTION)));
	}

	/**
	* [EN]
	* Returns whether this node represents a conditional branch (i.e.
	* its handle is SingleCondition or MultiCondition).
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* このノードが条件分岐を表すかどうか（すなわちハンドルが
	* SingleCondition または MultiCondition であるか）を返す。
	*/
	Bool JobNode::IsConditioner()const
	{
		return handle_.index() == JobNode::SINGLE_CONDITION || handle_.index() == JobNode::MULTI_CONDITION;
	}

	/**
	* [EN]
	* Attempts to acquire all of this node's required semaphores. If any
	* acquisition must wait, the corresponding waiting jobs are
	* collected into nodes and the call fails as a whole.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* このノードが必要とするすべてのセマフォの獲得を試みる。いずれかの
	* 獲得が待機を要する場合、該当する待機ジョブを nodes に収集し、
	* この呼び出し全体は失敗とする。
	*/
	Bool JobNode::AcquireAll(HybridArray<JobNode*>& nodes)
	{
		auto& acquire = semaphores_->acquire_;
		for (Size index = 0;index < acquire.size();++index)
		{
			if (!acquire[index]->try_acquire_or_wait(this))
			{
				/// [EN] Acquisition failed partway through: roll back by releasing every semaphore successfully acquired so far (in reverse order), then report overall failure.
				/// [JP] 途中で獲得に失敗したため、それまでに獲得済みのセマフォを（逆順に）すべて解放してロールバックし、全体としての失敗を報告する。
				for (Size rollbackIndex = 1;rollbackIndex <= index;++rollbackIndex)
				{
					acquire[index - rollbackIndex]->release(nodes);
				}
				return false;
			}
		}
		return true;
	}

	/**
	* [EN]
	* Releases all of this node's held semaphores, collecting any jobs
	* that become runnable as a result into nodes.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* このノードが保持しているすべてのセマフォを解放し、その結果
	* 実行可能になったジョブを nodes に収集する。
	*/
	void JobNode::ReleaseAll(HybridArray<JobNode*>& nodes)
	{
		auto& release = semaphores_->release_;
		for (const auto& semaphore : release)
		{
			semaphore->release(nodes);
		}
	}

	/**
	* [EN]
	* Establishes a precedence (dependency) edge from this node to node,
	* making node a successor of this node.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* このノードから node への先行関係（依存関係）のエッジを確立し、
	* node をこのノードの後続として設定する。
	*/
	void JobNode::Precede(JobNode* node)
	{
		/// [EN] Add node to the end of edges_, then swap it into position at index numberSuccessors_ (extending the successor region by one) so successors stay contiguous at the front.
		/// [JP] node を edges_ の末尾に追加し、それをインデックス numberSuccessors_ の位置にスワップする（後続領域を 1 つ拡張する）ことで、後続ノードが先頭に連続して並ぶようにする。
		edges_.push_back(node);
		std::swap(edges_[numberSuccessors_++], edges_[edges_.size() - 1]);

		/// [EN] Register this as a predecessor of node (its reciprocal edge).
		/// [JP] this を node の先行ノードとして登録する（相互エッジ）。
		node->edges_.push_back(this);
	}

	/**
	* [EN]
	* Initializes/recomputes this node's join counter based on its
	* current set of dependencies.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 現在の依存関係の集合に基づいて、このノードの join カウンタを
	* 初期化・再計算する。
	*/
	void JobNode::SetUpJoinCounter()
	{
		/// [EN] Count unconditional (non-conditioner) predecessors and fold that count into nstate_.
		/// [JP] 無条件（非 conditioner）の先行ノードの数を数え、その数を nstate_ に組み込む。
		for (Size index = numberSuccessors_;index < edges_.size();index++)
		{
			nstate_ += !edges_[index]->IsConditioner();
		}

		/// [EN] Initialize the join counter from the strong-dependency bits of nstate_, so the node becomes runnable once that many strong predecessors have completed.
		/// [JP] nstate_ の強い依存関係を表すビットから join カウンタを初期化し、その数の強い先行ノードが完了した時点でこのノードが実行可能になるようにする。
		joinCounter_.store(nstate_ & JobNodeState::STRONG_DEPENDENCIES_MASK, std::memory_order_relaxed);
	}

	/**
	* [EN]
	* Removes node from this node's list of successors.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* このノードの後続一覧から node を削除する。
	*/
	void JobNode::RemoveSuccessors(JobNode* node)
	{
		/// [EN] Remove node from the successor region [begin, begin + numberSuccessors_) using erase-remove, tracking the new successor count.
		/// [JP] erase-remove を用いて後続領域 [begin, begin + numberSuccessors_) から node を削除し、新しい後続ノード数を記録する。
		auto sit = std::remove(edges_.begin(), edges_.begin() + numberSuccessors_, node);
		Size newNumberSuccessor = std::distance(edges_.begin(), sit);

		/// [EN] Shift the predecessor region left to close the gap left by the removed successor(s), then shrink the container and update the successor count.
		/// [JP] 削除された後続ノード分の隙間を詰めるため、先行ノード領域を前方へ詰め、コンテナを縮小して後続ノード数を更新する。
		std::move(edges_.begin() + numberSuccessors_, edges_.end(), sit);
		edges_.resize(edges_.size() - (numberSuccessors_ - newNumberSuccessor));
		numberSuccessors_ = newNumberSuccessor;
	}

	/**
	* [EN]
	* Removes node from this node's list of predecessors.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* このノードの先行一覧から node を削除する。
	*/
	void JobNode::RemovePredecessors(JobNode* node)
	{
		/// [EN] Remove node from the predecessor region [begin + numberSuccessors_, end) using erase-remove; the successor region is untouched.
		/// [JP] erase-remove を用いて先行ノード領域 [begin + numberSuccessors_, end) から node を削除する。後続ノード領域には影響しない。
		edges_.erase(std::remove(edges_.begin() + numberSuccessors_, edges_.end(), node), edges_.end());
	}
}
