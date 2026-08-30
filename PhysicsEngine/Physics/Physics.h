#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/ECS/EcsID.h>
#include <PhysicsEngine/JoltPhysics/JoltShapePool.h>
#include <PhysicsEngine/JoltPhysics/JoltConstraintPool.h>

namespace SeedCore
{
	class JoltManager;

	struct CharacterDesc
	{
		Vector3 position_ = { 0.0f, 0.0f, 0.0f };
		Quaternion rotation_ = Quaternion::Identity;
		Float radius_ = 0.3f;
		Float height_ = 1.8f;
		Float maxSlopeAngle_ = ToRadians(50.0f);
		Float mass_ = 70.0f;
		Float maxStrength_ = 100.0f;
		JPH::ObjectLayer layer_ = 0;
		EntityID userData_ = 0;
	};

	struct RigidbodyDesc
	{
		ShapeHandle shape_;
		Vector3 position_ = { 0.0f, 0.0f, 0.0f };
		Quaternion rotation_ = Quaternion::Identity;
		JPH::EMotionType motionType_ = JPH::EMotionType::Dynamic;
		JPH::ObjectLayer layer_ = 0;
		Float mass_ = 1.0f;
		Float linearDamping_ = 0.05f;
		Float angularDamping_ = 0.05f;
		Float friction_ = 0.2f;
		Float restitution_ = 0.0f;
		Float gravityFactor_ = 1.0f;
		JPH::EAllowedDOFs allowedDOFs_ = JPH::EAllowedDOFs::All;
		EntityID userData_ = 0;
		Bool isSensor_ = false;
	};

	struct HingeJointDesc
	{
		Vector3 anchor_ = { 0.0f, 0.0f, 0.0f };
		Vector3 axis_ = { 0.0f, 1.0f, 0.0f };
		Bool useLimits_ = false;
		Float minAngle_ = 0.0f;
		Float maxAngle_ = 0.0f;
	};

	struct FixedJointDesc
	{
	};

	struct SpringJointDesc
	{
		Vector3 anchor_ = { 0.0f, 0.0f, 0.0f };
		Float minDistance_ = 0.0f;
		Float maxDistance_ = 0.0f;
		Float frequency_ = 2.0f;
		Float damping_ = 0.5f;
	};

	struct SliderJointDesc
	{
		Vector3 axis_ = { 1.0f, 0.0f, 0.0f };
		Bool useLimits_ = false;
		Float minDistance_ = 0.0f;
		Float maxDistance_ = 0.0f;
	};

	struct RaycastHit
	{
		Vector3 position_ = { 0.0f, 0.0f, 0.0f };
		Vector3 normal_ = { 0.0f, 0.0f, 0.0f };
		Float distance_ = 0.0f;
		EntityID entityID_ = 0;
	};

	struct RaycastHit2D
	{
		Vector2 position_ = { 0.0f, 0.0f };
		Vector2 normal_ = { 0.0f, 0.0f };
		Float distance_ = 0.0f;
		EntityID entityID_ = 0;
	};

	struct SoftbodyDesc
	{
		DynamicArray<Vector3> positions_;
		DynamicArray<Uint32> indices_;
		Vector3 position_ = { 0.0f, 0.0f, 0.0f };
		Quaternion rotation_ = Quaternion::Identity;
		JPH::ObjectLayer layer_ = 0;
		Float edgeCompliance_ = 0.0f;
		Float shearCompliance_ = 0.0f;
		Float bendCompliance_ = 0.0f;
		Uint32 numIterations_ = 5;
		Float linearDamping_ = 0.1f;
		Float pressure_ = 0.0f;
		Float friction_ = 0.2f;
		Float restitution_ = 0.0f;
		Float gravityFactor_ = 1.0f;
	};

	class Physics
	{
	public:
		Physics();
		~Physics() = default;

	public:
		ShapeHandle CreateBoxShape(const Vector3& size, const Vector3& center = { 0.0f, 0.0f, 0.0f });

		ShapeHandle CreateSphereShape(Float radius);

