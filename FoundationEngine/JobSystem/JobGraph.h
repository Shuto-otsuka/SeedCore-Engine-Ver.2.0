#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/JobSystem/JobConcept.h>

namespace SeedCore
{
	/**
	* [EN]
	* Generic parameter container that can be attached to a task,
	* carrying a display name and an arbitrary user-defined data pointer.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* タスクに付与できる汎用的なパラメータコンテナであり、表示名と
	* 任意のユーザー定義データへのポインタを保持する。
	*/
	class TaskParams
	{
	public:
		/// [EN] Display name associated with the task.
		/// [JP] タスクに関連付けられた表示名。
		String name_;

		/// [EN] Pointer to arbitrary user-defined data associated with the task. Ownership is not managed here.
		/// [JP] タスクに関連付けられた、任意のユーザー定義データへのポインタ。所有権はここでは管理しない。
		void* data_ = nullptr;
	};

	/**
	* [EN]
	* Empty marker type used to indicate that a task has no parameters.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* タスクがパラメータを持たないことを示すための、空のマーカー型。
	*/
	class DefaultTaskParams {};

	/**
	* [EN]
	* Concept that is satisfied when P is usable as task parameters:
	* either TaskParams, DefaultTaskParams, or any string-like type
	* (treated as a name-only shorthand).
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* P がタスクパラメータとして使用可能である場合に満たされるコンセプト。
	* TaskParams、DefaultTaskParams、または（名前のみを指定する
	* 簡略形として扱われる）任意の文字列型のいずれかであること。
	*/
	template<typename P>
	concept TaskParamsLike = std::same_as<std::decay_t<P>, TaskParams> || std::same_as<std::decay_t<P>, DefaultTaskParams> || StringLike<P>;

	/// [EN] Compile-time boolean value mirroring the TaskParamsLike concept, for use in non-concept contexts (e.g. SFINAE, static_assert).
	/// [JP] TaskParamsLike コンセプトをミラーするコンパイル時のブール値。コンセプトを使えない文脈（SFINAE や static_assert など）で利用する。
	template<typename P>
	constexpr Bool IsTaskParamsValue = TaskParamsLike<P>;

	/**
	* [EN]
	* Owns a collection of JobNode pointers that together form a task
	* graph. Provides creation, iteration, and lifetime management of
	* the nodes; the actual graph-building API is exposed through
	* friend classes such as FlowBuilder and Subflow.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* タスクグラフを構成する JobNode ポインタの集合を所有するクラス。
	* ノードの生成、走査、ライフタイム管理を提供する。実際のグラフ構築用
	* API は FlowBuilder や Subflow といった friend クラスを通じて
	* 公開される。
	*/
	class SEEDCORE_API JobGraph
	{
	private:
		friend class JobNode;
		friend class FlowBuilder;
		friend class Subflow;
		friend class JobTaskflow;
		friend class JobExecutor;

	private:
		/// [EN] Mutable iterator type over the contained nodes.
		/// [JP] 保持しているノードに対する、変更可能なイテレータ型。
		using Iterator = DynamicArray<JobNode*>::iterator;

		/// [EN] Const iterator type over the contained nodes.
		/// [JP] 保持しているノードに対する、読み取り専用のイテレータ型。
		using ConstIterator = DynamicArray<JobNode*>::const_iterator;

		/// [EN] The collection of nodes owned by this graph.
		/// [JP] このグラフが所有するノードの集合。
		DynamicArray<JobNode*> nodes_;

	public:
		/**
		* [EN]
		* Default constructor: starts with an empty node collection.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* デフォルトコンストラクタ: 空のノード集合から開始する。
		*/
		JobGraph() = default;

		/**
		* [EN]
		* Destroys the graph, releasing all owned nodes.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* グラフを破棄し、所有しているすべてのノードを解放する。
		*/
		~JobGraph();

		/**
		* [EN]
		* Copy construction is disabled: a JobGraph exclusively owns its
		* nodes, so copying would require deep-cloning the graph, which
		* is not supported.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* コピー構築は禁止されている: JobGraph はノードを排他的に
		* 所有するため、コピーにはグラフの深いクローンが必要になるが、
		* それはサポートされていない。
		*/
		JobGraph(const JobGraph&) = delete;

