#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/SeedScript.h>

namespace SeedCore
{
	enum class AnimationParameterType
	{
		Bool,
		Float,
		Int,
		Trigger,
	};

	struct AnimationParameter
	{
		SC_SERIALIZE_FIELD()
		String name_;

		SC_SERIALIZE_FIELD()
		AnimationParameterType type_ = AnimationParameterType::Bool;

		SC_SERIALIZE_FIELD()
		Float value_ = 0.0f;
	};

	enum class AnimationConditionComparison
	{
		Equal,
		NotEqual,
		Greater,
		Less,
		GreaterOrEqual,
		LessOrEqual,
	};

	struct AnimationCondition
	{
		SC_SERIALIZE_FIELD()
		String parameterName_;

		SC_SERIALIZE_FIELD()
		AnimationConditionComparison comparison_ = AnimationConditionComparison::Equal;

		SC_SERIALIZE_FIELD()
		Float value_ = 0.0f;

		SC_SERIALIZE_FIELD()
		Bool isOr_ = false;
	};

	struct AnimationState
	{
		SC_SERIALIZE_FIELD()
		String name_;

		SC_SERIALIZE_FIELD()
		Int animationID_ = 0;

		SC_SERIALIZE_FIELD()
		Float nodePositionX_ = 0.0f;

		SC_SERIALIZE_FIELD()
		Float nodePositionY_ = 0.0f;

		SC_SERIALIZE_FIELD()
		Bool useRootMotion_ = false;

		SC_SERIALIZE_FIELD()
		Bool useIK_ = false;
	};

	struct AnimationTransition
	{
		SC_SERIALIZE_FIELD()
		Int fromState_ = -1;

		SC_SERIALIZE_FIELD()
		Int toState_ = -1;

		SC_SERIALIZE_FIELD()
		Float duration_ = 0.25f;

		SC_SERIALIZE_FIELD()
		Float fromOffsetX_ = 0.0f;

		SC_SERIALIZE_FIELD()
		Float fromOffsetY_ = 0.0f;

		SC_SERIALIZE_FIELD()
		Float toOffsetX_ = 0.0f;

		SC_SERIALIZE_FIELD()
		Float toOffsetY_ = 0.0f;

		SC_SERIALIZE_FIELD()
		DynamicArray<AnimationCondition> conditions_;
	};

	class SEEDCORE_API Animator :public SeedScript
	{
	public:
		static constexpr Int ExitState = -2;

		SC_PAYLOAD_FIELD_EX("アニメーションID", Animation)
		DynamicArray<Uint32> animationIDs_;

		SC_REFLECTION_FIELD_EX("アニメーション速度")
		Float animationSpeed_ = 1.0f;

		SC_SERIALIZE_FIELD()
		DynamicArray<AnimationParameter> parameters_;

		SC_SERIALIZE_FIELD()
		DynamicArray<AnimationState> states_;

		SC_SERIALIZE_FIELD()
		DynamicArray<AnimationTransition> transitions_;

		SC_SERIALIZE_FIELD()
		Int entryStateIndex_ = -1;

		SC_SERIALIZE_FIELD()
		Float entryNodePositionX_ = -220.0f;

		SC_SERIALIZE_FIELD()
		Float entryNodePositionY_ = 40.0f;

		SC_SERIALIZE_FIELD()
		Float exitNodePositionX_ = 320.0f;

		SC_SERIALIZE_FIELD()
		Float exitNodePositionY_ = 40.0f;

	public:
		void OnTick(Float elapsedTime);

		void OnInspectorGUI();

		void SetTrigger(const std::string& name);

		void SetBool(const std::string& name, Bool value);

		void SetFloat(const std::string& name, Float value);

		void SetInt(const std::string& name, Int value);

		Int CurrentStateIndex()const;

		Float CurrentTime()const;

		Bool HasRootMotionBaseline(Int stateIndex)const;

		Float RootMotionBaselineSampleTime()const;

		Vector3 RootMotionBaselineTranslation()const;

		void UpdateRootMotionBaseline(Int stateIndex, Float sampleTime, const Vector3& translation);

	private:
		AnimationParameter* FindParameter(const String& name);

		const AnimationParameter* FindParameter(const String& name)const;

		void EvaluateTransitions();

		Bool EvaluateCondition(const AnimationCondition& condition)const;

	private:
		Float currentTime_ = 0.0f;

		Int currentStateIndex_ = -1;

		Bool rootMotionBaselineValid_ = false;

		Int rootMotionBaselineStateIndex_ = -1;

		Float rootMotionBaselineSampleTime_ = 0.0f;

		Vector3 rootMotionBaselineTranslation_ = Vector3::Zero;
	};
	REGISTER_COMPONENT(Animator, "Animation");
}