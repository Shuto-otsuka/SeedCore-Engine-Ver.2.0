#pragma once
#include <FoundationEngine/Prelude.h>
#include <PhysicsEngine/JoltPhysics/JoltShapePool.h>

namespace SeedCore
{
	class JoltManager;

	struct BodyDesc
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
	};

	class Physics
	{
	public:
		Physics();
		~Physics() = default;

		ShapeHandle CreateBoxShape(const Vector3& size, const Vector3& center = { 0.0f, 0.0f, 0.0f });

		ShapeHandle CreateSphereShape(Float radius);

		ShapeHandle CreateCapsuleShape(Float height, Float radius);

		ShapeHandle CreateCylinderShape(Float height, Float radius);

		ShapeHandle CreateRectShape(const Vector2& size, const Vector2& center = { 0.0f, 0.0f });

		ShapeHandle CreateCircleShape(Float radius, const Vector2& center = { 0.0f, 0.0f });

		ShapeHandle CreateMeshShape();

		ShapeHandle CreateConvexShape();

		void ReleaseShape(ShapeHandle handle);

		JPH::BodyID CreateBody(const BodyDesc& desc);

		void DestroyBody(JPH::BodyID bodyID);

		void GetBodyTransform(JPH::BodyID bodyID, Vector3& outPosition, Quaternion& outRotation)const;

	private:
		JoltManager& joltPhysics_;
	};
}