#include <GraphicsEngine/Camera/PreviewCameraController.h>
#include <GraphicsEngine/Camera/PreviewCamera.h>
#include <FoundationEngine/Input/InputSystem.h>

namespace SeedCore
{
	void PreviewCameraController::Update(PreviewCamera& camera, Float deltaTime)
	{
		Vector3 focus = camera.Focus();

		if (!initialized_)
		{
			Vector3 toEye = camera.Eye() - focus;
			distance_ = Max(toEye.Length(), 0.0001f);

			Vector3 direction = toEye / distance_;
			pitch_ = ToDegrees(std::asin(Clamp(direction.y, -1.0f, 1.0f)));
			yaw_ = ToDegrees(std::atan2(direction.x, direction.z));
			initialized_ = true;
		}

		Bool orbitHeld = InputSystem::MouseState(InputSystem::MouseButton::Left, InputSystem::IsPressed);
		Bool panHeld = InputSystem::MouseState(InputSystem::MouseButton::Middle, InputSystem::IsPressed);

		if (orbitHeld)
		{
			Float dx = InputSystem::MouseDeltaX();
			Float dy = InputSystem::MouseDeltaY();

			yaw_ += dx * rotateSpeed_;
			pitch_ = Clamp(pitch_ - dy * rotateSpeed_, -89.0f, 89.0f);
		}

		Float yawRadians = ToRadians(yaw_);
		Float pitchRadians = ToRadians(pitch_);

		Vector3 direction;
		direction.x = std::cos(pitchRadians) * std::sin(yawRadians);
		direction.y = std::sin(pitchRadians);
		direction.z = std::cos(pitchRadians) * std::cos(yawRadians);

		if (panHeld)
		{
			Float dx = InputSystem::MouseDeltaX();
			Float dy = InputSystem::MouseDeltaY();

			if (Abs(dx) > 0.0f || Abs(dy) > 0.0f)
			{
				Vector3 right = Vector3::Up.Cross(direction);
				if (right.LengthSquared() < 1e-6f)
				{
					right = Vector3::Right;
				}
				else
				{
					right.Normalize();
				}

				Vector3 up = direction.Cross(right);
				up.Normalize();

				Float pan = panSpeed_ * distance_;
				focus += (-right * dx + up * dy) * pan;
			}
		}

		Float wheel = InputSystem::MouseWheelDelta();
		if (Abs(wheel) > 0.0f)
		{
			distance_ = Max(0.05f, distance_ - wheel * distance_ * zoomSpeed_);
		}

		camera.Focus(focus);
		camera.Eye(focus + direction * distance_);
		camera.Up(Vector3::Up);
	}

	void PreviewCameraController::RotateSpeed(Float speed)
	{
		rotateSpeed_ = speed;
	}

	void PreviewCameraController::ZoomSpeed(Float speed)
	{
		zoomSpeed_ = speed;
	}

	void PreviewCameraController::PanSpeed(Float speed)
	{
		panSpeed_ = speed;
	}

	Float PreviewCameraController::RotateSpeed()const
	{
		return rotateSpeed_;
	}

	Float PreviewCameraController::ZoomSpeed()const
	{
		return zoomSpeed_;
	}

	Float PreviewCameraController::PanSpeed()const
	{
		return panSpeed_;
	}
}
