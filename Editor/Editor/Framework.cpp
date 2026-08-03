#include <Editor/Editor/Framework.h>
#include <Editor/Editor/Engine.h>
#include <FoundationEngine/Utility/Bootstrap.h>

namespace SeedCore
{
	Framework::Framework(Engine& engine, const Bootstrap& boot)
	{
		engine_ = &engine;
		engine.Boot(boot);
	}

	Framework::~Framework()
	{
		engine_->Shutdown();
		engine_ = nullptr;
	}

	Int Framework::Run()
	{
		engine_->MainLoop();
		return 0;
	}
}