#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/JobSystem/JobCommon.h>
#include <FoundationEngine/JobSystem/JobNodeBase.h>
#include <FoundationEngine/JobSystem/JobGraph.h>
#include <FoundationEngine/JobSystem/JobTopology.h>
#include <FoundationEngine/JobSystem/Semaphore.h>
#include <FoundationEngine/JobSystem/WorkerCommon.h>
#include <FoundationEngine/Pool/ObjectPool.h>

namespace SeedCore
{
	/**
	* [EN]
	* Represents a single node in a job graph. Holds the node's identity
	* (name/data), its position within the topology, its edges to other
	* nodes, and a NodeHandle variant describing what kind of work
	* (static function, runtime, subflow, condition, module, etc.) the
	* node actually performs.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* ジョブグラフ内の単一のノードを表すクラス。ノードの識別情報
	* （名前・データ）、トポロジー内での位置、他ノードへのエッジ、
	* および実際にどのような処理（静的関数、ランタイム、サブフロー、
	* 条件分岐、モジュールなど）を行うかを表す NodeHandle バリアントを
	* 保持する。
	*/
	class JobNode :public JobNodeBase
	{
	private:
		friend class JobGraph;
		friend class JobTask;
		friend class JobTaskView;
		friend class JobTaskflow;
		friend class JobExecutor;
		friend class FlowBuilder;
		friend class JobSubflow;
		friend class JobPreemptiveRuntime;
		friend class JobNonpreemptiveRuntime;

	private:
		/// [EN] Marker type representing a node that has not yet been assigned any work (the default/empty alternative of NodeHandle).
		/// [JP] まだ処理が割り当てられていないノードを表すマーカー型（NodeHandle のデフォルト・空の選択肢）。
		using Placeholder = std::monostate;

		/**
		* [EN]
		* Node handle alternative for a plain static (parameterless)
		* callable that is executed when the node runs.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* ノード実行時に呼び出される、単純な静的（引数なし）の呼び出し
		* 可能オブジェクトを表す NodeHandle の選択肢。
		*/
		struct Static
		{
			/**
			* [EN]
			* Constructs, storing c as the callable to run when this node executes.
			*
			* ---------------------------------------------------------------------
			*
			* [JP]
			* c を、このノード実行時に呼び出す処理として保持して構築する。
			*/
			template<typename C>
			Static(C&& c) :work_(std::forward<C>(c))
			{
				/// No Code
			}

			/// [EN] The work to execute when this node runs.
			/// [JP] このノードが実行される際に呼び出される処理。
			std::function<void()> work_;
		};

		/**
		* [EN]
		* Node handle alternative for work that runs under a preemptive
		* runtime, receiving a JobPreemptiveRuntime& to interact with
		* the scheduler (e.g. to yield).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* プリエンプティブなランタイム上で実行される処理を表す
		* NodeHandle の選択肢。スケジューラと連携（yield など）するための
		* JobPreemptiveRuntime& を受け取る。
		*/
		struct PreemptiveRuntime
		{
			/**
			* [EN]
			* Constructs, storing c as the callable to run against the preemptive runtime.
			*
			* ---------------------------------------------------------------------
			*
			* [JP]
			* c を、プリエンプティブランタイムに対して実行する処理として
			* 保持して構築する。
			*/
			template<typename C>
			PreemptiveRuntime(C&& c) :work_(std::forward<C>(c))
			{
				/// No Code
			}

			/// [EN] The work to execute, given access to the preemptive runtime.
			/// [JP] プリエンプティブランタイムへのアクセスを受け取りながら実行される処理。
			std::function<void(JobPreemptiveRuntime&)> work_;
		};

		/**
		* [EN]
		* Node handle alternative for work that runs under a
		* non-preemptive runtime, receiving a JobNonpreemptiveRuntime&
		* to interact with the scheduler.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 非プリエンプティブなランタイム上で実行される処理を表す
		* NodeHandle の選択肢。スケジューラと連携するための
		* JobNonpreemptiveRuntime& を受け取る。
		*/
		struct NonpreemptiveRuntime
		{
			/**
			* [EN]
			* Constructs, storing c as the callable to run against the non-preemptive runtime.
			*
			* ---------------------------------------------------------------------
			*
			* [JP]
			* c を、非プリエンプティブランタイムに対して実行する処理として
			* 保持して構築する。
			*/
			template<typename C>
			NonpreemptiveRuntime(C&& c) :work_(std::forward<C>(c))
			{
				/// No Code
			}

