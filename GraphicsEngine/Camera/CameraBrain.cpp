#include <GraphicsEngine/Camera/CameraBrain.h>
#include <FoundationEngine/World/Actor/Actor.h>
#include <FoundationEngine/World/World.h>
#include <FoundationEngine/World/ECS/Component/Position.h>
#include <FoundationEngine/World/ECS/Component/Bounds.h>

namespace SeedCore
{
	void CameraBrain::OnLateTick(Float elapsedTime)
	{
		if (shakeTime_ < shakeDuration_)
		{
			shakeTime_ += elapsedTime;
			Float falloff = Clamp(1.0f - shakeTime_ / shakeDuration_, 0.0f, 1.0f);
			Float strength = shakeAmplitude_ * falloff * falloff;
			Float phase = shakeTime_ * shakeFrequency_ * 6.28318530718f;
			shakeAngles_.x = strength * (Sin(phase) * 0.6f + Sin(phase * 2.31f + 1.7f) * 0.4f);
			shakeAngles_.y = strength * (Sin(phase * 1.13f + 3.1f) * 0.6f + Sin(phase * 2.57f + 4.9f) * 0.4f);
			shakeAngles_.z = strength * (Sin(phase * 0.87f + 5.3f) * 0.6f + Sin(phase * 2.03f + 2.2f) * 0.4f) * 0.5f;
		}
		else
		{
			shakeAngles_ = Vector3::Zero;
		}

		if (mode_ == CameraBrainMode::Free || mode_ == CameraBrainMode::Cinematic)
		{
			previousMode_ = mode_;
			snap_ = false;
			return;
		}

		Position* position = GetWorld().GetComponent<Position>(GetActor().GetEntity());
		if (!position)
		{
			return;
		}
		Vector3 eye(position->x_, position->y_, position->z_);

		Actor mainTarget = (mainTarget_ != 0) ? GetWorld().FindActor(mainTarget_) : Actor();
		Vector3 mainCenter = Vector3::Zero;
		Float mainRadius = 0.0f;
		if (mainTarget)
		{
			const Matrix& worldMatrix = mainTarget.WorldMatrix();
			const Bounds* bounds = mainTarget.GetComponent<Bounds>();
			mainCenter = bounds ? Vector3::Transform(bounds->center_, worldMatrix) : worldMatrix.Translation();
			mainRadius = bounds ? Vector3::TransformNormal(bounds->extent_, worldMatrix).Length() : 0.0f;
		}

		Actor subTarget = (subTarget_ != 0) ? GetWorld().FindActor(subTarget_) : Actor();
		Vector3 subCenter = Vector3::Zero;
		Float subRadius = 0.0f;
		if (subTarget)
		{
			const Matrix& worldMatrix = subTarget.WorldMatrix();
			const Bounds* bounds = subTarget.GetComponent<Bounds>();
			subCenter = bounds ? Vector3::Transform(bounds->center_, worldMatrix) : worldMatrix.Translation();
			subRadius = bounds ? Vector3::TransformNormal(bounds->extent_, worldMatrix).Length() : 0.0f;
		}

		if (mode_ == CameraBrainMode::Lookat)
		{
			previousMode_ = mode_;
			snap_ = false;
			if (mainTarget)
			{
				Vector3 look = mainCenter - eye;
				if (look.LengthSquared() > 1e-8f)
				{
					look.Normalize();
					direction_ = look;
				}
			}
			return;
		}

		Bool lockonFraming = mode_ == CameraBrainMode::Lockon && mainTarget && subTarget;
		Vector3 mainToSub = subCenter - mainCenter;
		Float axisYaw = (lockonFraming && mainToSub.x * mainToSub.x + mainToSub.z * mainToSub.z > 1e-6f) ? ToDegrees(Atan2(mainToSub.x, mainToSub.z)) : lockonAxisYaw_;

		if (!orbitInitialized_ || mode_ != previousMode_)
		{
			Vector3 currentDirection = direction_;
			currentDirection.Normalize();
			Float currentYaw = ToDegrees(Atan2(currentDirection.x, currentDirection.z));
			Float currentPitch = ToDegrees(Asin(Clamp(-currentDirection.y, -1.0f, 1.0f)));

			orbitYaw_ = currentYaw;
			orbitPitch_ = Clamp(currentPitch, -89.0f, 89.0f);
			orbitCenter_ = eye + currentDirection * distance_;
			if (mode_ == CameraBrainMode::Lockon)
			{
				lockonAxisYaw_ = lockonFraming ? axisYaw : currentYaw;
				orbitYaw_ = currentYaw - lockonAxisYaw_;
				axisYaw = lockonAxisYaw_;
			}
			orbitInitialized_ = true;
			previousMode_ = mode_;
		}

		if ((mode_ == CameraBrainMode::Follow || mode_ == CameraBrainMode::Lockon) && !mainTarget)
		{
			snap_ = false;
			return;
		}

		Vector3 pivot = mainTarget ? mainCenter : orbitCenter_;
		Float distance = distance_;
		Float yaw = orbitYaw_;
		Bool damped = mode_ == CameraBrainMode::Follow;

		if (mode_ == CameraBrainMode::Lockon)
		{
			if (lockonFraming)
			{
				Float blend = (snap_ || damping_ <= 0.0f) ? 1.0f : 1.0f - std::exp(-elapsedTime / damping_);
				lockonAxisYaw_ += std::remainder(axisYaw - lockonAxisYaw_, 360.0f) * blend;

				Float centerDistance = mainToSub.Length();
				Vector3 groupCenter = mainCenter;
				Float groupRadius = mainRadius;
				if (centerDistance + mainRadius <= subRadius)
				{
					groupCenter = subCenter;
					groupRadius = subRadius;
				}
				else if (centerDistance + subRadius > mainRadius)
				{
					groupRadius = (centerDistance + mainRadius + subRadius) * 0.5f;
					groupCenter = mainCenter + mainToSub * ((groupRadius - mainRadius) / centerDistance);
				}

				Float halfVertical = ToRadians(fieldOfView_) * 0.5f;
				Float halfHorizontal = std::atan(std::tan(halfVertical) * aspectRatio_);
				Float halfNarrow = Min(halfVertical, halfHorizontal);

				pivot = groupCenter;
				distance = Max(distance_, groupRadius / Sin(halfNarrow));
			}
			else
			{
				damped = true;
			}
			yaw = lockonAxisYaw_ + orbitYaw_;
		}

		Float yawRadian = ToRadians(yaw);
		Float pitchRadian = ToRadians(orbitPitch_);
		Vector3 forward(Cos(pitchRadian) * Sin(yawRadian), -Sin(pitchRadian), Cos(pitchRadian) * Cos(yawRadian));
		Vector3 desired = pivot - forward * distance;

		Vector3 next = desired;
		if (damped && !snap_ && damping_ > 0.0f)
		{
			next = Vector3::Lerp(eye, desired, 1.0f - std::exp(-elapsedTime / damping_));
		}
		snap_ = false;

		position->x_ = next.x;
		position->y_ = next.y;
		position->z_ = next.z;

		Vector3 look = pivot - next;
		if (look.LengthSquared() > 1e-8f)
		{
			look.Normalize();
			direction_ = look;
		}
	}

