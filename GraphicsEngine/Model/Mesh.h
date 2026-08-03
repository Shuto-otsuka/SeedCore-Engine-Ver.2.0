#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/SeedScript.h>

namespace SeedCore
{
	struct Mesh
	{
		SC_PAYLOAD_FIELD_EX("メッシュID", Model)
		Uint32 meshID_ = 0;
	};
	REGISTER_COMPONENT(Mesh, "Geometry", ComponentStorage::Archetype);
}