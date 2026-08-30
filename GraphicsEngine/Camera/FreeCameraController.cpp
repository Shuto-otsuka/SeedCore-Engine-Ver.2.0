#include <GraphicsEngine/Camera/FreeCameraController.h>
#include <FoundationEngine/ECS/Actor.h>
#include <FoundationEngine/ECS/World.h>
#include <FoundationEngine/ECS/Component/Position.h>
#include <FoundationEngine/ECS/Component/Rotation.h>
#include <FoundationEngine/Input/InputSystem.h>

namespace SeedCore
{
	void FreeCameraController::OnTick(Float elapsedTime)
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
			Vector3 direction = Vector3::Forward;

			Float ry = ToRadians(rotation->y_);
			Float rx = ToRadians(rotation->x_);
			Matrix rotMatrix = Matrix::CreateFromYawPitchRoll(ry, rx, 0.0f);
			direction = Vector3::TransformNormal(Vector3::Forward, rotMatrix);
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
			rotationMatrix._21 = -up.x;
			rotationMatrix._22 = -up.y;
			rotationMatrix._23 = -up.z;
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
			Vector3 eye(position->x_, position->y_, position->z_);
			Float speed = moveSpeed_ * elapsedTime;

			/// [JP] Shift 押下中は移動速度に倍率を掛ける（Unreal風の高速移動）。
			if (InputSystem::KeyState(InputSystem::Key::Shift))
			{
				speed *= shiftSpeedMultiplier_;
			}

			if (InputSystem::KeyState(InputSystem::Key::S))
			{
				eye += forward * speed;
			}
			if (InputSystem::KeyState(InputSystem::Key::W))
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
			if (InputSystem::KeyState(InputSystem::Key::Q))
			{
				eye += up * speed;
			}
			if (InputSystem::KeyState(InputSystem::Key::E))
			{
				eye -= up * speed;
			}

			Position* mutablePosition = const_cast<Position*>(position);
			mutablePosition->x_ = eye.x;
			mutablePosition->y_ = eye.y;
			mutablePosition->z_ = eye.z;
		}

		/// [JP] 中ボタンドラッグでパン（Unreal風）。掴んだ景色が指に追従する向き。
		if (InputSystem::MouseState(InputSystem::MouseButton::Middle, InputSystem::IsPressed))
		{
			Float dx = InputSystem::MouseDeltaX();
			Float dy = InputSystem::MouseDeltaY();

			if (Abs(dx) > 0.0f || Abs(dy) > 0.0f)
			{
				Vector3 right = Vector3::Transform(Vector3::Right, rotation_);
				Vector3 up = Vector3::Transform(Vector3::Up, rotation_);
				Vector3 eye(position->x_, position->y_, position->z_);

				Float pan = panSpeed_;
				if (InputSystem::KeyState(InputSystem::Key::Shift))
				{
					pan *= shiftSpeedMultiplier_;
				}

				eye += right * dx * pan;
				eye += up * dy * pan;

				Position* mutablePosition = const_cast<Position*>(position);
				mutablePosition->x_ = eye.x;
				mutablePosition->y_ = eye.y;
				mutablePosition->z_ = eye.z;
			}
		}

		Float wheel = InputSystem::MouseWheelDelta();
		if (Abs(wheel) > 0.0f)
		{
			Vector3 forward = Vector3::Transform(Vector3::Forward, rotation_);
			Vector3 eye(position->x_, position->y_, position->z_);

			Float scroll = scrollSpeed_;
			if (InputSystem::KeyState(InputSystem::Key::Shift))
			{
				scroll *= shiftSpeedMultiplier_;
			}

			eye += forward * wheel * scroll;

			Position* mutablePosition = const_cast<Position*>(position);
			mutablePosition->x_ = eye.x;
			mutablePosition->y_ = eye.y;
			mutablePosition->z_ = eye.z;
		}

		Vector3 forward = Vector3::Transform(Vector3::Forward, rotation_);
		Float yaw = ToDegrees(Atan2(forward.x, forward.z));
		Float pitch = ToDegrees(Asin(-forward.y));

		Rotation* mutableRotation = const_cast<Rotation*>(rotation);
		mutableRotation->x_ = pitch;
		mutableRotation->y_ = yaw;
		mutableRotation->z_ = 0.0f;
	}

	void FreeCameraController::OnLateTick(Float elapsedTime)
	{

	}
}
