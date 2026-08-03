#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	struct Bootstrap;
	class Engine;

	class Framework :public NonTransferable
	{
	public:
		Framework(Engine& engine, const Bootstrap& boot);
		~Framework();

		Int Run(Engine& engine);
	};
}