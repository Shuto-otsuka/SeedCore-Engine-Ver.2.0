#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/JobSystem/FlowBuilder.h>

namespace SeedCore
{
	/**
	* [EN]
	* Owns a named JobGraph together with the queue of JobTopology
	* instances (submitted runs) built from it. This is the top-level
	* entry point users build a graph on via FlowBuilder, then submit to
	* a JobExecutor for execution.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 名前付きの JobGraph と、そこから構築された JobTopology
	* インスタンス（投入された実行）のキューを合わせて所有するクラス。
	* ユーザーが FlowBuilder 経由でグラフを構築し、JobExecutor へ実行を
	* 投入するための、最上位のエントリーポイントとなる。
	*/
	class SEEDCORE_API JobTaskflow :public FlowBuilder
	{
	private:
		friend class JobTopology;
		friend class JobExecutor;
		friend class FlowBuilder;
		friend class Subflow;

	public:
		/**
		* [EN]
		* Constructs an empty taskflow with the given display name.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 指定された表示名で、空のタスクフローを構築する。
		*/
		JobTaskflow(const String& name);

		/**
		* [EN]
		* Constructs an empty taskflow with no name.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 名前を持たない、空のタスクフローを構築する。
		*/
		JobTaskflow();

		/**
		* [EN]
		* Move-constructs, transferring rhs's graph, name, and pending topologies.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* rhs のグラフ、名前、保留中のトポロジーを移譲してムーブ構築する。
		*/
		JobTaskflow(JobTaskflow&& rhs);

		/**
		* [EN]
		* Move-assigns, transferring rhs's graph, name, and pending topologies.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* rhs のグラフ、名前、保留中のトポロジーを移譲してムーブ代入する。
		*/
		JobTaskflow& operator=(JobTaskflow&& rhs);

		/**
		* [EN]
		* Destructor; uses the compiler-generated default (owned members
		* clean themselves up).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* デストラクタ。コンパイラ生成のデフォルトを使用する（所有する
		* メンバーは自身で後始末される）。
		*/
		~JobTaskflow() = default;

		/**
		* [EN]
		* Returns the number of tasks (nodes) currently in the graph.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* グラフ内に現在存在するタスク（ノード）の数を返す。
		*/
		Size NumberTasks()const;

		/**
		* [EN]
		* Returns whether the graph currently has no tasks.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* グラフが現在タスクを持たないかどうかを返す。
		*/
		Bool Empty()const;

		/**
		* [EN]
		* Sets the taskflow's display name.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* タスクフローの表示名を設定する。
		*/
		void Name(const String& name);

		/**
		* [EN]
		* Returns the taskflow's display name.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* タスクフローの表示名を返す。
		*/
		const String& Name()const;

		/**
		* [EN]
		* Removes every task from the graph.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* グラフからすべてのタスクを削除する。
		*/
		void Clear();

		/**
		* [EN]
		* Removes the precedence edge between from and to.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* from と to の間の先行関係エッジを削除する。
		*/
		void RemoveDependency(JobTask from, JobTask to);

		/**
		* [EN]
		* Returns a reference to the underlying JobGraph.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 内部の JobGraph への参照を返す。
		*/
		JobGraph& Graph();

	private:
		/// [EN] Guards concurrent access to name_/graph_/topologies_ from executor threads.
		/// [JP] エグゼキュータスレッドからの name_/graph_/topologies_ への並行アクセスを保護する。
		mutable std::mutex mutex_;

		/// [EN] Display name of this taskflow.
		/// [JP] このタスクフローの表示名。
		String name_;

		/// [EN] The graph this taskflow builds and owns.
		/// [JP] このタスクフローが構築・所有するグラフ。
		JobGraph graph_;

		/// [EN] Queue of submitted topologies (runs of graph_) awaiting or undergoing execution.
		/// [JP] 実行待ち、または実行中の、投入済みトポロジー（graph_ の実行インスタンス）のキュー。
		std::queue<ResourceRef<JobTopology>> topologies_;

