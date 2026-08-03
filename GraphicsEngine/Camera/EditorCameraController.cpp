#include <GraphicsEngine/Camera/EditorCameraController.h>
#include <GraphicsEngine/Camera/EditorCamera.h>
#include <FoundationEngine/Input/InputSystem.h>

namespace SeedCore
{
	void EditorCameraController::Update(EditorCamera& camera, Float deltaTime)
	{
		if (!initialized_)
		{
			Vector3 direction = camera.Focus() - camera.Eye();
			direction.Normalize();

			Vector3 right = Vector3::Up.Cross(direction);

			if (right.LengthSquared() < 1e-6f)
			{
				right = Vector3::Right;
			}
			right.Normalize();

			Vector3 up = direction.Cross(right);
			up.Normalize();

			Matrix rotationMatrix = Matrix::Identity;
			rotationMatrix._11 = right.x; 
			rotationMatrix._12 = right.y; 
			rotationMatrix._13 = right.z;
			rotationMatrix._21 = up.x;      
			rotationMatrix._22 = up.y;    
			rotationMatrix._23 = up.z;
			rotationMatrix._31 = direction.x;
			rotationMatrix._32 = direction.y;
			rotationMatrix._33 = direction.z;

			rotation_ = Quaternion::CreateFromRotationMatrix(rotationMatrix);
			rotation_.Normalize();
			initialized_ = true;
		}

		if (InputSystem::MouseState(InputSystem::MouseButton::Right, InputSystem::IsPressed))
		{
			Float dx = InputSystem::MouseDeltaX();
			Float dy = InputSystem::MouseDeltaY();

			if (Abs(dx) > 0.0f || Abs(dy) > 0.0f)
			{
				Quaternion pitchDelta = Quaternion::CreateFromAxisAngle(Vector3::Right, ToRadians(-dy * rotateSpeed_));
				Quaternion yawDelta = Quaternion::CreateFromAxisAngle(Vector3::Up, ToRadians(dx * rotateSpeed_));

				rotation_ = pitchDelta * rotation_ * yawDelta;
				rotation_.Normalize();
			}

			Vector3 forward = Vector3::Transform(Vector3::Forward, rotation_);
			Vector3 right = Vector3::Transform(Vector3::Right, rotation_);
			Vector3 up = Vector3::Transform(Vector3::Up, rotation_);
			Vector3 eye = camera.Eye();
			Float speed = moveSpeed_ * deltaTime;

			/// [JP] Shift 押下中は移動速度に倍率を掛ける（Unreal風の高速移動）。
			if (InputSystem::KeyState(InputSystem::Key::Shift))
			{
				speed *= shiftSpeedMultiplier_;
			}

			if (InputSystem::KeyState(InputSystem::Key::W))
			{
				eye += forward * speed;
			}
			if (InputSystem::KeyState(InputSystem::Key::S))
			{
				eye -= forward * speed;
			}
			if (InputSystem::KeyState(InputSystem::Key::A))
			{
				eye += right * speed;
			}
			if (InputSystem::KeyState(InputSystem::Key::D))
			{
				eye -= right * speed;
			}
			if (InputSystem::KeyState(InputSystem::Key::E))
			{
				eye += up * speed;
			}
			if (InputSystem::KeyState(InputSystem::Key::Q))
			{
				eye -= up * speed;
			}

			camera.Eye(eye);
			camera.Focus(eye + forward);
			camera.Up(up);
		}

		/// [JP] 中ボタンドラッグでパン（Unreal風）。掴んだ景色が指に追従する向き。
		if (InputSystem::MouseState(InputSystem::MouseButton::Middle, InputSystem::IsPressed))
		{
			Float dx = InputSystem::MouseDeltaX();
			Float dy = InputSystem::MouseDeltaY();

			if (Abs(dx) > 0.0f || Abs(dy) > 0.0f)
			{
				Vector3 forward = Vector3::Transform(Vector3::Forward, rotation_);
				Vector3 right = Vector3::Transform(Vector3::Right, rotation_);
				Vector3 up = Vector3::Transform(Vector3::Up, rotation_);
				Vector3 eye = camera.Eye();

				Float pan = panSpeed_;
				if (InputSystem::KeyState(InputSystem::Key::Shift))
				{
					pan *= shiftSpeedMultiplier_;
				}

				eye += right * dx * pan;
				eye += up * dy * pan;

				camera.Eye(eye);
				camera.Focus(eye + forward);
				camera.Up(up);
			}
		}

		Float wheel = InputSystem::MouseWheelDelta();
		if (Abs(wheel) > 0.0f)
		{
			Vector3 forward = Vector3::Transform(Vector3::Forward, rotation_);
			Vector3 eye = camera.Eye();

			Float scroll = scrollSpeed_;
			if (InputSystem::KeyState(InputSystem::Key::Shift))
			{
				scroll *= shiftSpeedMultiplier_;
			}

			eye += forward * wheel * scroll;
			camera.Eye(eye);
			camera.Focus(eye + forward);
		}
	}

	void EditorCameraController::MoveSpeed(Float speed)
	{
		moveSpeed_ = speed;
	}

	void EditorCameraController::RotateSpeed(Float speed)
	{
		rotateSpeed_ = speed;
	}

	void EditorCameraController::ScrollSpeed(Float speed)
	{
		scrollSpeed_ = speed;
	}

	void EditorCameraController::PanSpeed(Float speed)
	{
		panSpeed_ = speed;
	}

	void EditorCameraController::ShiftSpeedMultiplier(Float multiplier)
	{
		shiftSpeedMultiplier_ = multiplier;
	}

	Float EditorCameraController::MoveSpeed()const
	{
		return moveSpeed_;
	}

	Float EditorCameraController::RotateSpeed()const
	{
		return rotateSpeed_;
	}

	Float EditorCameraController::ScrollSpeed()const
	{
		return scrollSpeed_;
	}

	Float EditorCameraController::PanSpeed()const
	{
		return panSpeed_;
	}

	Float EditorCameraController::ShiftSpeedMultiplier()const
	{
		return shiftSpeedMultiplier_;
	}
}
