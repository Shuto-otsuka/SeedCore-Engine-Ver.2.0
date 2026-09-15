#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	class AnimalCharacterModel;

	class SEEDCORE_API AnimalCharacterEvaluator :public NonCopyable
	{
	public:
		AnimalCharacterEvaluator() = default;
		~AnimalCharacterEvaluator() = default;

		void SetModel(const AnimalCharacterModel& model);

		void SetAxisWeight(Uint32 axisIndex, Float weight);

		void ResetAxisWeights();

		[[nodiscard]] Float AxisWeight(Uint32 axisIndex)const;

		void Evaluate();

		[[nodiscard]] std::span<const Vector3> Positions()const;

		[[nodiscard]] std::span<const Vector3> Normals()const;

		[[nodiscard]] std::span<const Vector3> JointHeads()const;

	private:
		/// [EN] Total absolute weight the additive feature axes may reach before being scaled down.
		///      (The count of leading body-type blend axes comes from the model's active group.)
		/// [JP] 加算的な部位調整軸の絶対重み合計がこの値を超えると縮小される上限。
		///      (先頭の体型ブレンド軸の本数はモデルのアクティブグループから取る。)
		static constexpr Float featureBudget_ = 2.5f;

		const AnimalCharacterModel* model_ = nullptr;

		DynamicArray<Float> axisWeights_;
		DynamicArray<Vector3> positions_;
		DynamicArray<Vector3> normals_;
		DynamicArray<Vector3> jointHeads_;
	};
}