		/**
		* [EN]
		* Enqueues topologies for execution and returns the resulting
		* queue size.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* topologies を実行キューへ追加し、追加後のキューサイズを返す。
		*/
		Size FetchEnqueue(ResourceRef<JobTopology> topologies);
	};

	/**
	* [EN]
	* Job-system-aware extension of std::future<T>: in addition to the
	* usual future interface, it optionally holds a weak reference to
	* the owning JobTopology so callers can Cancel() the underlying run.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* ジョブシステムを意識した std::future<T> の拡張クラス。通常の
	* future インターフェースに加えて、所有元の JobTopology への弱参照を
	* 任意で保持し、呼び出し側が内部の実行を Cancel() できるようにする。
	*/
	template<typename T>
	class JobFuture :public std::future<T>
	{
	private:
		friend class JobExecutor;
		friend class Subflow;
		friend class JobPreemptiveRuntime;

	public:
		/**
		* [EN]
		* Default constructor: creates a future with no shared state.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* デフォルトコンストラクタ: 共有状態を持たない future を生成する。
		*/
		JobFuture() = default;

		/**
		* [EN]
		* Copy construction is disabled, matching std::future's own
		* non-copyable contract.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* コピー構築は禁止されている。std::future 自体のコピー不可契約に
		* 合わせている。
		*/
		JobFuture(const JobFuture&) = delete;

		/**
		* [EN]
		* Move-constructs, transferring the source future's shared state
		* and topology reference (compiler-generated default).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* コピー元の future の共有状態とトポロジー参照を移譲して
		* ムーブ構築する（コンパイラ生成のデフォルト）。
		*/
		JobFuture(JobFuture&&) = default;

		/**
		* [EN]
		* Constructs from a plain std::future<T>, moving it in with no
		* associated topology (Cancel() will report false).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 素の std::future<T> をムーブして構築する。関連付けられた
		* トポロジーは持たない（Cancel() は false を返す）。
		*/
		JobFuture(std::future<T>&& future) :std::future<T>(std::move(future))
		{
			/// No Code
		}

		/**
		* [EN]
		* Copy assignment is disabled, matching std::future's own
		* non-copyable contract.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* コピー代入は禁止されている。std::future 自体のコピー不可契約に
		* 合わせている。
		*/
		JobFuture& operator=(const JobFuture&) = delete;

		/**
		* [EN]
		* Move-assigns, transferring the source future's shared state and
		* topology reference (compiler-generated default).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* コピー元の future の共有状態とトポロジー参照を移譲して
		* ムーブ代入する（コンパイラ生成のデフォルト）。
		*/
		JobFuture& operator=(JobFuture&&) = default;

		/**
		* [EN]
		* Attempts to cancel the run associated with this future by
		* flagging its topology CANCELLED. Returns whether an associated
		* (still alive) topology was found.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* このfutureに関連付けられた実行に対し、そのトポロジーへ
		* CANCELLED フラグを立てることでキャンセルを試みる。関連付けられた
		* （まだ生存している）トポロジーが見つかったかどうかを返す。
		*/
		Bool Cancel()
		{
			if (topology_)
			{
				topology_->estate_.fetch_or(JobExceptionState::CANCELLED, std::memory_order_relaxed);
				return true;
			}
			return false;
		}

	private:
		/// [EN] Observing reference to the topology this future's result belongs to; empty if none.
		/// [JP] この future の結果が属するトポロジーへの observing 参照。なければ空。
		ResourceRef<JobTopology> topology_;

		/**
		* [EN]
		* Constructs from a std::future<T> and the topology that produces
		* its result, enabling Cancel().
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* std::future<T> と、その結果を生成するトポロジーから構築する。
		* これにより Cancel() が使用可能になる。
		*/
		JobFuture(std::future<T>&& future, ResourceRef<JobTopology> pointer) :std::future<T>(std::move(future)), topology_(std::move(pointer))
		{
			/// No Code
		}
	};
}