		/**
		* [EN]
		* Move-constructs the graph, transferring ownership of the
		* nodes from other.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* グラフをムーブ構築し、other からノードの所有権を移譲する。
		*/
		JobGraph(JobGraph&& other);

		/**
		* [EN]
		* Copy assignment is disabled, for the same reason as the copy
		* constructor.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* コピー代入も、コピーコンストラクタと同じ理由により禁止されている。
		*/
		JobGraph& operator=(const JobGraph&) = delete;

		/**
		* [EN]
		* Move-assigns the graph, transferring ownership of the nodes
		* from other and releasing any previously owned nodes.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* グラフをムーブ代入し、other からノードの所有権を移譲し、
		* それ以前に所有していたノードを解放する。
		*/
		JobGraph& operator=(JobGraph&& other);

		/**
		* [EN]
		* Removes and destroys all nodes currently owned by the graph.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* グラフが現在所有しているすべてのノードを削除し、破棄する。
		*/
		void clear();

		/**
		* [EN]
		* Returns the number of nodes currently in the graph.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* グラフ内に現在存在するノードの数を返す。
		*/
		Size size()const;

		/**
		* [EN]
		* Returns whether the graph currently has no nodes.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* グラフが現在ノードを持たないかどうかを返す。
		*/
		Bool empty()const;

		/**
		* [EN]
		* Returns a mutable iterator to the first node.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 先頭ノードを指す、変更可能なイテレータを返す。
		*/
		Iterator begin();

		/**
		* [EN]
		* Returns a mutable iterator to one past the last node.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 末尾ノードの次を指す、変更可能なイテレータを返す。
		*/
		Iterator end();

		/**
		* [EN]
		* Returns a const iterator to the first node.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 先頭ノードを指す、読み取り専用のイテレータを返す。
		*/
		ConstIterator begin()const;

		/**
		* [EN]
		* Returns a const iterator to one past the last node.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 末尾ノードの次を指す、読み取り専用のイテレータを返す。
		*/
		ConstIterator end()const;

	private:
		/**
		* [EN]
		* Removes and destroys a single node from the graph.
		* Intended to be called by JobNode itself or other friend
		* classes during graph maintenance.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* グラフから単一の node を削除し、破棄する。グラフの保守処理中に
		* JobNode 自身や他の friend クラスから呼び出されることを想定している。
		*/
		void erase(JobNode* node);

		/**
		* [EN]
		* Constructs a new JobNode in place using args, adds it to
		* the graph's node list, and returns a pointer to it.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* args を用いて新しい JobNode をその場で構築し、グラフの
		* ノード一覧に追加した上で、そのノードへのポインタを返す。
		*/
		template <typename ...Args>
		JobNode* emplace_back(Args&&... args)
		{
			nodes_.push_back(animate(std::forward<Args>(args)...));
			return nodes_.back();
		}
	};

	/**
	* [EN]
	* Concept that is satisfied when T behaves like a JobGraph: either
	* by directly deriving from it, or by exposing a graph() accessor
	* that returns a reference convertible to JobGraph&.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* T が JobGraph のように振る舞う場合に満たされるコンセプト。
	* JobGraph を直接継承しているか、JobGraph& に変換可能な参照を
	* 返す graph() アクセサを持っていること。
	*/
	template<typename T>
	concept GraphLike = std::derived_from<T, JobGraph> || requires(T & t)
	{
		{ t.graph() }->std::convertible_to<JobGraph&>;
	};

	/**
	* [EN]
	* Retrieves the underlying JobGraph& from target, regardless of
	* whether target exposes it via a graph() accessor or is itself
	* derived from JobGraph.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* target が graph() アクセサを介して公開しているか、JobGraph
	* 自体を継承しているかに関わらず、内部の JobGraph& を取得する。
	*/
	template<GraphLike T>
	JobGraph& RetrieveGraph(T& target)
	{
		if constexpr (requires{target.graph();})
		{
			/// [EN] T exposes a graph() accessor: use it to obtain the underlying graph reference.
			/// [JP] T が graph() アクセサを公開している場合、それを使って内部のグラフ参照を取得する。
			return target.graph();
		}
		else
		{
			/// [EN] T derives from JobGraph directly: cast target itself to a JobGraph reference.
			/// [JP] T が JobGraph を直接継承している場合、target 自身を JobGraph 参照にキャストする。
			return static_cast<JobGraph&>(target);
		}
	}
}