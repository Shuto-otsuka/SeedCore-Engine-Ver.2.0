#include "UserProject/EntryPoint/EntryGame.h"
#include <FoundationEngine/ECS/World.h>

void SC_SetImGuiContext(ImGuiContext* context)
{
	ImGui::SetCurrentContext(context);
}

void SC_OnGameLoad(SeedCore::World& world)
{
	/// No Code
}

void SC_OnGameUnload(SeedCore::World& world)
{
	/// No Code
}
