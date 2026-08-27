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
		SeedCore::Scene::Change("skeletal_test.scene", 1.0f, 1.0f);
	}
}
