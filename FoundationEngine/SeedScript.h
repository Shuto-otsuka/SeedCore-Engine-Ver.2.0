#pragma once
#include <FoundationEngine/World/ECS/Component/ComponentBehaviour.h>
#include <FoundationEngine/World/ECS/Component/ComponentRegistry.h>

namespace SeedCore
{
	class SeedScript :public ComponentBehaviour
	{
	public:
		virtual ~SeedScript() = default;

	private:
		using ComponentBehaviour::DispatchDestroy;
		using ComponentBehaviour::DispatchInspectorGUI;
		using ComponentBehaviour::DispatchCollisionEnter;
		using ComponentBehaviour::DispatchCollisionStay;
		using ComponentBehaviour::DispatchCollisionExit;
		using ComponentBehaviour::DispatchTriggerEnter;
		using ComponentBehaviour::DispatchTriggerStay;
		using ComponentBehaviour::DispatchTriggerExit;
	};
}
