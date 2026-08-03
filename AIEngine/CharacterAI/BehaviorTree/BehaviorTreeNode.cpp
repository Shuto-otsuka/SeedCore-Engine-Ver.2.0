#include <AIEngine/CharacterAI/BehaviorTree/BehaviorTreeNode.h>

namespace SeedCore
{
	BTreeNodeBase::BTreeNodeBase(NodeID id)
	{

	}

	void BTreeNodeBase::Enter()
	{

	}

	BTreeNodeResult BTreeNodeBase::Tick()
	{
		return BTreeNodeResult::Success;
	}

	void BTreeNodeBase::Exit()
	{
		/// No Code
	}
}