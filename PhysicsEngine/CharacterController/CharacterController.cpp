#include <PhysicsEngine/CharacterController/CharacterController.h>
#include <PhysicsEngine/Physics/Physics.h>
#include <PhysicsEngine/JoltPhysics/JoltLayerdef.h>
#include <FoundationEngine/ECS/Actor.h>
#include <FoundationEngine/ECS/World.h>
#include <FoundationEngine/ECS/Component/Position.h>
#include <FoundationEngine/ECS/Component/Rotation.h>

namespace SeedCore
{
	void CharacterController::OnAwake()
	{
		Actor& actor = GetActor();

		const Position* position = actor.GetComponent<Position>();
		const Rotation* rotation = actor.GetComponent<Rotation>();

		CharacterDesc desc;
		desc.position_ = position ? Vector3(position->x, position->y, position->z) : Vector3(0.0f, 0.0f, 0.0f);
		desc.rotation_ = rotation ? Quaternion::CreateFromYawPitchRoll(ToRadians(rotation->y), ToRadians(rotation->x), ToRadians(rotation->z)) : Quaternion::Identity;
		desc.radius_ = radius_;
		desc.height_ = height_;
		desc.maxSlopeAngle_ = ToRadians(maxSlopeAngle_);
		desc.mass_ = mass_;
		desc.maxStrength_ = pushForce_;
		desc.layer_ = Layers::Pack(Layers::DYNAMIC, actor.GetLayer());
		desc.userData_ = static_cast<EntityID>(actor.GetEntity().GetHandle().index_);

		character_ = actor.GetPhysics().CreateCharacter(desc);

		characterContactListener_ = new JoltCharacterContactListener();
		characterContactListener_->SetTarget(&actor.GetWorld(), desc.userData_);
		character_->SetListener(characterContactListener_.GetPtr());
	}

