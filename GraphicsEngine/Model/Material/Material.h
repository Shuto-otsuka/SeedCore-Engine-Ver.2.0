#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/SeedScript.h>

namespace SeedCore
{
	class SEEDCORE_API Material :public SeedScript
	{
	public:
		SC_PAYLOAD_FIELD_EX("マテリアル", Material)
		DynamicArray<Uint32> materialIDs_;

	public:
		void OnInspectorGUI();

	};
	REGISTER_COMPONENT(Material, "Geometry");
}
