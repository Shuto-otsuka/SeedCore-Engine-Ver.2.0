#include <GraphicsEngine/Camera/OrbitCameraController.h>
#include <FoundationEngine/ECS/Actor.h>
#include <FoundationEngine/ECS/World.h>
#include <FoundationEngine/ECS/Component/Position.h>
#include <FoundationEngine/ECS/Component/Rotation.h>
#include <FoundationEngine/Input/InputSystem.h>

namespace SeedCore
{
	void OrbitCameraController::OnTick(Float elapsedTime)
	{
		Actor& actor = GetActor();
		const Position* position = actor.GetComponent<Position>();
		const Rotation* rotation = actor.GetComponent<Rotation>();
		if (!position || !rotation)
		{
			return;
		}

		if (!initialized_)
		{
			Vector3 eye(position->x, position->y, position->z);
			Vector3 target(targetX_, targetY_, targetZ_);
			Vector3 offset = eye - target;
			distance_ = offset.Length();
			if (distance_ < 0.001f)
			{
				distance_ = 10.0f;
			}

			yaw_ = ToDegrees(Atan2(offset.x, offset.z));
			pitch_ = ToDegrees(Asin(Clamp(offset.y / distance_, -1.0f, 1.0f)));
			initialized_ = true;
		}

		if (InputSystem::MouseState(InputSystem::MouseButton::Right, InputSystem::IsPressed))
		{
			Float dx = InputSystem::MouseDeltaX();
			Float dy = InputSystem::MouseDeltaY();
			yaw_ += dx * rotateSpeed_;
			pitch_ += dy * rotateSpeed_;
			pitch_ = Clamp(pitch_, -89.0f, 89.0f);
		}

		Float wheel = InputSystem::MouseWheelDelta();
		if (Abs(wheel) > 0.0f)
		{
			distance_ -= wheel * zoomSpeed_;
			distance_ = Clamp(distance_, minDistance_, maxDistance_);
		}

		Float yawRad = ToRadians(yaw_);
		Float pitchRad = ToRadians(pitch_);
		Float cosPitch = Cos(pitchRad);

		Vector3 offset(Sin(yawRad) * cosPitch, Sin(pitchRad), Cos(yawRad) * cosPitch);

		Vector3 target(targetX_, targetY_, targetZ_);
		Vector3 eye = target + offset * distance_;

		Position* mutablePosition = const_cast<Position*>(position);
		mutablePosition->x = eye.x;
		mutablePosition->y = eye.y;
		mutablePosition->z = eye.z;

		Vector3 direction = target - eye;
		direction.Normalize();
		Float rotationY = ToDegrees(Atan2(direction.x, direction.z));
		Float rotationX = ToDegrees(Asin(-direction.y));

		Rotation* mutableRotation = const_cast<Rotation*>(rotation);
		mutableRotation->x = rotationX;
		mutableRotation->y = rotationY;
		mutableRotation->z = 0.0f;
	}

	void OrbitCameraController::OnLateTick(Float elapsedTime)
	{

	}
}
