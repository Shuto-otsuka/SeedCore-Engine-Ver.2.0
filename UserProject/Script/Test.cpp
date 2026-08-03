#include "UserProject/Script/Test.h"
#include "../../FoundationEngine/Input/InputSystem.h"

void Test::OnStart()
{

}

void Test::OnTick(float elapsedTime)
{
	if (SeedCore::InputSystem::ActionState("Jump", SeedCore::InputSystem::OnPressed))
	{
		test_ = !test_;
	}
}
