#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	using NodeID = Uint32;
	using NodeTypeID = Uint32;

	struct BTreeNodeData
	{
		NodeID id_;
		NodeTypeID type_;

		NodeID parent_;
		DynamicArray<NodeID> children_;
	};
}