#pragma once
#include <FoundationEngine/Prelude.h>
#include <AIEngine/CharacterAI/BehaviorTree/BehaviorTreeData.h>

namespace SeedCore
{
	enum class BTreeNodeResult
	{
		Success,
		Running,
		Failure,
		Aborted,
	};

	class BTreeNode
	{
	public:
		BTreeNode() = default;
		virtual ~BTreeNode() = default;

		virtual void Enter() = 0;

		virtual BTreeNodeResult Tick() = 0;

		virtual void Exit() = 0;
	};

	class BTreeNodeBase :public BTreeNode
	{
	protected:
		explicit BTreeNodeBase(NodeID id);

		virtual void Enter()override;

		virtual BTreeNodeResult Tick()override;

		virtual void Exit()override;

	protected:
		BTreeNodeResult nodeResult_ = BTreeNodeResult::Failure;
	};
}