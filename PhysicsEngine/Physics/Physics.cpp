#include <PhysicsEngine/Physics/Physics.h>
#include <PhysicsEngine/JoltPhysics/JoltManager.h>
#include <FoundationEngine/Resource/Gateway.h>

namespace SeedCore
{
	Physics::Physics() :joltPhysics_(Gateway::GetJoltManager())
	{
		/// No Code
	}

	ShapeHandle Physics::CreateBoxShape(const Vector3& size, const Vector3& center)
	{
		return joltPhysics_.GetShapePool().CreateBoxShape(size, center);
	}

	ShapeHandle Physics::CreateSphereShape(Float radius)
	{
		return joltPhysics_.GetShapePool().CreateSphereShape(radius);
	}

	ShapeHandle Physics::CreateCapsuleShape(Float height, Float radius)
	{
		return joltPhysics_.GetShapePool().CreateCapsuleShape(height, radius);
	}

	ShapeHandle Physics::CreateCylinderShape(Float height, Float radius)
	{
		return joltPhysics_.GetShapePool().CreateCylinderShape(height, radius);
	}

	ShapeHandle Physics::CreateRectShape(const Vector2& size, const Vector2& center)
	{
		return joltPhysics_.GetShapePool().CreateRectShape(size, center);
	}

	ShapeHandle Physics::CreateCircleShape(Float radius, const Vector2& center)
	{
		return joltPhysics_.GetShapePool().CreateCircleShape(radius, center);
	}

	//ShapeHandle Physics::CreateMeshShape()
	//{
	//	return 
	//}

	//ShapeHandle Physics::CreateConvexShape()
	//{
	//	return 
	//}

	void Physics::ReleaseShape(ShapeHandle handle)
	{
		joltPhysics_.GetShapePool().Release(handle);
	}

	JPH::BodyID Physics::CreateBody(const BodyDesc& desc)
	{
		JPH::ShapeRefC shape = joltPhysics_.GetShapePool().Get(desc.shape_);
		if (!shape)
		{
			return JPH::BodyID();
		}

		JPH::BodyCreationSettings settings(shape, JPH::RVec3(desc.position_.x, desc.position_.y, desc.position_.z), JPH::Quat(desc.rotation_.x, desc.rotation_.y, desc.rotation_.z, desc.rotation_.w), desc.motionType_, desc.layer_);

		settings.mAllowedDOFs = desc.allowedDOFs_;
		settings.mLinearDamping = desc.linearDamping_;
		settings.mAngularDamping = desc.angularDamping_;
		settings.mFriction = desc.friction_;
		settings.mRestitution = desc.restitution_;
		settings.mGravityFactor = desc.gravityFactor_;

		if (desc.motionType_ != JPH::EMotionType::Static)
		{
			settings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
			settings.mMassPropertiesOverride.mMass = desc.mass_;
		}

		JPH::BodyInterface& bodyInterface = joltPhysics_.GetBodyInterface();
		JPH::Body* body = bodyInterface.CreateBody(settings);
		if (!body)
		{
			return JPH::BodyID();
		}

		bodyInterface.AddBody(body->GetID(), JPH::EActivation::Activate);
		return body->GetID();
	}

	void Physics::DestroyBody(JPH::BodyID bodyID)
	{
		if (bodyID.IsInvalid())
		{
			return;
		}

		JPH::BodyInterface& bodyInterface = joltPhysics_.GetBodyInterface();
		bodyInterface.RemoveBody(bodyID);
		bodyInterface.DestroyBody(bodyID);
	}

	void Physics::GetBodyTransform(JPH::BodyID bodyID, Vector3& outPosition, Quaternion& outRotation)const
	{
		if (bodyID.IsInvalid())
		{
			return;
		}

		JPH::BodyInterface& bodyInterface = joltPhysics_.GetBodyInterface();
		JPH::RVec3 position = bodyInterface.GetPosition(bodyID);
		JPH::Quat rotation = bodyInterface.GetRotation(bodyID);

		outPosition = Vector3(position.GetX(), position.GetY(), position.GetZ());
		outRotation = Quaternion(rotation.GetX(), rotation.GetY(), rotation.GetZ(), rotation.GetW());
	}
}