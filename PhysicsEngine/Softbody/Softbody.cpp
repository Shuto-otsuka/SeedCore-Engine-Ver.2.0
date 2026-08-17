#include <PhysicsEngine/Softbody/Softbody.h>
#include <PhysicsEngine/Physics/Physics.h>
#include <PhysicsEngine/JoltPhysics/JoltLayerdef.h>
#include <FoundationEngine/ECS/Actor.h>
#include <FoundationEngine/ECS/Component/Position.h>
#include <FoundationEngine/ECS/Component/Rotation.h>
#include <GraphicsEngine/Model/Crister.h>
#include <GraphicsEngine/Model/Mesh.h>

namespace SeedCore
{
	void Softbody::OnAwake()
	{
		const Mesh* mesh = GetActor().GetComponent<Mesh>();
		pending_ = mesh != nullptr && mesh->meshID_ != 0;
	}

	void Softbody::OnFixedTick(Float elapsedTime)
	{
		if (bodyID_.IsInvalid())
		{
			return;
		}

		GetActor().GetPhysics().GetSoftbodyVertexPositions(bodyID_, vertexPositions_);

		Matrix inverseWorld = GetActor().GetWorldMatrix().Invert();
		for (Vector3& position : vertexPositions_)
		{
			position = Vector3::Transform(position, inverseWorld);
		}
	}

	void Softbody::OnDestroy()
	{
		if (bodyID_.IsInvalid())
		{
			return;
		}

		GetActor().GetPhysics().DestroyBody(bodyID_);

		bodyID_ = JPH::BodyID();
		vertexPositions_.clear();

		const Mesh* mesh = GetActor().GetComponent<Mesh>();
		pending_ = mesh != nullptr && mesh->meshID_ != 0;
	}

	Bool Softbody::IsPending()const
	{
		return pending_;
	}

	void Softbody::Build(const Crister& crister)
	{
		Actor& actor = GetActor();

		DynamicArray<Vertex> vertices;
		SoftbodyDesc desc;
		if (!crister.SoftbodyCoarsestVertices(vertices, desc.indices_))
		{
			return;
		}

		desc.positions_.reserve(vertices.size());
		for (const Vertex& vertex : vertices)
		{
			desc.positions_.push_back(vertex.position_);
		}

		desc.layer_ = Layers::DYNAMIC;
		desc.edgeCompliance_ = (1.0f - Clamp(edgeStiffness_, 0.0f, 1.0f)) * 1.0e-4f;
		desc.shearCompliance_ = (1.0f - Clamp(areaStiffness_, 0.0f, 1.0f)) * 1.0e-4f;
		desc.bendCompliance_ = (1.0f - Clamp(bendStiffness_, 0.0f, 1.0f)) * 1.0e-4f;
		desc.numIterations_ = static_cast<Uint32>(iterationCount_);
		desc.linearDamping_ = damping_;
		desc.pressure_ = pressure_;
		desc.friction_ = friction_;
		desc.restitution_ = restitution_;
		desc.gravityFactor_ = useGravity_ ? gravityScale_ : 0.0f;

		const Position* position = actor.GetComponent<Position>();
		const Rotation* rotation = actor.GetComponent<Rotation>();
		desc.position_ = position ? Vector3(position->x, position->y, position->z) : Vector3(0.0f, 0.0f, 0.0f);
		desc.rotation_ = rotation ? Quaternion::CreateFromYawPitchRoll(ToRadians(rotation->y), ToRadians(rotation->x), ToRadians(rotation->z)) : Quaternion::Identity;

		JPH::Ref<JPH::SoftBodySharedSettings> sharedSettings = Physics::BuildSoftbodySettings(desc);
		bodyID_ = actor.GetPhysics().CreateSoftbody(desc, sharedSettings);
		if (bodyID_.IsInvalid())
		{
			return;
		}

		pending_ = false;
	}

	Bool Softbody::HasBody()const
	{
		return !bodyID_.IsInvalid();
	}

	const DynamicArray<Vector3>& Softbody::GetVertexPositions()const
	{
		return vertexPositions_;
	}
}
