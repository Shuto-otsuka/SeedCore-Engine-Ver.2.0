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

	JPH::ObjectLayer ToObjectLayer(Rigidbody::BodyType bodyType, Size userLayer)
	{
		switch (bodyType)
		{
		case Rigidbody::BodyType::Dynamic:
			return Layers::Pack(Layers::DYNAMIC, userLayer);
		case Rigidbody::BodyType::Kinematic:
			return Layers::Pack(Layers::KINEMATIC, userLayer);
		case Rigidbody::BodyType::Static:
			return Layers::Pack(Layers::STATIC, userLayer);
		default:
			return Layers::Pack(Layers::DYNAMIC, userLayer);
		}
	}

	JPH::ObjectLayer ToObjectLayer(Rigidbody::BodyType bodyType)
	{
		return ToObjectLayer(bodyType, LayerRegistry::DefaultLayer);
	}
}
