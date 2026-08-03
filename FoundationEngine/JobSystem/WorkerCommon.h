#pragma once

namespace SeedCore
{
	/// [EN] When non-zero, JobNode instances are allocated from a pooled
	///      allocator instead of directly via new/delete (see JobNodeBase).
	/// [JP] 非ゼロの場合、JobNode インスタンスは new/delete を直接使う代わりに
	///      プールされたアロケータから確保される（JobNodeBase 参照）。
#define SC_ENABLE_TASK_POOL 0

	/// [EN] When non-zero, worker threads use AtomicNotifier instead of
	///      NonblockingNotifier for wait/wake signaling.
	/// [JP] 非ゼロの場合、ワーカースレッドは待機/起床の通知に
	///      NonblockingNotifier ではなく AtomicNotifier を使う。
#define SC_ENABLE_ATOMIC_NOTIFIER 0

	/// [EN] When non-zero, the job system skips try/catch around task
	///      execution (lower overhead, but an exception escaping a task
	///      becomes std::terminate instead of being caught/reported).
	/// [JP] 非ゼロの場合、ジョブシステムはタスク実行時の try/catch を
	///      省略する（オーバーヘッドは下がるが、タスクから漏れた例外は
	///      捕捉/報告されず std::terminate になる）。
#define SC_DISABLE_EXCEPTION_HANDLING 0
}