			/// [EN] The work to execute, given access to the non-preemptive runtime.
			/// [JP] 非プリエンプティブランタイムへのアクセスを受け取りながら実行される処理。
			std::function<void(JobNonpreemptiveRuntime&)> work_;
		};

		/**
		* [EN]
		* Node handle alternative representing a dynamically-built
		* subflow: a nested JobGraph that is populated at runtime by
		* user-supplied work that receives a JobSubflow& builder.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 動的に構築されるサブフローを表す NodeHandle の選択肢。
		* JobSubflow& ビルダーを受け取るユーザー定義の処理によって
		* 実行時に構築される、ネストされた JobGraph を保持する。
		*/
		struct Subflow
		{
			/**
			* [EN]
			* Constructs, storing c as the callable that builds the subgraph at runtime.
			*
			* ---------------------------------------------------------------------
			*
			* [JP]
			* c を、実行時にサブグラフを構築する処理として保持して構築する。
			*/
			template<typename C>
			Subflow(C&& c) :work_(std::forward<C>(c))
			{
				/// No Code
			}

			/// [EN] User-supplied work that builds the subgraph at runtime via the given subflow builder.
			/// [JP] 渡されたサブフロービルダーを通じて、実行時にサブグラフを構築するユーザー定義の処理。
			std::function<void(JobSubflow&)> work_;

			/// [EN] The nested graph populated by work_ when the subflow runs.
			/// [JP] サブフローが実行される際に work_ によって構築されるネストされたグラフ。
			JobGraph subgraph_;
		};

		/**
		* [EN]
		* Node handle alternative for a conditional branch that
		* evaluates to a single successor index, selecting exactly one
		* outgoing edge to follow.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 単一の後続インデックスへ評価される条件分岐を表す NodeHandle の
		* 選択肢であり、辿るべき出力エッジをちょうど 1 つ選択する。
		*/
		struct SingleCondition
		{
			/**
			* [EN]
			* Constructs, storing c as the callable that returns the successor index to follow.
			*
			* ---------------------------------------------------------------------
			*
			* [JP]
			* c を、辿るべき後続インデックスを返す処理として保持して構築する。
			*/
			template <typename C>
			SingleCondition(C&& c) :work_(std::forward<C>(c))
			{
				/// No Code
			}

			/// [EN] Work that returns the index of the single successor edge to follow.
			/// [JP] 辿るべき単一の後続エッジのインデックスを返す処理。
			std::function<Int()> work_;
		};

		/**
		* [EN]
		* Node handle alternative for a conditional branch that
		* evaluates to multiple successor indices, selecting zero or
		* more outgoing edges to follow.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 複数の後続インデックスへ評価される条件分岐を表す NodeHandle の
		* 選択肢であり、辿るべき出力エッジを 0 個以上選択する。
		*/
		struct MultiCondition
		{
			/**
			* [EN]
			* Constructs, storing c as the callable that returns the successor indices to follow.
			*
			* ---------------------------------------------------------------------
			*
			* [JP]
			* c を、辿るべき後続インデックス群を返す処理として保持して構築する。
			*/
			template <typename C>
			MultiCondition(C&& c) :work_(std::forward<C>(c))
			{
				/// No Code
			}

			/// [EN] Work that returns the indices of all successor edges to follow.
			/// [JP] 辿るべきすべての後続エッジのインデックスを返す処理。
			std::function<HybridArray<Int>()> work_;
		};

		/**
		* [EN]
		* Node handle alternative for a module whose underlying
		* JobGraph is owned externally; this node only holds a
		* reference to it.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 内部の JobGraph を外部が所有しているモジュールを表す
		* NodeHandle の選択肢であり、このノードはそれへの参照のみを保持する。
		*/
		struct OwnedModule
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
			OwnedModule(JobGraph& graph);

			/// [EN] Reference to the externally-owned graph this module wraps.
			/// [JP] このモジュールが包む、外部が所有しているグラフへの参照。
			JobGraph& graph_;
		};

