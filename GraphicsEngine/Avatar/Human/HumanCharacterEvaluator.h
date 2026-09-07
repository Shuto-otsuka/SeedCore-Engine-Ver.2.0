#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	class HumanCharacterModel;

	class SEEDCORE_API HumanCharacterEvaluator :public NonCopyable
	{
	public:
		HumanCharacterEvaluator() = default;
		~HumanCharacterEvaluator() = default;

		void SetModel(const HumanCharacterModel& model);

		void SetAxisWeight(Uint32 axisIndex, Float weight);

		void ResetAxisWeights();

		[[nodiscard]] Float AxisWeight(Uint32 axisIndex)const;

		void Evaluate();

		[[nodiscard]] std::span<const Vector3> Positions()const;

		[[nodiscard]] std::span<const Vector3> Normals()const;

		[[nodiscard]] std::span<const Vector3> JointHeads()const;

	private:
		const HumanCharacterModel* model_ = nullptr;

		DynamicArray<Float> axisWeights_;
		DynamicArray<Vector3> positions_;
		DynamicArray<Vector3> normals_;
		DynamicArray<Vector3> jointHeads_;
	};
}
