#pragma once
#include <FoundationEngine/ECS/Component/ComponentBase.h>
#include <FoundationEngine/ECS/ComponentRegistry.h>

namespace SeedCore
{
	class SEEDCORE_API SeedScript :public ComponentBase
	{
	public:
		virtual ~SeedScript() = default;
	};
}