#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/Utility/DynamicArray.h>
#include <FoundationEngine/Utility/FlatMap.h>
#include <FoundationEngine/Utility/Handle.h>

namespace SeedCore
{
	class ComputeShader;
	class ShaderHotReload;

	struct ZephyrGeneratedShader
	{
		String generatedFilePath_;
		Handle<ComputeShader> spawnShader_;
		Handle<ComputeShader> updateShader_;
	};

	class ZephyrModuleAssembler :public NonTransferable
	{
	public:
		ZephyrModuleAssembler() = default;
		~ZephyrModuleAssembler() = default;

		[[nodiscard]] ZephyrGeneratedShader GetOrCreate(const DynamicArray<String>& moduleNames, ShaderHotReload& shaderHotReload);

	private:
		FlatMap<Uint64, ZephyrGeneratedShader> generatedCache_;
	};
}
