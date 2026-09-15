#include <GraphicsEngine/Effect/Zephyr/ZephyrModuleAssembler.h>
#include <GraphicsEngine/Effect/Zephyr/ZephyrModuleRegistry.h>
#include <GraphicsEngine/Shader/ShaderHotReload.h>
#include <FoundationEngine/Log/Error.h>

namespace SeedCore
{
	ZephyrGeneratedShader ZephyrModuleAssembler::GetOrCreate(const DynamicArray<String>& moduleNames, ShaderHotReload& shaderHotReload)
	{
		Uint64 contentHash = 14695981039346656037ull;
		for (const String& name : moduleNames)
		{
			std::string nameString = name.str();
			for (Char character : nameString)
			{
				contentHash ^= static_cast<Uint64>(static_cast<Uint8>(character));
				contentHash *= 1099511628211ull;
			}
			contentHash ^= static_cast<Uint64>(';');
			contentHash *= 1099511628211ull;
		}

		if (auto found = generatedCache_.find(contentHash); found != generatedCache_.end())
		{
			return found->second;
		}

		DynamicArray<const ZephyrModuleDescriptor*> resolvedModules;
		DynamicArray<String> fieldNames;
		for (const String& name : moduleNames)
		{
			const ZephyrModuleDescriptor* descriptor = ZephyrModuleRegistry::Find(name);
			if (!descriptor)
			{
				SC_LOG_ERROR("Zephyr: 未登録のモジュール名です: {}", name.str());
				continue;
			}

			Size index = resolvedModules.size();
			std::string configStructName = descriptor->name_.str();
			configStructName[0] = static_cast<Char>(std::tolower(static_cast<Uint8>(configStructName[0])));

			std::ostringstream fieldName;
			fieldName << configStructName << "_" << index << "_";

			resolvedModules.push_back(descriptor);
			fieldNames.push_back(String(fieldName.str()));
		}

		std::ostringstream source;
		source << "#include \"../ParticlePool.hlsli\"\n";
		for (const ZephyrModuleDescriptor* descriptor : resolvedModules)
		{
			source << "#include \"../" << descriptor->includePath_.str() << "\"\n";
		}
		source << "\n";

		source << "struct EffectModules\n{\n";
		for (Size moduleIndex = 0; moduleIndex < resolvedModules.size(); ++moduleIndex)
		{
			source << "\t" << resolvedModules[moduleIndex]->configStructName_.str() << " " << fieldNames[moduleIndex].str() << ";\n";
		}
		source << "};\n\n";

		source << "ConstantBuffer<EffectModules> GetEffectModules()\n{\n";
		source << "\tParticleDispatchBuffer dispatchBuffer = GetParticleDispatchBuffer();\n";
		source << "\treturn ResourceDescriptorHeap[dispatchBuffer.module_index_];\n";
		source << "}\n\n";

		source << "void ApplyGeneratedModulesSpawn(inout ParticleSeed seed, EffectModules modules, ParticleMeta meta)\n{\n";
		for (Size moduleIndex = 0; moduleIndex < resolvedModules.size(); ++moduleIndex)
		{
			if (!resolvedModules[moduleIndex]->hasSpawn_)
			{
				continue;
			}
			source << "\tApply" << resolvedModules[moduleIndex]->name_.str() << "Spawn(seed, modules." << fieldNames[moduleIndex].str() << ", meta);\n";
		}
		source << "}\n\n";

		source << "void ApplyGeneratedModulesUpdate(inout ParticleSeed seed, EffectModules modules, ParticleMeta meta)\n{\n";
		for (Size moduleIndex = 0; moduleIndex < resolvedModules.size(); ++moduleIndex)
		{
			if (!resolvedModules[moduleIndex]->hasUpdate_)
			{
				continue;
			}
			source << "\tApply" << resolvedModules[moduleIndex]->name_.str() << "Update(seed, modules." << fieldNames[moduleIndex].str() << ", meta);\n";
		}
		source << "}\n\n";

		source << "[numthreads(64, 1, 1)]\n";
		source << "void SpawnMain(uint3 dispatch_thread_id : SV_DispatchThreadID)\n{\n";
		source << "\tRWStructuredBuffer<ParticleCountersConstantBuffer> counters = GetParticleCountersConstantBuffer();\n\n";
		source << "\tuint dead_slot_index;\n";
		source << "\tInterlockedAdd(counters[0].dead_count_, -1, dead_slot_index);\n\n";
		source << "\tuint particle_index = GetDeadList()[dead_slot_index - 1];\n\n";
		source << "\tParticleSeed seed = (ParticleSeed)0;\n\n";
		source << "\tParticleMeta meta = GetParticleMeta();\n";
		source << "\tEffectModules modules = GetEffectModules();\n\n";
		source << "\tApplyGeneratedModulesSpawn(seed, modules, meta);\n\n";
		source << "\tGetParticleBuffer()[particle_index] = seed;\n\n";
		source << "\tuint alive_write_index;\n";
		source << "\tInterlockedAdd(counters[0].alive_write_count_, 1, alive_write_index);\n";
		source << "\tGetAliveListWrite()[alive_write_index] = particle_index;\n";
		source << "}\n\n";

		source << "[numthreads(64, 1, 1)]\n";
		source << "void UpdateMain(uint3 dispatch_thread_id : SV_DispatchThreadID)\n{\n";
		source << "\tRWStructuredBuffer<ParticleCountersConstantBuffer> counters = GetParticleCountersConstantBuffer();\n";
		source << "\tif (dispatch_thread_id.x >= counters[0].alive_count_)\n\t{\n\t\treturn;\n\t}\n\n";
		source << "\tuint particle_index = GetAliveListRead()[dispatch_thread_id.x];\n\n";
		source << "\tRWStructuredBuffer<ParticleSeed> particle_buffer = GetParticleBuffer();\n";
		source << "\tParticleSeed seed = particle_buffer[particle_index];\n\n";
		source << "\tParticleMeta meta = GetParticleMeta();\n";
		source << "\tEffectModules modules = GetEffectModules();\n\n";
		source << "\tseed.age_ += meta.emitter_delta_;\n\n";
		source << "\tApplyGeneratedModulesUpdate(seed, modules, meta);\n\n";
		source << "\tif (seed.age_ >= seed.lifetime_)\n\t{\n";
		source << "\t\tuint dead_write_index;\n";
		source << "\t\tInterlockedAdd(counters[0].dead_count_, 1, dead_write_index);\n";
		source << "\t\tGetDeadList()[dead_write_index] = particle_index;\n";
		source << "\t\treturn;\n\t}\n\n";
		source << "\tparticle_buffer[particle_index] = seed;\n\n";
		source << "\tuint alive_write_index;\n";
		source << "\tInterlockedAdd(counters[0].alive_write_count_, 1, alive_write_index);\n";
		source << "\tGetAliveListWrite()[alive_write_index] = particle_index;\n";
		source << "}\n";

		std::ostringstream fileNameStream;
		fileNameStream << "../GraphicsEngine/Effect/Zephyr/Generated/Zephyr_" << std::hex << contentHash << ".hlsl";
		String filePath = String(fileNameStream.str());

		std::filesystem::path outputPath(filePath.str());
		std::error_code directoryError;
		std::filesystem::create_directories(outputPath.parent_path(), directoryError);

		std::ofstream outputFile(outputPath, std::ios::binary);
		if (outputFile)
		{
			std::string sourceText = source.str();
			outputFile.write(sourceText.data(), static_cast<std::streamsize>(sourceText.size()));
		}
		outputFile.close();

		ZephyrGeneratedShader generatedShader{};
		generatedShader.generatedFilePath_ = filePath;
		generatedShader.spawnShader_ = shaderHotReload.GetOrCreateComputeShader(filePath, String("SpawnMain"));
		generatedShader.updateShader_ = shaderHotReload.GetOrCreateComputeShader(filePath, String("UpdateMain"));

		generatedCache_.insert({ contentHash, generatedShader });
		return generatedShader;
	}
}
