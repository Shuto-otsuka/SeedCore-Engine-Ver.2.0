#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/SeedScript.h>
#include <PhysicsEngine/JoltPhysics/JoltCharacterContactListener.h>

namespace SeedCore
{
	class SEEDCORE_API CharacterController :public SeedScript
	{
	public:
		SC_REFLECTION_CLAMPED_EX("半径", 0.001f, 100.0f)
		Float radius_ = 0.3f;

		SC_REFLECTION_CLAMPED_EX("高さ", 0.001f, 100.0f)
		Float height_ = 1.8f;

		SC_REFLECTION_CLAMPED_EX("質量", 0.001f, 1000.0f)
		Float mass_ = 70.0f;

		SC_REFLECTION_CLAMPED_EX("押し出し力", 0.0f, 10000.0f)
		Float pushForce_ = 100.0f;

		SC_REFLECTION_CLAMPED_EX("最大移動速度", 0.0f, 100.0f)
		Float maxMoveSpeed_ = 5.0f;

		SC_REFLECTION_CLAMPED_EX("加速度", 0.0f, 1000.0f)
		Float acceleration_ = 20.0f;

		SC_REFLECTION_CLAMPED_EX("減速度", 0.0f, 1000.0f)
		Float deceleration_ = 20.0f;

		SC_REFLECTION_CLAMPED_EX("回転速度", 0.0f, 1000.0f)
		Float turnSpeed_ = 10.0f;

		SC_REFLECTION_CLAMPED_EX("空気抵抗", 0.0f, 10.0f)
		Float airDrag_ = 0.0f;

		SC_REFLECTION_CLAMPED_EX("最大斜面角度", 0.0f, 89.0f)
		Float maxSlopeAngle_ = 50.0f;

		SC_REFLECTION_CLAMPED_EX("最大許容段差高", 0.0f, 10.0f)
		Float maxStepHeight_ = 0.4f;

		SC_REFLECTION_FIELD_EX("重力倍率")
		Float gravityScale_ = 1.0f;

		SC_REFLECTION_CLAMPED_EX("最大落下速度", 0.0f, 1000.0f)
		Float maxFallSpeed_ = 50.0f;

		SC_REFLECTION_CLAMPED_EX("ジャンプ力", 0.0f, 1000.0f)
		Float jumpPower_ = 5.0f;

		SC_REFLECTION_FIELD_EX("しゃがみ")
		Bool crouch_ = false;

		SC_REFLECTION_CLAMPED_EX("しゃがみ時の高さ", 0.001f, 100.0f)
		Float crouchHeight_ = 1.0f;

	public:
		void OnAwake();

		void OnFixedTick(Float elapsedTime);

		void OnDestroy();

	public:
		void SetMoveDirection(const Vector3& moveDirection);

		const Vector3& GetMoveDirection()const;

		void SetForwardDirection(const Vector3& forwardDirection);

		const Vector3& GetForwardDirection()const;

	public:
		void Jump();

		void Teleport(const Vector3& position);

	public:
		Bool OnGround()const;

		Bool OnWall()const;

		Bool OnCeiling()const;

		Bool OnSlope()const;

	public:
		Vector3 GetGroundNormal()const;

		Vector3 GetWallNormal()const;

		Vector3 GetCeilingNormal()const;

	public:
		Bool IsCrouching()const;

		Bool IsFalling()const;

		Bool IsFlying()const;

		Bool IsGrounded()const;

		Bool IsJumping()const;

		Bool IsRunning()const;

		Bool IsStopped()const;

	public:
		SinglecastDelegate<void(Float)> onCustomMove_;

	private:
		static constexpr Float wallNormalDotLimit_ = 0.5f;

	private:
		Vector3 moveDirection_ = { 0.0f, 0.0f, 0.0f };

		Vector3 forwardDirection_ = { 0.0f, 0.0f, 1.0f };

		Bool isCrouched_ = false;

		JPH::Ref<JPH::CharacterVirtual> character_;

		JPH::Ref<JoltCharacterContactListener> characterContactListener_;
	};
	REGISTER_COMPONENT(CharacterController, "Physics");
}