		ShapeHandle CreateCapsuleShape(Float height, Float radius);

		ShapeHandle CreateCylinderShape(Float height, Float radius);

		ShapeHandle CreateRectShape(const Vector2& size, const Vector2& center = { 0.0f, 0.0f });

		ShapeHandle CreateCircleShape(Float radius, const Vector2& center = { 0.0f, 0.0f });

		ShapeHandle CreateMeshShape(Uint32 assetID, const DynamicArray<Vector3>& positions, const DynamicArray<Uint32>& indices);

		ShapeHandle CreateConvexShape(Uint32 assetID, const DynamicArray<Vector3>& positions);

		void ReleaseShape(ShapeHandle handle);

	public:
		JPH::Ref<JPH::CharacterVirtual> CreateCharacter(const CharacterDesc& desc);

		Bool SetCharacterHeight(JPH::CharacterVirtual* character, Float height, Float radius);

		void UpdateCharacter(JPH::CharacterVirtual* character, Float elapsedTime, Float maxSlopeAngle, Float stepHeight);

		void Refresh(JPH::CharacterVirtual* character);

		void DestroyCharacter(JPH::Ref<JPH::CharacterVirtual>& character);

		Vector3 GetGravity()const;

	public:
		JPH::BodyID CreateRigidbody(const RigidbodyDesc& desc);

		JPH::BodyID CreateSoftbody(const SoftbodyDesc& desc);

		void SetBodyShape(JPH::BodyID bodyID, ShapeHandle shape);

		void DestroyBody(JPH::BodyID bodyID);

		void GetBodyTransform(JPH::BodyID bodyID, Vector3& outPosition, Quaternion& outRotation)const;

		void GetVertexPosition(JPH::BodyID bodyID, DynamicArray<Vector3>& outPositions)const;

		EntityID GetBodyEntityID(JPH::BodyID bodyID)const;

	public:
		ConstraintHandle CreateHingeJoint(JPH::BodyID bodyA, JPH::BodyID bodyB, const HingeJointDesc& desc);

		ConstraintHandle CreateFixedJoint(JPH::BodyID bodyA, JPH::BodyID bodyB, const FixedJointDesc& desc);

		ConstraintHandle CreateSpringJoint(JPH::BodyID bodyA, JPH::BodyID bodyB, const SpringJointDesc& desc);

		ConstraintHandle CreateSliderJoint(JPH::BodyID bodyA, JPH::BodyID bodyB, const SliderJointDesc& desc);

		void DestroyJoint(ConstraintHandle handle);

	public:
		Bool Raycast(const Vector3& origin, const Vector3& direction, Float maxDistance, RaycastHit& outHit, Uint32 layerMask = 0xFFFFFFFF)const;

		Bool Spherecast(const Vector3& origin, Float radius, const Vector3& direction, Float maxDistance, RaycastHit& outHit, Uint32 layerMask = 0xFFFFFFFF)const;

		DynamicArray<EntityID> Overlap(ShapeHandle shape, const Vector3& position, const Quaternion& rotation, Uint32 layerMask = 0xFFFFFFFF)const;

		Bool Raycast2D(const Vector2& origin, const Vector2& direction, Float maxDistance, RaycastHit2D& outHit, Float z = 0.0f, Uint32 layerMask = 0xFFFFFFFF)const;

		Bool Circlecast2D(const Vector2& origin, Float radius, const Vector2& direction, Float maxDistance, RaycastHit2D& outHit, Float z = 0.0f, Uint32 layerMask = 0xFFFFFFFF)const;

		DynamicArray<EntityID> Overlap2D(ShapeHandle shape, const Vector2& position, Float rotation, Float z = 0.0f, Uint32 layerMask = 0xFFFFFFFF)const;

	private:
		ConstraintHandle CreateConstraint(JPH::BodyID bodyA, JPH::BodyID bodyB, const JPH::TwoBodyConstraintSettings& settings);

		Bool PassesLayerMask(JPH::BodyID bodyID, Uint32 layerMask)const;

	private:
		JoltManager& joltPhysics_;
	};
}