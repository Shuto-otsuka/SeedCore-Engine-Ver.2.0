#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	class HumanCharacterModel;
	class HumanCharacterEvaluator;

	class SEEDCORE_API HumanCharacterConverter
	{
	public:
		HumanCharacterConverter() = delete;

		static void Convert(const HumanCharacterModel& model, const HumanCharacterEvaluator& evaluator, tinygltf::Model& outModel);

		static Bool Bake(const HumanCharacterModel& model, const HumanCharacterEvaluator& evaluator, Bool binary, String filePath);
	};
}
