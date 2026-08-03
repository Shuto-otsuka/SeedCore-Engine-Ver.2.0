#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/ECS/ReflectionRegistry.h>
#include <UserProject/Script/Test.h>

extern "C" int _force_reflection_Test = 0;

namespace SeedCore
{
	 namespace ScReflection
	 {
		// ---- UserProject/Script/Test.h ----
		struct Register_Test
		{
			Register_Test()
			{
				ReflectionRegistry::Register(String("Test"), [](void* ptr, DynamicArray<FieldInfo>& outInfo) {
					Test& obj = *static_cast<Test*>(ptr);
					outInfo.push_back({ String("テスト"), offsetof(Test, test_), AttributeType::Bool });
				});
			}
		};
		static Register_Test global_Test_register;

	}
}