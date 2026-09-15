#include <GraphicsEngine/Effect/Zephyr/ZephyrModuleRegistry.h>

namespace SeedCore
{
	const DynamicArray<ZephyrModuleDescriptor>& ZephyrModuleRegistry::GetModules()
	{
		static const DynamicArray<ZephyrModuleDescriptor> modules =
		{
			ZephyrModuleDescriptor{ String("Gravity"), String("Module/GravityModule.hlsli"), String("GravityModule"), false, true },
			ZephyrModuleDescriptor{ String("Drag"), String("Module/DragModule.hlsli"), String("DragModule"), false, true },
		};
		return modules;
	}

	const ZephyrModuleDescriptor* ZephyrModuleRegistry::Find(const String& name)
	{
		const DynamicArray<ZephyrModuleDescriptor>& modules = GetModules();
		auto found = std::ranges::find_if(modules, [&name](const ZephyrModuleDescriptor& module_) { return module_.name_ == name; });
		if (found == modules.end())
		{
			return nullptr;
		}
		return &(*found);
	}
}