		/**
		* [EN]
		* Node handle alternative for a module whose underlying
		* JobGraph is moved into and owned by this node.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 内部の JobGraph がこのノードへムーブされ、所有されるモジュールを
		* 表す NodeHandle の選択肢。
		*/
		struct AdoptedModule
		{
			/**
			* [EN]
			* Constructs, moving graph into this module's ownership.
			*
			* ---------------------------------------------------------------------
			*
			* [JP]
			* graph をこのモジュールの所有権へムーブして構築する。
			*/
			AdoptedModule(JobGraph&& graph);

			/// [EN] The graph owned (adopted) by this module.
			/// [JP] このモジュールが所有（養子化）しているグラフ。
			JobGraph graph_;
		};

		/// [EN] Variant describing the kind of work this node performs; exactly one alternative is active at a time.
		/// [JP] このノードが行う処理の種類を表すバリアント。常にいずれか 1 つの選択肢のみが有効になる。
		using NodeHandle = std::variant
			<
			Placeholder,
			Static,
			PreemptiveRuntime,
			NonpreemptiveRuntime,
			Subflow,
			SingleCondition,
			MultiCondition,
			OwnedModule,
			AdoptedModule
			>;

		/**
		* [EN]
		* Groups the semaphores that must be acquired before this node
		* runs, and the semaphores that must be released after it runs.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* このノードが実行される前に獲得しておく必要があるセマフォと、
		* 実行後に解放する必要があるセマフォをまとめて保持する。
		*/
		struct Semaphores
		{
			/// [EN] Semaphores that must be successfully acquired before this node may execute.
			/// [JP] このノードが実行可能になる前に、獲得に成功している必要があるセマフォ群。
			HybridArray<Semaphore*> acquire_;

			/// [EN] Semaphores that must be released once this node has finished executing.
			/// [JP] このノードの実行が完了した後に解放される必要があるセマフォ群。
			HybridArray<Semaphore*> release_;
		};

	public:
		/// [EN] Index of Placeholder within NodeHandle, used to query/check the node's current handle kind.
		/// [JP] NodeHandle 内における Placeholder のインデックス。ノードの現在のハンドル種別を判定・取得する際に使用する。
		constexpr static auto PLACEHOLDER = GetIndexValue<Placeholder, NodeHandle>;
		
		/// [EN] Index of Static within NodeHandle.
		/// [JP] NodeHandle 内における Static のインデックス。
		constexpr static auto STATIC = GetIndexValue<Static, NodeHandle>;
		
		/// [EN] Index of PreemptiveRuntime within NodeHandle.
		/// [JP] NodeHandle 内における PreemptiveRuntime のインデックス。
		constexpr static auto PREEMPTIVE_RUNTIME = GetIndexValue<PreemptiveRuntime, NodeHandle>;
		
		/// [EN] Index of NonpreemptiveRuntime within NodeHandle.
		/// [JP] NodeHandle 内における NonpreemptiveRuntime のインデックス。
		constexpr static auto NONPREEMPTIVE_RUNTIME = GetIndexValue<NonpreemptiveRuntime, NodeHandle>;
		
		/// [EN] Index of Subflow within NodeHandle.
		/// [JP] NodeHandle 内における Subflow のインデックス。
		constexpr static auto SUBFLOW = GetIndexValue<Subflow, NodeHandle>;
		
		/// [EN] Index of SingleCondition within NodeHandle.
		/// [JP] NodeHandle 内における SingleCondition のインデックス。
		constexpr static auto SINGLE_CONDITION = GetIndexValue<SingleCondition, NodeHandle>;
		
		/// [EN] Index of MultiCondition within NodeHandle.
		/// [JP] NodeHandle 内における MultiCondition のインデックス。
		constexpr static auto MULTI_CONDITION = GetIndexValue<MultiCondition, NodeHandle>;
		
		/// [EN] Index of OwnedModule within NodeHandle.
		/// [JP] NodeHandle 内における OwnedModule のインデックス。
		constexpr static auto OWNED_MODULE = GetIndexValue<OwnedModule, NodeHandle>;
		
		/// [EN] Index of AdoptedModule within NodeHandle.
		/// [JP] NodeHandle 内における AdoptedModule のインデックス。
		constexpr static auto ADOPTED_MODULE = GetIndexValue<AdoptedModule, NodeHandle>;

		/**
		* [EN]
		* Default constructor: leaves every field at its in-class default,
		* including a NodeHandle in the Placeholder (no-work) alternative.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* デフォルトコンストラクタ: NodeHandle が Placeholder（無処理）の
		* 状態を含め、全フィールドをクラス内デフォルト値のままにする。
		*/
		JobNode() = default;

