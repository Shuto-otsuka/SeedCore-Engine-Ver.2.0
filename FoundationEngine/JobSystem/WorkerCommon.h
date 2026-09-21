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

	/// [EN] Non-zero skips the try/catch around task execution: less overhead, but an exception escaping a task calls std::terminate.
	/// [JP] 非ゼロならタスク実行時の try/catch を省く。負荷は下がるが、タスクから漏れた例外は std::terminate になる。
#define SC_DISABLE_EXCEPTION_HANDLING 0
}