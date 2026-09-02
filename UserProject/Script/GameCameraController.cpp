#include "GameCameraController.h"

#include <SeedCore/ScInput.h>
#include <SeedCore/ScMath.h>
#include <SeedCore/ScComponent.h>

using namespace SeedCore;

void GameCameraController::OnStart()
{

}

void GameCameraController::OnTick(float elapsedTime)
{
	Vector2 cameraDirection = InputSystem::ActionAxis2D("CameraMove");
}