		/**
		* [EN]
		* Constructs a node using a full TaskParams (name + user data),
		* forwarding the remaining args to construct the NodeHandle.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 完全な TaskParams（名前 + ユーザーデータ）を用いてノードを
		* 構築し、残りの args を NodeHandle の構築へ転送する。
		*/
		template<typename... Args>
		JobNode(NState nstate, EState estate, const TaskParams& params, JobTopology* topology, JobNodeBase* parent, Size joinCounter, Args&&... args) : JobNodeBase(nstate, estate, parent, joinCounter), name_(params.name_), data_(params.data_), topology_(topology), handle_(std::forward<Args>(args)...)
		{
			/// No Code
		}

		/**
		* [EN]
		* Constructs a node with no name or user data (using
		* DefaultTaskParams as a marker), forwarding the remaining
		* args to construct the NodeHandle.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 名前もユーザーデータも持たないノードを構築する（マーカーとして
		* DefaultTaskParams を使用）。残りの args を NodeHandle の
		* 構築へ転送する。
		*/
		template<typename... Args>
		JobNode(NState nstate, EState estate, const DefaultTaskParams& defaultTaskParams, JobTopology* topology, JobNodeBase* parent, Size joinCounter, Args&&... args) : JobNodeBase(nstate, estate, parent, joinCounter), topology_(topology), handle_(std::forward<Args>(args)...)
		{
			/// No Code
		}

		/**
		* [EN]
		* Constructs a node using only a string-like name (shorthand for
		* when no user data is needed), forwarding the remaining args
		* to construct the NodeHandle.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 文字列型の名前のみを用いてノードを構築する（ユーザーデータが
		* 不要な場合の簡略形）。残りの args を NodeHandle の構築へ
		* 転送する。
		*/
		template<StringLike S, typename... Args>
		JobNode(NState nstate, EState estate, const S&& name, JobTopology* topology, JobNodeBase* parent, Size joinCounter, Args&&... args) : JobNodeBase(nstate, estate, parent, joinCounter), name_(std::forward<S>(name)), topology_(topology), handle_(std::forward<Args>(args)...)
		{
			/// No Code
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
		* Returns the number of strong dependencies (e.g. unconditional
		* predecessors) this node has.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* このノードが持つ強い依存関係（条件分岐を伴わない先行ノードなど）の
		* 数を返す。
		*/
		Size NumberStrongDependencies()const;

		/**
		* [EN]
		* Returns the number of weak dependencies (e.g. predecessors
		* reached only through a conditional branch) this node has.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* このノードが持つ弱い依存関係（条件分岐を経由してのみ到達される
		* 先行ノードなど）の数を返す。
		*/
		Size NumberWeakDependencies()const;

		/**
		* [EN]
		* Returns this node's display name.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* このノードの表示名を返す。
		*/
		const String& Name()const;

	private:
		/// [EN] Display name of this node.
		/// [JP] このノードの表示名。
		String name_;

		/// [EN] Pointer to arbitrary user-defined data associated with this node. Ownership is not managed here.
		/// [JP] このノードに関連付けられた、任意のユーザー定義データへのポインタ。所有権はここでは管理しない。
		void* data_ = nullptr;

		/// [EN] The topology this node belongs to.
		/// [JP] このノードが属するトポロジー。
		JobTopology* topology_ = nullptr;

		/// [EN] Cached count of successor edges, kept in sync with edges_.
		/// [JP] 後続エッジの数をキャッシュした値。edges_ と同期して保持される。
		Size numberSuccessors_ = 0;

		/// [EN] Outgoing edges to other nodes (e.g. successors), stored inline for up to 4 entries before spilling to the heap.
		/// [JP] 他ノードへの出力エッジ（後続ノードなど）。最大 4 要素まではインラインに格納され、それを超えるとヒープへ格納される。
		HybridArray<JobNode*, 4> edges_;

		/// [EN] The active variant describing what kind of work this node performs.
		/// [JP] このノードが行う処理の種類を表す、現在有効なバリアント値。
		NodeHandle handle_;

		/// [EN] Semaphores to acquire/release around this node's execution; null if the node uses no semaphores.
		/// [JP] このノードの実行前後で獲得・解放するセマフォ群。セマフォを使用しないノードでは null になる。
		std::unique_ptr<Semaphores> semaphores_;

		/**
		* [EN]
		* Returns whether this node's parent has been cancelled,
		* meaning this node should not proceed with execution.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* このノードの親がキャンセルされているかどうかを返す。
		* キャンセルされている場合、このノードは実行を進めるべきではない。
		*/
		Bool IsParentCancelled()const;
		
		/**
		* [EN]
		* Returns whether this node represents a conditional branch
		* (i.e. its handle is SingleCondition or MultiCondition).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* このノードが条件分岐を表すかどうか（すなわちハンドルが
		* SingleCondition または MultiCondition であるか）を返す。
		*/
		Bool IsConditioner()const;

		/**
		* [EN]
		* Attempts to acquire all of this node's required semaphores.
		* If any acquisition must wait, the corresponding waiting jobs
		* are collected into nodes and the call fails as a whole.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* このノードが必要とするすべてのセマフォの獲得を試みる。いずれかの
		* 獲得が待機を要する場合、該当する待機ジョブを nodes に収集し、
		* この呼び出し全体は失敗とする。
		*/
		Bool AcquireAll(HybridArray<JobNode*>& nodes);

		/**
		* [EN]
		* Releases all of this node's held semaphores, collecting any
		* jobs that become runnable as a result into nodes.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* このノードが保持しているすべてのセマフォを解放し、その結果
		* 実行可能になったジョブを nodes に収集する。
		*/
		void ReleaseAll(HybridArray<JobNode*>& nodes);

		/**
		* [EN]
		* Establishes a precedence (dependency) edge from this node to
		* node, making node a successor of this node.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* このノードから node への先行関係（依存関係）のエッジを確立し、
		* node をこのノードの後続として設定する。
		*/
		void Precede(JobNode* node);

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
		void SetUpJoinCounter();

		/**
		* [EN]
		* Removes node from this node's list of successors.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* このノードの後続一覧から node を削除する。
		*/
		void RemoveSuccessors(JobNode* node);

		/**
		* [EN]
		* Removes node from this node's list of predecessors.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* このノードの先行一覧から node を削除する。
		*/
		void RemovePredecessors(JobNode* node);
	};

#ifdef SC_ENABLE_TASK_POOL
	/// [EN] Object pool type used to allocate/recycle JobNode instances, chosen based on whether the platform's atomics for SynchronizedPointer are lock-free.
	/// [JP] JobNode インスタンスの確保・再利用に使用するオブジェクトプール型。プラットフォームの SynchronizedPointer 用アトミックがロックフリーかどうかに基づいて選択される。
	using NodePool = std::conditional_t
		<
		std::atomic<SynchronizedPointer>::is_always_lock_free,
		ObjectPool<JobNode, SynchronizedPointer>,
		ObjectPool<JobNode, PackedSynchronizedPointer<>>
		>;

