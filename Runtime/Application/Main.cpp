#include <Runtime/Application/Framework.h>
#include <Runtime/Application/Engine.h>
#include <FoundationEngine/Utility/Bootstrap.h>

int WinMain(HINSTANCE hCurrentInstance, HINSTANCE hPreviousInstance, LPSTR lpCommandLine, int nShowCommand)
{
	wchar_t modulePath[MAX_PATH]{};
	GetModuleFileNameW(nullptr, modulePath, MAX_PATH);
	SetCurrentDirectoryW(std::filesystem::path(modulePath).parent_path().c_str());

	SeedCore::Bootstrap boot{};

	SeedCore::Engine engine;
	SeedCore::Framework framework(engine, boot);

	return framework.Run();
}
