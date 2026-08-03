#include <Runtime/Application/Framework.h>
//#include <Runtime/Application/Engine.h>
#include <FoundationEngine/Utility/Bootstrap.h>

namespace SeedCore
{
	Framework::Framework(Engine& engine, const Bootstrap& boot)
	{
		//engine.Boot(boot);
	}

	Framework::~Framework()
	{

	}

	Int Framework::Run(Engine& engine)
	{
		//engine.MainLoop();
		return 0;
	}
}