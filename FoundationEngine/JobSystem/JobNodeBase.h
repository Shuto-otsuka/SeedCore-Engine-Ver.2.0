#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	/**
	* [EN]
	* Represents the execution state of a job node.
	* The upper 4 bits encode control flags, while the lower 28 bits represent
	* the count of strong (unresolved) dependencies.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* ジョブノードの実行状態を表す構造体。
	* 上位4ビットが制御フラグを、下位28ビットが未解決の強依存関係の数を表す。
	*/
	struct JobNodeState
	{
		/// [EN] Underlying integer type for node state bitfield.
		/// [JP] ノード状態ビットフィールドの基底整数型。
		using NodeStateType = Uint32;

		/// [EN] No flags set; initial default state.
		/// [JP] フラグなし：初期デフォルト状態。
		static constexpr NodeStateType NONE = 0x00000000;

		/// [EN] Node is implicitly anchored; it will not be destroyed when the graph is destroyed.
		/// [JP] 暗黙的にアンカーされた状態。グラフ破棄時にこのノードは破棄されない。
		static constexpr NodeStateType IMPLICITLY_ANCHORED = 0x10000000;
		
		/// [EN] Node has been preempted and is waiting to be resumed.
		/// [JP] ノードがプリエンプトされ、再開待ち状態にある。
		static constexpr NodeStateType PREEMPTED = 0x20000000;
		
		/// [EN] Node should retain its subflow after execution completes.
		/// [JP] 実行完了後もサブフローを保持する。
		static constexpr NodeStateType RETAIN_SUBFLOW = 0x40000000;
		
		/// [EN] Node has already joined its subflow.
		/// [JP] サブフローへの合流が完了している。
		static constexpr NodeStateType JOINED_SUBFLOW = 0x80000000;

		/// [EN] Bitmask to extract the strong dependency counter from the state value (lower 28 bits).
		/// [JP] 状態値から強依存関係カウンタを取り出すためのビットマスク（下位28ビット）。
		static constexpr NodeStateType STRONG_DEPENDENCIES_MASK = 0x0FFFFFFF;
	};

	/// [EN] Convenient alias for the node state type.
	/// [JP] ノード状態型の短縮エイリアス。
	using NState = JobNodeState::NodeStateType;

	/**
	* [EN]
	* Represents the exception and lifecycle state of a job node.
	* The upper byte encodes exception/completion flags; the lower bits are reserved for extensions.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* ジョブノードの例外状態とライフサイクル状態を表す構造体。
	* 上位バイトが例外・完了フラグを保持し、下位ビットは拡張用として予約されている。
	*/
	struct JobExceptionState
	{
		/// [EN] Underlying integer type for exception state bitfield.
		/// [JP] 例外状態ビットフィールドの基底整数型。
		using ExceptionStateType = Uint32;

		/// [EN] No exception has occurred; initial default state.
		/// [JP] 例外なし：初期デフォルト状態。
		static constexpr ExceptionStateType NONE = 0x00000000;

		/// [EN] An exception was thrown during node execution.
		/// [JP] ノードの実行中に例外がスローされた。
		static constexpr ExceptionStateType EXCEPTION = 0x10000000;

		/// [EN] The exception has been caught and handled.
		/// [JP] 例外がキャッチされ処理済みになった。
		static constexpr ExceptionStateType CAUGHT = 0x20000000;

		/// [EN] The node was cancelled before it could execute.
		/// [JP] ノードは実行前にキャンセルされた。
		static constexpr ExceptionStateType CANCELLED = 0x40000000;

		/// [EN] The state is locked; no further exception state transitions are permitted.
		/// [JP] 状態がロックされており、以降の例外状態遷移は禁止される。
		static constexpr ExceptionStateType LOCKED = 0x01000000;

		/// [EN] The node has finished all execution and cleanup.
		/// [JP] ノードの実行とクリーンアップがすべて完了した。
		static constexpr ExceptionStateType FINISHED = 0x02000000;

		/// [EN] Bitmask covering all exception state flag bits (upper byte).
		/// [JP] 例外状態フラグビット全体を覆うビットマスク（上位バイト）。
		static constexpr ExceptionStateType MASK = 0xFF000000;
	};

	/// [EN] Convenient alias for the exception state type.
	/// [JP] 例外状態型の短縮エイリアス。
	using EState = JobExceptionState::ExceptionStateType;

	/**
	* [EN]
	* Base class for all job nodes in the execution graph.
	* Holds the core scheduling state, exception state, parent linkage,
	* join counter, and any propagated exception pointer.
	* All graph-related classes are declared as friends so they can
	* manipulate internal state directly without exposing public setters.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 実行グラフ上のすべてのジョブノードの基底クラス。
	* スケジューリング状態・例外状態・親ノードへのリンク・
	* 合流カウンタ・伝播例外ポインタを保持する。
	* グラフ関連クラスはすべてフレンド宣言されており、
	* パブリックなセッターを介さずに内部状態を直接操作できる。
	*/
	class SEEDCORE_API JobNodeBase
	{
	private:
		friend class JobNode;
		friend class JobGraph;
		friend class JobTask;
		friend class JobTaskView;
		friend class JobTaskflow;
		friend class JobExecutor;
		friend class FlowBuilder;
		friend class JobSubflow;
		friend class JobPreemptiveRuntime;
		friend class JobNonpreemptiveRuntime;

	protected:
		/// [EN] Scheduling state bitfield (control flags + strong dependency counter).
		/// [JP] スケジューリング状態ビットフィールド（制御フラグ＋強依存関係カウンタ）。
		NState nstate_ = JobNodeState::NONE;

		/// [EN] Exception / lifecycle state; atomic because executor threads may update it concurrently.
		/// [JP] 例外・ライフサイクル状態。エグゼキュータスレッドが並行更新するためアトミック。
		std::atomic<EState> estate_ = JobExceptionState::NONE;

		/// [EN] Pointer to the parent node; nullptr if this node is a root.
		/// [JP] 親ノードへのポインタ。ルートノードの場合は nullptr。
		JobNodeBase* parent_ = nullptr;

		/// [EN] Number of predecessors that have not yet notified this node; decremented atomically on join.
		/// [JP] まだ通知していない先行ノードの数。合流時にアトミックにデクリメントされる。
		std::atomic<Size> joinCounter_ = 0;

		/// [EN] Stores an exception propagated from a child or the node itself; nullptr if no exception.
		/// [JP] 子ノードまたはノード自身から伝播した例外を保持する。例外がなければ nullptr。
		std::exception_ptr exceptionPtr_ = nullptr;

	protected:
		/**
		* [EN]
		* Default constructor: leaves every field at its in-class default
		* (unscheduled, no exception, no parent, zero join counter).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* デフォルトコンストラクタ: 全フィールドをクラス内デフォルト値の
		* ままにする（未スケジュール、例外なし、親なし、合流カウンタ0）。
		*/
		JobNodeBase() = default;

		/**
		* [EN]
		* Explicit constructor for initializing all fields at once.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 全フィールドを一括初期化するための明示的コンストラクタ。
		*/
		JobNodeBase(NState nstate, EState estate, JobNodeBase* parent, Size joinCounter);
	};
}