	void CharacterController::OnFixedTick(Float elapsedTime)
	{
		if (!character_)
		{
			return;
		}

		if (onCustomMove_.IsBound())
		{
			onCustomMove_.Execute(elapsedTime);
			return;
		}

		Actor& actor = GetActor();

		if (crouch_ && !isCrouched_)
		{
			isCrouched_ = actor.GetPhysics().SetCharacterHeight(character_.GetPtr(), crouchHeight_, radius_);
		}
		else if (!crouch_ && isCrouched_)
		{
			isCrouched_ = !actor.GetPhysics().SetCharacterHeight(character_.GetPtr(), height_, radius_);
		}

		JPH::Vec3 currentVelocity = character_->GetLinearVelocity();
		Vector3 horizontalVelocity(currentVelocity.GetX(), 0.0f, currentVelocity.GetZ());
		Float verticalVelocity = currentVelocity.GetY();

		Vector3 inputDirection = moveDirection_;
		Float inputLength = inputDirection.Length();
		if (inputLength > 0.0001f)
		{
			inputDirection /= inputLength;
		}
		else
		{
			inputDirection = Vector3(0.0f, 0.0f, 0.0f);
			inputLength = 0.0f;
		}

		Vector3 targetHorizontalVelocity = inputDirection * (Min(inputLength, 1.0f) * maxMoveSpeed_);
		Float rate = (inputLength > 0.0001f) ? acceleration_ : deceleration_;

		Vector3 horizontalDelta = targetHorizontalVelocity - horizontalVelocity;
		Float horizontalDeltaLength = horizontalDelta.Length();
		Float maxDelta = rate * elapsedTime;
		if (horizontalDeltaLength > maxDelta)
		{
			horizontalVelocity += horizontalDelta * (maxDelta / horizontalDeltaLength);
		}
		else
		{
			horizontalVelocity = targetHorizontalVelocity;
		}

		Bool grounded = character_->GetGroundState() == JPH::CharacterBase::EGroundState::OnGround;

		if (!grounded)
		{
			horizontalVelocity *= Max(0.0f, 1.0f - airDrag_ * elapsedTime);
		}

		if (grounded && verticalVelocity <= 0.0f)
		{
			JPH::Vec3 groundVelocity = character_->GetGroundVelocity();
			horizontalVelocity.x += groundVelocity.GetX();
			horizontalVelocity.z += groundVelocity.GetZ();
			verticalVelocity = groundVelocity.GetY();
		}

		Vector3 gravity = actor.GetPhysics().GetGravity();
		verticalVelocity += gravity.y * gravityScale_ * elapsedTime;
		verticalVelocity = Max(verticalVelocity, -maxFallSpeed_);

		character_->SetLinearVelocity(JPH::Vec3(horizontalVelocity.x, verticalVelocity, horizontalVelocity.z));

		Vector3 forwardXZ(forwardDirection_.x, 0.0f, forwardDirection_.z);
		Float forwardLength = forwardXZ.Length();
		if (forwardLength > 0.0001f)
		{
			forwardXZ /= forwardLength;

			Quaternion targetRotation = Quaternion::CreateFromYawPitchRoll(Atan2(forwardXZ.x, forwardXZ.z), 0.0f, 0.0f);

			JPH::Quat currentJoltRotation = character_->GetRotation();
			Quaternion currentRotation(currentJoltRotation.GetX(), currentJoltRotation.GetY(), currentJoltRotation.GetZ(), currentJoltRotation.GetW());

			Float dot = Clamp(Abs(currentRotation.Dot(targetRotation)), -1.0f, 1.0f);
			Float angleBetween = 2.0f * Acos(dot);
			Float maxAngle = ToRadians(turnSpeed_) * elapsedTime;

			Quaternion newRotation = (angleBetween <= maxAngle) ? targetRotation : Quaternion::Slerp(currentRotation, targetRotation, maxAngle / angleBetween);

			character_->SetRotation(JPH::Quat(newRotation.x, newRotation.y, newRotation.z, newRotation.w));
		}

		actor.GetPhysics().UpdateCharacter(character_.GetPtr(), elapsedTime, ToRadians(maxSlopeAngle_), maxStepHeight_);

		JPH::RVec3 outPosition = character_->GetPosition();
		JPH::Quat outRotation = character_->GetRotation();

		World& world = actor.GetWorld();
		Entity entity = actor.GetEntity();

		Position* position = world.GetComponent<Position>(entity);
		if (position)
		{
			position->x = static_cast<Float>(outPosition.GetX());
			position->y = static_cast<Float>(outPosition.GetY());
			position->z = static_cast<Float>(outPosition.GetZ());
		}

		Rotation* rotation = world.GetComponent<Rotation>(entity);
		if (rotation)
		{
			const Vector3 euler = Quaternion(outRotation.GetX(), outRotation.GetY(), outRotation.GetZ(), outRotation.GetW()).ToEuler();
			rotation->x = ToDegrees(euler.x);
			rotation->y = ToDegrees(euler.y);
			rotation->z = ToDegrees(euler.z);
		}
	}

	void CharacterController::OnDestroy()
	{
		GetActor().GetPhysics().DestroyCharacter(character_);
		characterContactListener_ = nullptr;
	}

	void CharacterController::SetMoveDirection(const Vector3& moveDirection)
	{
		moveDirection_ = moveDirection;
	}

	const Vector3& CharacterController::GetMoveDirection()const
	{
		return moveDirection_;
	}

	void CharacterController::SetForwardDirection(const Vector3& forwardDirection)
	{
		forwardDirection_ = forwardDirection;
	}

	const Vector3& CharacterController::GetForwardDirection()const
	{
		return forwardDirection_;
	}

	void CharacterController::Jump()
	{
		if (!character_ || character_->GetGroundState() != JPH::CharacterBase::EGroundState::OnGround)
		{
			return;
		}

		JPH::Vec3 velocity = character_->GetLinearVelocity();
		character_->SetLinearVelocity(JPH::Vec3(velocity.GetX(), jumpPower_, velocity.GetZ()));
	}

	void CharacterController::Teleport(const Vector3& position)
	{
		if (!character_)
		{
			return;
		}

		character_->SetPosition(JPH::RVec3(position.x, position.y, position.z));

		Actor& actor = GetActor();
		actor.GetPhysics().Refresh(character_.GetPtr());

		World& world = actor.GetWorld();
		Entity entity = actor.GetEntity();

		Position* positionComponent = world.GetComponent<Position>(entity);
		if (positionComponent)
		{
			positionComponent->x = position.x;
			positionComponent->y = position.y;
			positionComponent->z = position.z;
		}
	}

