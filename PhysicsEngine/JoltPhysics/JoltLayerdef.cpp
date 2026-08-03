#include <PhysicsEngine/JoltPhysics/JoltLayerdef.h>

namespace SeedCore
{
	JPH::EMotionType ToMotionType(Rigidbody::BodyType bodyType)
	{
		switch (bodyType)
		{
		case Rigidbody::BodyType::Dynamic:
			return JPH::EMotionType::Dynamic;
		case Rigidbody::BodyType::Kinematic:
			return JPH::EMotionType::Kinematic;
		case Rigidbody::BodyType::Static:
			return JPH::EMotionType::Static;
		default:
			return JPH::EMotionType::Dynamic;
		}
	}

	JPH::ObjectLayer ToObjectLayer(Rigidbody::BodyType bodyType)
	{
		switch (bodyType)
		{
		case Rigidbody::BodyType::Dynamic:
			return Layers::DYNAMIC;
		case Rigidbody::BodyType::Kinematic:
			return Layers::KINEMATIC;
		case Rigidbody::BodyType::Static:
			return Layers::STATIC;
		default:
			return Layers::DYNAMIC;
		}
	}
}
