#include <FoundationEngine/ECS/CompoundCommand.h>

namespace SeedCore
{
	/**
	* [EN]
	* Appends command as the next sub-command.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* command を次のサブコマンドとして追加する。
	*/
	void CompoundCommand::Add(ResourcePtr<Command> command)
	{
		commands_.push_back(std::move(command));
	}

	/**
	* [EN]
	* Returns whether no sub-command has been added.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* サブコマンドが1つも追加されていないかを返す。
	*/
	Bool CompoundCommand::Empty()const
	{
		return commands_.empty();
	}

	/**
	* [EN]
	* Re-applies every sub-command, in the order they were added.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 全サブコマンドを、追加された順に再適用する。
	*/
	void CompoundCommand::Redo()
	{
		for (Size index = 0; index < commands_.size(); ++index)
		{
			commands_[index]->Redo();
		}
	}

	/**
	* [EN]
	* Reverts every sub-command, in reverse of the order they were added.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 全サブコマンドを、追加された順の逆順で取り消す。
	*/
	void CompoundCommand::Undo()
	{
		for (Size index = commands_.size(); index > 0; --index)
		{
			commands_[index - 1]->Undo();
		}
	}
}
