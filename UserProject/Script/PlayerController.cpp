#include "PlayerController.h"

#include <SeedCore/ScInput.h>
#include <SeedCore/ScMath.h>
#include <SeedCore/ScComponent.h>

using namespace SeedCore;

void PlayerController::OnStart()
{

}

void PlayerController::OnTick(float elapsedTime)
{
	CharacterController* characterController = GetActor().GetComponent<CharacterController>();
	Animator* animator = GetActor().GetComponent<Animator>();
	if (!characterController || !animator)
	{
		return;
	}

	Vector2 inputDirection = InputSystem::ActionAxis2D("PlayerMove");
	Vector3 moveDirection = Vector3(inputDirection.x, 0.0f, inputDirection.y);
	float moveSpeed = moveDirection.Length();

	characterController->SetMoveDirection(moveDirection);

	if (moveSpeed > 1e-4f)
	{
		characterController->SetForwardDirection(moveDirection);
	}

	animator->SetFloat("Speed", moveSpeed);

	if (InputSystem::ActionState("PlayerJump"))
	{
		characterController->Jump();
		animator->SetTrigger("Jump");
	}
}