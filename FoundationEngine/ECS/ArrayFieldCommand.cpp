#include <FoundationEngine/ECS/ArrayFieldCommand.h>

namespace SeedCore
{
	ArrayAppendCommand::ArrayAppendCommand(std::function<void()> add, std::function<void(Size)> remove, Size index)
		: add_(std::move(add)), remove_(std::move(remove)), index_(index)
	{
		/// No Code
	}

	void ArrayAppendCommand::Redo()
	{
		add_();
	}

	void ArrayAppendCommand::Undo()
	{
		remove_(index_);
	}

	PayloadArrayCommand::PayloadArrayCommand(std::function<void()> add, std::function<void(Size)> remove, std::function<void*()> lastPtr, Size index, Int value, Bool addOnRedo)
		: add_(std::move(add)), remove_(std::move(remove)), lastPtr_(std::move(lastPtr)), index_(index), value_(value), addOnRedo_(addOnRedo)
	{
		/// No Code
	}

	void PayloadArrayCommand::Redo()
	{
		if (addOnRedo_)
		{
			Append();
		}
		else
		{
			Remove();
		}
	}

	void PayloadArrayCommand::Undo()
	{
		if (addOnRedo_)
		{
			Remove();
		}
		else
		{
			Append();
		}
	}

	/**
	* [EN]
	* Appends a new element via add_, then writes value_ into it via
	* lastPtr_.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* add_経由で新しい要素を追加し、lastPtr_経由でそこへvalue_を書き込む。
	*/
	void PayloadArrayCommand::Append()
	{
		add_();

		if (!lastPtr_)
		{
			return;
		}

		void* pointer = lastPtr_();
		if (pointer)
		{
			*static_cast<Int*>(pointer) = value_;
		}
	}

	/**
	* [EN]
	* Removes the element at index_ via remove_.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* remove_経由でindex_の要素を削除する。
	*/
	void PayloadArrayCommand::Remove()
	{
		remove_(index_);
	}
}
