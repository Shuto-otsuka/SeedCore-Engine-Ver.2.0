#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/Utility/Bitset.h>
#include <functional>

namespace SeedCore
{
	class JobExecutor;

	/**
	* [EN]
	* Schedules a set of systems for one frame by their declared
	* component access: each is added with its read and write signatures
	* (as produced by Query::GetReadSignature / GetWriteSignature) and a
	* task to run. Run builds a JobGraph in which two systems get a
	* dependency edge - forcing them to run in registration order - only
	* when their accesses conflict (write/write, write/read or
	* read/write on the same component); non-conflicting systems are left
	* free to run in parallel on the JobExecutor.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 1フレーム分のシステム群を、宣言されたコンポーネントアクセスに
	* 基づいてスケジュールする: 各システムは read/write シグネチャ
	* （Query::GetReadSignature / GetWriteSignature が生成するもの）と
	* 実行タスクとともに追加する。Run は JobGraph を構築し、2つの
	* システムのアクセスが衝突する場合（同じコンポーネントへの
	* write/write、write/read、read/write）に限り、登録順で走らせる
	* 依存エッジを張る。衝突しないシステムは JobExecutor 上で並列に
	* 走れるようにする。
	*/
	class SEEDCORE_API SystemGraph
	{
	public:
		/**
		* [EN]
		* Default constructor: starts with no registered systems.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* デフォルトコンストラクタ: 登録済みシステムが無い状態から始める。
		*/
		SystemGraph() = default;

		/**
		* [EN]
		* Destructor; uses the compiler-generated default.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* デストラクタ。コンパイラ生成のデフォルトを使用する。
		*/
		~SystemGraph() = default;

		/**
		* [EN]
		* Registers a system: readSignature / writeSignature are the
		* components it reads / writes (used for conflict detection), task
		* is invoked once when Run reaches it. Registration order is the
		* tie-breaker between conflicting systems.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* システムを登録する: readSignature / writeSignature はそれが
		* 読み取る / 書き込むコンポーネント（衝突検出に使う）、task は
		* Run が到達した時に1回呼ばれる。登録順が衝突するシステム間の
		* 優先順位を決める。
		*/
		void Add(const Bitset& readSignature, const Bitset& writeSignature, std::function<void()> task);

		/**
		* [EN]
		* Runs every registered system on executor, honoring the
		* dependency edges implied by conflicting accesses, and blocks
		* until all have finished. Does nothing if no systems are
		* registered.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 登録された全システムを executor 上で、衝突するアクセスが示唆
		* する依存エッジを守りつつ実行し、全て終わるまでブロックする。
		* 登録されたシステムが無ければ何もしない。
		*/
		void Run(JobExecutor& executor);

		/**
		* [EN]
		* Drops every registered system.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 登録された全システムを破棄する。
		*/
		void Clear();

	private:
		/**
		* [EN]
		* One registered system: the components it accesses and the task
		* that runs it.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 登録された1つのシステム: それがアクセスするコンポーネントと、
		* それを走らせるタスク。
		*/
		struct Entry
		{
			/// [EN] Components this system reads.
			/// [JP] このシステムが読み取るコンポーネント。
			Bitset readSignature_;

			/// [EN] Components this system writes.
			/// [JP] このシステムが書き込むコンポーネント。
			Bitset writeSignature_;

			/// [EN] The system's per-frame work.
			/// [JP] このシステムの毎フレームの処理。
			std::function<void()> task_;
		};

		/**
		* [EN]
		* Returns whether two systems' accesses conflict: a write on
		* either side that overlaps a read or write on the other.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 2つのシステムのアクセスが衝突するかを返す: いずれか一方の
		* write が、他方の read または write と重なる場合。
		*/
		static Bool Conflicts(const Entry& first, const Entry& second);

	private:
		/// [EN] Registered systems, in registration order.
		/// [JP] 登録されたシステム。登録順で保持する。
		DynamicArray<Entry> entries_;
	};
}
