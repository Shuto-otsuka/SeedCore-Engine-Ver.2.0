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
		Bool hasExitTime_ = false;

		SC_SERIALIZE_FIELD()
		Float exitTime_ = 1.0f;

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

	struct IKTarget
	{
		Vector3 targetPosition_ = Vector3::Zero;
		Quaternion targetRotation_ = Quaternion::Identity;
		Bool hasRotation_ = false;
		Vector3 poleVector_ = Vector3::Zero;
		Bool hasPoleVector_ = false;
		Float weight_ = 0.0f;
		Bool refreshedThisFrame_ = false;
	};

	struct JointConstraint
	{
		Vector3 axis_ = Vector3::Forward;
		Vector3 swingAxis_ = Vector3::Right;
		Float swingAngle1_ = 45.0f;
		Float swingAngle2_ = 45.0f;
	};

	class ModelRenderer;
	class AnimationSystem;
	class AnimatorControllerPanel;
	class TimelinePanel;

	namespace ScReflection
	{ 
		struct Register_Animator;
	}

	namespace ScPayload 
	{ 
		struct Register_Animator; 
	}

	class SEEDCORE_API Animator :public SeedScript
	{
		friend class ModelRenderer;
		friend class AnimationSystem;
		friend class AnimatorControllerPanel;
		friend class TimelinePanel;
		friend struct ScReflection::Register_Animator;
		friend struct ScPayload::Register_Animator;

	public:
		SC_PAYLOAD_FIELD_EX("アニメーションID", Animation)
		DynamicArray<Uint32> animationIDs_;

		SC_REFLECTION_FIELD_EX("アニメーション速度")
		Float animationSpeed_ = 1.0f;

	public:
		void OnTick(Float elapsedTime);

		void OnInspectorGUI();

	public:
		void SetTrigger(const std::string& name);

		void SetBool(const std::string& name, Bool value);

		void SetFloat(const std::string& name, Float value);

		void SetInt(const std::string& name, Int value);

		Bool GetBool(const std::string& name)const;

		Float GetFloat(const std::string& name)const;

		Int GetInt(const std::string& name)const;

		Int CurrentStateIndex()const;

		Float CurrentTime()const;

		Int PreviousStateIndex()const;

		Float PreviousTime()const;

		Bool Blending()const;

		Float Alpha()const;

	public:
		void SetIKTarget(const std::string& effectorBoneName, const Vector3& targetPosition, Float weight = 1.0f);

		void SetIKTarget(const std::string& effectorBoneName, const Vector3& targetPosition, const Quaternion& targetRotation, Float weight = 1.0f);

		void SetIKPole(const std::string& effectorBoneName, const Vector3& poleVector);

		void SetJointConstraint(const std::string& boneName, const Vector3& axis, const Vector3& swingAxis, Float swingAngle1, Float swingAngle2);

		[[nodiscard]] Bool HasIK()const;

		[[nodiscard]] const std::unordered_map<std::string, IKTarget>& GetIKTarget()const;

		[[nodiscard]] const std::unordered_map<std::string, JointConstraint>& GetJointConstraint()const;

	private:
		AnimationParameter* FindParameter(const String& name);

		const AnimationParameter* FindParameter(const String& name)const;

		void EvaluateTransitions();

		Bool EvaluateCondition(const AnimationCondition& condition)const;

		Bool HasRootMotionBaseline(Int stateIndex)const;

		Float RootMotionBaselineSampleTime()const;

		Vector3 RootMotionBaselineTranslation()const;

		void UpdateRootMotionBaseline(Int stateIndex, Float sampleTime, const Vector3& translation);

		void UpdateCurrentClipDuration(Float duration);

	private:
		static constexpr Int ExitState = -2;

		static constexpr Int AnyState = -3;

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

		SC_SERIALIZE_FIELD()
		Float anyNodePositionX_ = -220.0f;

		SC_SERIALIZE_FIELD()
		Float anyNodePositionY_ = 200.0f;

	private:
		Float currentTime_ = 0.0f;

		Int currentStateIndex_ = -1;

		Int previousStateIndex_ = -1;

		Float previousStateTime_ = 0.0f;

		Float blendElapsed_ = 0.0f;

		Float blendDuration_ = 0.0f;

		Float currentClipDuration_ = 0.0f;

		Bool rootMotionBaselineValid_ = false;

		Int rootMotionBaselineStateIndex_ = -1;

		Float rootMotionBaselineSampleTime_ = 0.0f;

		Vector3 rootMotionBaselineTranslation_ = Vector3::Zero;

		std::unordered_map<std::string, IKTarget> ikTargets_;

		std::unordered_map<std::string, JointConstraint> jointConstraints_;
	};
	REGISTER_COMPONENT(Animator, "Animation");
}