	void CameraBrain::Move(const Vector3& delta)
	{
		if (mode_ != CameraBrainMode::Free && mode_ != CameraBrainMode::Lookat && mode_ != CameraBrainMode::Orbit)
		{
			return;
		}

		Vector3 forward = direction_;
		forward.Normalize();
		Vector3 right = Vector3::Up.Cross(forward);
		if (right.LengthSquared() < 1e-6f)
		{
			right = Vector3::Right;
		}
		right.Normalize();
		Vector3 up = forward.Cross(right);
		Vector3 worldDelta = right * delta.x + up * delta.y + forward * delta.z;

		if (mode_ == CameraBrainMode::Orbit)
		{
			orbitCenter_ += worldDelta;
			return;
		}

		Position* position = GetWorld().GetComponent<Position>(GetActor().GetEntity());
		if (!position)
		{
			return;
		}
		position->x_ += worldDelta.x;
		position->y_ += worldDelta.y;
		position->z_ += worldDelta.z;
	}

	void CameraBrain::Turn(Float yaw, Float pitch)
	{
		if (mode_ == CameraBrainMode::Cinematic || mode_ == CameraBrainMode::Lookat)
		{
			return;
		}

		if (mode_ == CameraBrainMode::Free)
		{
			Vector3 direction = direction_;
			direction.Normalize();
			Float newYaw = ToRadians(ToDegrees(Atan2(direction.x, direction.z)) + yaw);
			Float newPitch = ToRadians(Clamp(ToDegrees(Asin(Clamp(-direction.y, -1.0f, 1.0f))) + pitch, -89.0f, 89.0f));
			direction_ = Vector3(Cos(newPitch) * Sin(newYaw), -Sin(newPitch), Cos(newPitch) * Cos(newYaw));
			return;
		}

		orbitYaw_ += yaw;
		orbitPitch_ = Clamp(orbitPitch_ + pitch, -89.0f, 89.0f);
	}

	void CameraBrain::Teleport(const Vector3& position, const Vector3& direction)
	{
		Position* current = GetWorld().GetComponent<Position>(GetActor().GetEntity());
		if (current)
		{
			current->x_ = position.x;
			current->y_ = position.y;
			current->z_ = position.z;
		}

		if (direction.LengthSquared() > 1e-8f)
		{
			direction_ = direction;
			direction_.Normalize();
		}

		orbitInitialized_ = false;
		snap_ = true;
		cut_ = true;
	}

	void CameraBrain::Shake(Float amplitude, Float duration, Float frequency)
	{
		if (duration <= 0.0f)
		{
			return;
		}

		Float remaining = 0.0f;
		if (shakeTime_ < shakeDuration_)
		{
			Float falloff = 1.0f - shakeTime_ / shakeDuration_;
			remaining = shakeAmplitude_ * falloff * falloff;
		}
		if (amplitude < remaining)
		{
			return;
		}

		shakeAmplitude_ = amplitude;
		shakeDuration_ = duration;
		shakeFrequency_ = frequency;
		shakeTime_ = 0.0f;
	}
}
