#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/Utility/DynamicArray.h>

namespace SeedCore
{
	struct ZephyrModuleDescriptor
	{
		String name_;
		String includePath_;
		String configStructName_;
		Bool hasSpawn_;
		Bool hasUpdate_;
	};

	class ZephyrModuleRegistry
	{
	public:
		[[nodiscard]] static const DynamicArray<ZephyrModuleDescriptor>& GetModules();

		[[nodiscard]] static const ZephyrModuleDescriptor* Find(const String& name);
	};
}