	Bool CharacterController::OnGround()const
	{
		if (!character_)
		{
			return false;
		}

		return character_->GetGroundState() == JPH::CharacterBase::EGroundState::OnGround;
	}

	Bool CharacterController::OnWall()const
	{
		if (!character_)
		{
			return false;
		}

		for (const JPH::CharacterContact& contact : character_->GetActiveContacts())
		{
			if (contact.mHadCollision && Abs(contact.mContactNormal.Dot(JPH::Vec3::sAxisY())) < wallNormalDotLimit_)
			{
				return true;
			}
		}

		return false;
	}

	Bool CharacterController::OnCeiling()const
	{
		if (!character_)
		{
			return false;
		}

		for (const JPH::CharacterContact& contact : character_->GetActiveContacts())
		{
			if (contact.mHadCollision && contact.mContactNormal.Dot(JPH::Vec3::sAxisY()) < -wallNormalDotLimit_)
			{
				return true;
			}
		}

		return false;
	}

	Bool CharacterController::OnSlope()const
	{
		if (!character_ || character_->GetGroundState() != JPH::CharacterBase::EGroundState::OnGround)
		{
			return false;
		}

		return character_->GetGroundNormal().Dot(JPH::Vec3::sAxisY()) < 0.999f;
	}

	Vector3 CharacterController::GetGroundNormal()const
	{
		if (!character_)
		{
			return Vector3(0.0f, 1.0f, 0.0f);
		}

		JPH::Vec3 normal = character_->GetGroundNormal();
		return Vector3(normal.GetX(), normal.GetY(), normal.GetZ());
	}

	Vector3 CharacterController::GetWallNormal()const
	{
		if (character_)
		{
			for (const JPH::CharacterContact& contact : character_->GetActiveContacts())
			{
				if (contact.mHadCollision && Abs(contact.mContactNormal.Dot(JPH::Vec3::sAxisY())) < wallNormalDotLimit_)
				{
					return Vector3(contact.mContactNormal.GetX(), contact.mContactNormal.GetY(), contact.mContactNormal.GetZ());
				}
			}
		}

		return Vector3(0.0f, 0.0f, 0.0f);
	}

	Vector3 CharacterController::GetCeilingNormal()const
	{
		if (character_)
		{
			for (const JPH::CharacterContact& contact : character_->GetActiveContacts())
			{
				if (contact.mHadCollision && contact.mContactNormal.Dot(JPH::Vec3::sAxisY()) < -wallNormalDotLimit_)
				{
					return Vector3(contact.mContactNormal.GetX(), contact.mContactNormal.GetY(), contact.mContactNormal.GetZ());
				}
			}
		}

		return Vector3(0.0f, 0.0f, 0.0f);
	}

	Bool CharacterController::IsCrouching()const
	{
		return isCrouched_;
	}

	Bool CharacterController::IsFalling()const
	{
		if (!character_ || character_->IsSupported())
		{
			return false;
		}

		return character_->GetLinearVelocity().GetY() < 0.0f;
	}

	Bool CharacterController::IsFlying()const
	{
		if (!character_)
		{
			return false;
		}

		return character_->GetGroundState() == JPH::CharacterBase::EGroundState::InAir;
	}

	Bool CharacterController::IsGrounded()const
	{
		if (!character_)
		{
			return false;
		}

		return character_->IsSupported();
	}

	Bool CharacterController::IsJumping()const
	{
		if (!character_ || character_->IsSupported())
		{
			return false;
		}

		return character_->GetLinearVelocity().GetY() > 0.0f;
	}

	Bool CharacterController::IsRunning()const
	{
		if (!character_ || !character_->IsSupported())
		{
			return false;
		}

		JPH::Vec3 velocity = character_->GetLinearVelocity();
		Float horizontalSpeedSquared = velocity.GetX() * velocity.GetX() + velocity.GetZ() * velocity.GetZ();
		return horizontalSpeedSquared > 0.0001f;
	}

	Bool CharacterController::IsStopped()const
	{
		if (!character_ || !character_->IsSupported())
		{
			return false;
		}

		JPH::Vec3 velocity = character_->GetLinearVelocity();
		Float horizontalSpeedSquared = velocity.GetX() * velocity.GetX() + velocity.GetZ() * velocity.GetZ();
		return horizontalSpeedSquared <= 0.0001f;
	}
}
