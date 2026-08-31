#include <FoundationEngine/ECS/System/SystemGraph.h>
#include <FoundationEngine/JobSystem/JobExecutor.h>
#include <FoundationEngine/JobSystem/JobTaskflow.h>

namespace SeedCore
{
	/**
	* [EN]
	* Registers a system with its read / write signatures and per-frame
	* task.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* システムを、その read / write シグネチャと毎フレームのタスクと
	* ともに登録する。
	*/
	void SystemGraph::Add(const Bitset& readSignature, const Bitset& writeSignature, std::function<void()> task)
	{
		Entry entry;
		entry.readSignature_ = readSignature;
		entry.writeSignature_ = writeSignature;
		entry.task_ = std::move(task);
		entries_.push_back(std::move(entry));
	}

	/**
	* [EN]
	* Runs every registered system on executor, adding a registration-
	* order dependency edge between any two whose accesses conflict, and
	* blocks until all have finished.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 登録された全システムを executor 上で実行し、アクセスが衝突する
	* 2つの間には登録順の依存エッジを張り、全て終わるまでブロックする。
	*/
	void SystemGraph::Run(JobExecutor& executor)
	{
		if (entries_.empty())
		{
			return;
		}

		JobTaskflow taskflow;

		DynamicArray<JobTask> tasks;
		tasks.reserve(entries_.size());
		for (Size index = 0; index < entries_.size(); ++index)
		{
			tasks.push_back(taskflow.emplace([this, index]()
				{
					entries_[index].task_();
				}));
		}

		/// [EN] For every pair, the earlier-registered system precedes the later one whenever their accesses conflict; independent pairs get no edge and stay parallel.
		/// [JP] 全ペアについて、アクセスが衝突する場合は先に登録されたシステムを後のシステムの前に置く。独立したペアにはエッジを張らず並列のままにする。
		for (Size later = 1; later < entries_.size(); ++later)
		{
			for (Size earlier = 0; earlier < later; ++earlier)
			{
				if (Conflicts(entries_[earlier], entries_[later]))
				{
					tasks[earlier].Precede(tasks[later]);
				}
			}
		}

		executor.Run(taskflow).wait();
	}

	/**
	* [EN]
	* Drops every registered system.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 登録された全システムを破棄する。
	*/
	void SystemGraph::Clear()
	{
		entries_.clear();
	}

	/**
	* [EN]
	* Returns whether two systems' accesses conflict: a write on either
	* side overlapping a read or write on the other.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 2つのシステムのアクセスが衝突するかを返す: いずれか一方の write が
	* 他方の read または write と重なる場合。
	*/
	Bool SystemGraph::Conflicts(const Entry& first, const Entry& second)
	{
		if ((first.writeSignature_ & second.writeSignature_).any())
		{
			return true;
		}

		if ((first.writeSignature_ & second.readSignature_).any())
		{
			return true;
		}

		if ((first.readSignature_ & second.writeSignature_).any())
		{
			return true;
		}

		return false;
	}
}
