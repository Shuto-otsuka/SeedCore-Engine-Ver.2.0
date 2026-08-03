#include <FoundationEngine/JobSystem/JobGraph.h>
#include <FoundationEngine/JobSystem/JobNode.h>

namespace SeedCore
{
	/**
	* [EN]
	* Destroys the graph, releasing all owned nodes.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* グラフを破棄し、所有しているすべてのノードを解放する。
	*/
	JobGraph::~JobGraph()
	{
		clear();
	}

	/**
	* [EN]
	* Move-constructs the graph, transferring ownership of the nodes
	* from other.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* グラフをムーブ構築し、other からノードの所有権を移譲する。
	*/
	JobGraph::JobGraph(JobGraph&& other) :nodes_(std::move(other.nodes_))
	{
		/// No Code
	}

	/**
	* [EN]
	* Move-assigns the graph, transferring ownership of the nodes from
	* other and releasing any previously owned nodes.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* グラフをムーブ代入し、other からノードの所有権を移譲し、
	* それ以前に所有していたノードを解放する。
	*/
	JobGraph& JobGraph::operator=(JobGraph&& other)
	{
		/// [EN] Release any nodes currently owned by *this before taking ownership of other's nodes.
		/// [JP] other のノードの所有権を受け取る前に、*this が現在所有しているノードを解放する。
		clear();
		nodes_ = std::move(other.nodes_);
		return *this;
	}

	/**
	* [EN]
	* Removes and destroys all nodes currently owned by the graph.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* グラフが現在所有しているすべてのノードを削除し、破棄する。
	*/
	void JobGraph::clear()
	{
		/// [EN] Return every node back to the object pool before clearing the container itself.
		/// [JP] コンテナ自体をクリアする前に、すべてのノードをオブジェクトプールへ返却する。
		for (auto node : nodes_)
		{
			recycle(node);
		}
		nodes_.clear();
	}

	/**
	* [EN]
	* Returns the number of nodes currently in the graph.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* グラフ内に現在存在するノードの数を返す。
	*/
	Size JobGraph::size()const
	{
		return nodes_.size();
	}

	/**
	* [EN]
	* Returns whether the graph currently has no nodes.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* グラフが現在ノードを持たないかどうかを返す。
	*/
	Bool JobGraph::empty()const
	{
		return nodes_.empty();
	}

	/**
	* [EN]
	* Returns a mutable iterator to the first node.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 先頭ノードを指す、変更可能なイテレータを返す。
	*/
	JobGraph::Iterator JobGraph::begin()
	{
		return nodes_.begin();
	}

	/**
	* [EN]
	* Returns a mutable iterator to one past the last node.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 末尾ノードの次を指す、変更可能なイテレータを返す。
	*/
	JobGraph::Iterator JobGraph::end()
	{
		return nodes_.end();
	}

	/**
	* [EN]
	* Returns a const iterator to the first node.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 先頭ノードを指す、読み取り専用のイテレータを返す。
	*/
	JobGraph::ConstIterator JobGraph::begin()const
	{
		return nodes_.begin();
	}

	/**
	* [EN]
	* Returns a const iterator to one past the last node.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 末尾ノードの次を指す、読み取り専用のイテレータを返す。
	*/
	JobGraph::ConstIterator JobGraph::end()const
	{
		return nodes_.end();
	}

	/**
	* [EN]
	* Removes and destroys a single node from the graph.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* グラフから単一の node を削除し、破棄する。
	*/
	void JobGraph::erase(JobNode* node)
	{
		/// [EN] Find the matching node, recycle it back to the pool as a side effect of the predicate, then physically remove it from the container (erase-remove idiom).
		/// [JP] 該当するノードを見つけ、述語の副作用としてそれをプールへ返却（recycle）した上で、コンテナから物理的に削除する（erase-remove イディオム）。
		nodes_.erase(
			std::remove_if(nodes_.begin(), nodes_.end(), [&](auto& p)
				{
					if (p == node)
					{
						recycle(p);
						return true;
					}
					return false;
				}),
			nodes_.end()
		);
	}
}
