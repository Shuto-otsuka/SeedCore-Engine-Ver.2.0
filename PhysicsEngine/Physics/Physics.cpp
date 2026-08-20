#include <PhysicsEngine/Physics/Physics.h>
#include <PhysicsEngine/JoltPhysics/JoltManager.h>
#include <PhysicsEngine/JoltPhysics/JoltLayerdef.h>
#include <FoundationEngine/Resource/Gateway.h>
#include <FoundationEngine/Log/Warning.h>

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

	ShapeHandle Physics::CreateMeshShape(Uint32 assetID, const DynamicArray<Vector3>& positions, const DynamicArray<Uint32>& indices)
	{
		return joltPhysics_.GetShapePool().CreateMeshShape(assetID, positions, indices);
	}

	ShapeHandle Physics::CreateConvexShape(Uint32 assetID, const DynamicArray<Vector3>& positions)
	{
		return joltPhysics_.GetShapePool().CreateConvexShape(assetID, positions);
	}

	void Physics::ReleaseShape(ShapeHandle handle)
	{
		joltPhysics_.GetShapePool().Release(handle);
	}

	JPH::Ref<JPH::CharacterVirtual> Physics::CreateCharacter(const CharacterDesc& desc)
	{
		JPH::RefConst<JPH::Shape> capsule = new JPH::CapsuleShape(desc.height_ * 0.5f, desc.radius_);
		JPH::RefConst<JPH::Shape> shape = new JPH::RotatedTranslatedShape(JPH::Vec3(0.0f, desc.height_ * 0.5f + desc.radius_, 0.0f), JPH::Quat::sIdentity(), capsule);

		JPH::CharacterVirtualSettings settings;
		settings.mShape = shape;
		settings.mMaxSlopeAngle = desc.maxSlopeAngle_;
		settings.mMass = desc.mass_;
		settings.mMaxStrength = desc.maxStrength_;

		return new JPH::CharacterVirtual(&settings, JPH::RVec3(desc.position_.x, desc.position_.y, desc.position_.z), JPH::Quat(desc.rotation_.x, desc.rotation_.y, desc.rotation_.z, desc.rotation_.w), desc.userData_, &joltPhysics_.GetPhysicsSystem());
	}

	Bool Physics::SetCharacterHeight(JPH::CharacterVirtual* character, Float height, Float radius)
	{
		if (!character)
		{
			return false;
		}

		JPH::RefConst<JPH::Shape> capsule = new JPH::CapsuleShape(height * 0.5f, radius);
		JPH::RefConst<JPH::Shape> shape = new JPH::RotatedTranslatedShape(JPH::Vec3(0.0f, height * 0.5f + radius, 0.0f), JPH::Quat::sIdentity(), capsule);

		JPH::PhysicsSystem& physicsSystem = joltPhysics_.GetPhysicsSystem();

		return character->SetShape(shape, 0.05f, physicsSystem.GetDefaultBroadPhaseLayerFilter(Layers::DYNAMIC), physicsSystem.GetDefaultLayerFilter(Layers::DYNAMIC), { }, { }, joltPhysics_.GetPhysicsAllocator());
	}

	void Physics::UpdateCharacter(JPH::CharacterVirtual* character, Float elapsedTime, Float maxSlopeAngle, Float stepHeight)
	{
		if (!character)
		{
			return;
		}

		character->SetMaxSlopeAngle(maxSlopeAngle);

		JPH::PhysicsSystem& physicsSystem = joltPhysics_.GetPhysicsSystem();

		JPH::CharacterVirtual::ExtendedUpdateSettings updateSettings;
		updateSettings.mWalkStairsStepUp = JPH::Vec3(0.0f, stepHeight, 0.0f);

		character->ExtendedUpdate(elapsedTime, physicsSystem.GetGravity(), updateSettings, physicsSystem.GetDefaultBroadPhaseLayerFilter(Layers::DYNAMIC), physicsSystem.GetDefaultLayerFilter(Layers::DYNAMIC), { }, { }, joltPhysics_.GetPhysicsAllocator());
	}

	void Physics::Refresh(JPH::CharacterVirtual* character)
	{
		if (!character)
		{
			return;
		}

		JPH::PhysicsSystem& physicsSystem = joltPhysics_.GetPhysicsSystem();

		character->RefreshContacts(physicsSystem.GetDefaultBroadPhaseLayerFilter(Layers::DYNAMIC), physicsSystem.GetDefaultLayerFilter(Layers::DYNAMIC), { }, { }, joltPhysics_.GetPhysicsAllocator());
	}

	void Physics::DestroyCharacter(JPH::Ref<JPH::CharacterVirtual>& character)
	{
		character = nullptr;
	}

	Vector3 Physics::GetGravity()const
	{
		JPH::Vec3 gravity = joltPhysics_.GetPhysicsSystem().GetGravity();
		return Vector3(gravity.GetX(), gravity.GetY(), gravity.GetZ());
	}

	JPH::BodyID Physics::CreateRigidbody(const RigidbodyDesc& desc)
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
		settings.mUserData = desc.userData_;
		settings.mIsSensor = desc.isSensor_;

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

	JPH::BodyID Physics::CreateSoftbody(const SoftbodyDesc& desc)
	{
		JPH::Ref<JPH::SoftBodySharedSettings> sharedSettings = new JPH::SoftBodySharedSettings();

		sharedSettings->mVertices.reserve(desc.positions_.size());
		for (const Vector3& position : desc.positions_)
		{
			sharedSettings->mVertices.push_back(JPH::SoftBodySharedSettings::Vertex(JPH::Float3(position.x, position.y, position.z)));
		}

		Uint32 vertexCount = static_cast<Uint32>(desc.positions_.size());

		sharedSettings->mFaces.reserve(desc.indices_.size() / 3);
		for (Size faceIndex = 0; faceIndex + 2 < desc.indices_.size(); faceIndex += 3)
		{
			Uint32 vertex0 = desc.indices_[faceIndex];
			Uint32 vertex1 = desc.indices_[faceIndex + 1];
			Uint32 vertex2 = desc.indices_[faceIndex + 2];

			if (vertex0 >= vertexCount || vertex1 >= vertexCount || vertex2 >= vertexCount)
			{
				SC_LOG_WARNING("Softbody: 頂点数(%u)を超える頂点インデックス(%u, %u, %u)を持つ面をスキップしました", vertexCount, vertex0, vertex1, vertex2);
				continue;
			}

			JPH::SoftBodySharedSettings::Face face(vertex0, vertex1, vertex2);
			if (face.IsDegenerate())
			{
				continue;
			}

			sharedSettings->AddFace(face);
		}

		if (sharedSettings->mFaces.empty())
		{
			return JPH::BodyID();
		}

		JPH::SoftBodySharedSettings::VertexAttributes vertexAttributes(desc.edgeCompliance_, desc.shearCompliance_, desc.bendCompliance_);
		sharedSettings->CreateConstraints(&vertexAttributes, 1);
		sharedSettings->Optimize();

		JPH::SoftBodyCreationSettings settings(sharedSettings, JPH::RVec3(desc.position_.x, desc.position_.y, desc.position_.z), JPH::Quat(desc.rotation_.x, desc.rotation_.y, desc.rotation_.z, desc.rotation_.w), desc.layer_);
		settings.mNumIterations = desc.numIterations_;
		settings.mLinearDamping = desc.linearDamping_;
		settings.mPressure = desc.pressure_;
		settings.mFriction = desc.friction_;
		settings.mRestitution = desc.restitution_;
		settings.mGravityFactor = desc.gravityFactor_;

		JPH::BodyInterface& bodyInterface = joltPhysics_.GetBodyInterface();
		return bodyInterface.CreateAndAddSoftBody(settings, JPH::EActivation::Activate);
	}

	void Physics::SetBodyShape(JPH::BodyID bodyID, ShapeHandle shape)
	{
		if (bodyID.IsInvalid())
		{
			return;
		}

		JPH::ShapeRefC newShape = joltPhysics_.GetShapePool().Get(shape);
		if (!newShape)
		{
			return;
		}

		JPH::BodyInterface& bodyInterface = joltPhysics_.GetBodyInterface();
		bodyInterface.SetShape(bodyID, newShape, true, JPH::EActivation::Activate);
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

	void Physics::GetVertexPosition(JPH::BodyID bodyID, DynamicArray<Vector3>& outPositions)const
	{
		if (bodyID.IsInvalid())
		{
			return;
		}

		const JPH::BodyLockInterface& lockInterface = joltPhysics_.GetPhysicsSystem().GetBodyLockInterface();

		JPH::BodyLockRead lock(lockInterface, bodyID);
		if (!lock.Succeeded())
		{
			return;
		}

		const JPH::Body& body = lock.GetBody();
		const JPH::SoftBodyMotionProperties* motionProperties = static_cast<const JPH::SoftBodyMotionProperties*>(body.GetMotionPropertiesUnchecked());
		if (!motionProperties)
		{
			return;
		}

		JPH::RMat44 transform = body.GetCenterOfMassTransform();
		const JPH::Array<JPH::SoftBodyMotionProperties::Vertex>& vertices = motionProperties->GetVertices();

		outPositions.clear();
		outPositions.reserve(vertices.size());
		for (const JPH::SoftBodyMotionProperties::Vertex& vertex : vertices)
		{
			JPH::RVec3 worldPosition = transform * vertex.mPosition;
			outPositions.push_back(Vector3(static_cast<Float>(worldPosition.GetX()), static_cast<Float>(worldPosition.GetY()), static_cast<Float>(worldPosition.GetZ())));
		}
	}

	EntityID Physics::GetBodyEntityID(JPH::BodyID bodyID)const
	{
		if (bodyID.IsInvalid())
		{
			return 0;
		}

		const JPH::BodyLockInterface& lockInterface = joltPhysics_.GetPhysicsSystem().GetBodyLockInterface();

		JPH::BodyLockRead lock(lockInterface, bodyID);
		if (!lock.Succeeded())
		{
			return 0;
		}

		return static_cast<EntityID>(lock.GetBody().GetUserData());
	}
}