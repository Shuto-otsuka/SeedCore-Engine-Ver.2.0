#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	class AnimalCharacterModel;
	class AnimalCharacterEvaluator;
	enum class ExportPreset;

	class SEEDCORE_API AnimalCharacterConverter
	{
	public:
		AnimalCharacterConverter() = delete;

		static void Convert(const AnimalCharacterModel& model, const AnimalCharacterEvaluator& evaluator, tinygltf::Model& outModel);

		static Bool Bake(const AnimalCharacterModel& model, const AnimalCharacterEvaluator& evaluator, ExportPreset preset, String filePath);
	};
}
