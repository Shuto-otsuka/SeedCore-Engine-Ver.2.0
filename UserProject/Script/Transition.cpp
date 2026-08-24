#include "UserProject/Script/Transition.h"
#include <SeedCore/ScScene.h>
#include <SeedCore/ScInput.h>

void Transition::OnStart()
{

}

void Transition::OnTick(float elapsedTime)
{
	if (SeedCore::InputSystem::KeyState(SeedCore::InputSystem::Key::Space, SeedCore::InputSystem::OnPressed))
	{
		SeedCore::Scene::Change("transitiontest_0.scene", 0.3f, 0.3f);
	}
}