	/// [EN] Global pool instance from which JobNode objects are allocated/recycled when pooling is enabled.
	/// [JP] プーリングが有効な場合に JobNode オブジェクトを確保・再利用するための、グローバルなプールインスタンス。
	inline NodePool nodePool_;
#endif

	/**
	* [EN]
	* Constructs a new JobNode with the given arguments, using the
	* object pool if SC_ENABLE_TASK_POOL is defined, or plain new
	* otherwise.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 指定された引数で新しい JobNode を構築する。SC_ENABLE_TASK_POOL
	* が定義されている場合はオブジェクトプールを使用し、それ以外の場合は
	* 通常の new を使用する。
	*/
	template<typename... Args>
	__forceinline JobNode* animate(Args&&... args)
	{
#ifdef SC_ENABLE_TASK_POOL
		return nodePool_.Create(std::forward<Args>(args)...);
#else
		return new JobNode(std::forward<Args>(args)...);
#endif
	}

	/**
	* [EN]
	* Releases a JobNode previously created by animate, returning
	* it to the object pool if SC_ENABLE_TASK_POOL is defined, or
	* deleting it otherwise.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* animate で生成された JobNode を解放する。SC_ENABLE_TASK_POOL
	* が定義されている場合はオブジェクトプールへ返却し、それ以外の場合は
	* delete する。
	*/
	__forceinline void recycle(JobNode* node)
	{
#ifdef SC_ENABLE_TASK_POOL
		nodePool_.Recycle(node);
#else
		delete node;
#endif
	}
}