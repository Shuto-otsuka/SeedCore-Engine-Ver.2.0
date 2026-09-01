#include <GraphicsEngine/Model/Animation/Animator.h>
#include <GraphicsEngine/Model/Animation/AnimatorControllerState.h>
#include <FoundationEngine/ECS/Actor.h>

namespace SeedCore
{
	void Animator::OnTick(Float elapsedTime)
	{
		currentTime_ += elapsedTime * animationSpeed_;

		if (previousStateIndex_ >= 0)
		{
			previousStateTime_ += elapsedTime * animationSpeed_;
			blendElapsed_ += elapsedTime;

			if (blendElapsed_ >= blendDuration_)
			{
				previousStateIndex_ = -1;
				previousStateTime_ = 0.0f;
				blendElapsed_ = 0.0f;
				blendDuration_ = 0.0f;
			}
		}

		if (currentStateIndex_ < 0 && !states_.empty())
		{
			currentStateIndex_ = (entryStateIndex_ >= 0 && static_cast<Size>(entryStateIndex_) < states_.size()) ? entryStateIndex_ : 0;
			currentTime_ = 0.0f;
		}

		EvaluateTransitions();

		constexpr Float ikTargetDecayPerSecond = 6.0f;
		for (auto it = ikTargets_.begin(); it != ikTargets_.end();)
		{
			if (it->second.refreshedThisFrame_)
			{
				it->second.refreshedThisFrame_ = false;
				++it;
			}
			else
			{
				it->second.weight_ -= ikTargetDecayPerSecond * elapsedTime;
				if (it->second.weight_ <= 0.001f)
				{
					it = ikTargets_.erase(it);
				}
				else
				{
					++it;
				}
			}
		}
	}

	void Animator::OnInspectorGUI()
	{
		ImGui::Dummy(ImVec2(0.0f, ImGui::GetTextLineHeightWithSpacing()));

		if (ImGui::Button("アニメーターコントローラーを開く"))
		{
			AnimatorControllerRequest::requested_ = true;
		}

		ImGui::Spacing();

		if (ImGui::Button("タイムラインを開く"))
		{
			TimelineRequest::requested_ = true;
		}
	}

	void Animator::SetTrigger(const std::string& name)
	{
		if (AnimationParameter* parameter = FindParameter(String(std::string_view(name))))
		{
			parameter->value_ = 1.0f;
		}
	}

	void Animator::SetBool(const std::string& name, Bool value)
	{
		if (AnimationParameter* parameter = FindParameter(String(std::string_view(name))))
		{
			parameter->value_ = value ? 1.0f : 0.0f;
		}
	}

	void Animator::SetFloat(const std::string& name, Float value)
	{
		if (AnimationParameter* parameter = FindParameter(String(std::string_view(name))))
		{
			parameter->value_ = value;
		}
	}

	void Animator::SetInt(const std::string& name, Int value)
	{
		if (AnimationParameter* parameter = FindParameter(String(std::string_view(name))))
		{
			parameter->value_ = static_cast<Float>(value);
		}
	}

	Bool Animator::GetBool(const std::string& name)const
	{
		const AnimationParameter* parameter = FindParameter(String(std::string_view(name)));
		return parameter && parameter->value_ != 0.0f;
	}

	Float Animator::GetFloat(const std::string& name)const
	{
		const AnimationParameter* parameter = FindParameter(String(std::string_view(name)));
		return parameter ? parameter->value_ : 0.0f;
	}

	Int Animator::GetInt(const std::string& name)const
	{
		const AnimationParameter* parameter = FindParameter(String(std::string_view(name)));
		return parameter ? static_cast<Int>(parameter->value_) : 0;
	}

	Int Animator::CurrentStateIndex()const
	{
		return currentStateIndex_;
	}

	Float Animator::CurrentTime()const
	{
		return currentTime_;
	}

	Int Animator::PreviousStateIndex()const
	{
		return previousStateIndex_;
	}

	Float Animator::PreviousTime()const
	{
		return previousStateTime_;
	}

	Bool Animator::Blending()const
	{
		return previousStateIndex_ >= 0;
	}

	Float Animator::Alpha()const
	{
		return blendDuration_ > 0.0f ? Clamp(blendElapsed_ / blendDuration_, 0.0f, 1.0f) : 1.0f;
	}

	const DynamicArray<String>& Animator::BoneNames()const
	{
		return boneNames_;
	}

	void Animator::SetIKTarget(const std::string& effectorBoneName, const Vector3& targetPosition, Float weight)
	{
		IKTarget& target = ikTargets_[effectorBoneName];
		target.targetPosition_ = targetPosition;
		target.hasRotation_ = false;
		target.weight_ = weight;
		target.refreshedThisFrame_ = true;
	}

	void Animator::SetIKTarget(const std::string& effectorBoneName, const Vector3& targetPosition, const Quaternion& targetRotation, Float weight)
	{
		IKTarget& target = ikTargets_[effectorBoneName];
		target.targetPosition_ = targetPosition;
		target.targetRotation_ = targetRotation;
		target.hasRotation_ = true;
		target.weight_ = weight;
		target.refreshedThisFrame_ = true;
	}

	void Animator::SetJointConstraint(const std::string& boneName, const Vector3& axis, const Vector3& swingAxis, Float swingAngle1, Float swingAngle2)
	{
		JointConstraint& constraint = jointConstraints_[boneName];
		constraint.axis_ = axis;
		constraint.swingAxis_ = swingAxis;
		constraint.swingAngle1_ = swingAngle1;
		constraint.swingAngle2_ = swingAngle2;
	}

	Bool Animator::HasIK()const
	{
		return currentStateIndex_ >= 0 && static_cast<Size>(currentStateIndex_) < states_.size() && states_[currentStateIndex_].useIK_;
	}

	const std::unordered_map<std::string, IKTarget>& Animator::GetIKTarget()const
	{
		return ikTargets_;
	}

	const std::unordered_map<std::string, JointConstraint>& Animator::GetJointConstraint()const
	{
		return jointConstraints_;
	}

	AnimationParameter* Animator::FindParameter(const String& name)
	{
		for (AnimationParameter& parameter : parameters_)
		{
			if (parameter.name_ == name)
			{
				return &parameter;
			}
		}
		return nullptr;
	}

	const AnimationParameter* Animator::FindParameter(const String& name)const
	{
		for (const AnimationParameter& parameter : parameters_)
		{
			if (parameter.name_ == name)
			{
				return &parameter;
			}
		}
		return nullptr;
	}

	void Animator::EvaluateTransitions()
	{
		if (currentStateIndex_ < 0 || previousStateIndex_ >= 0)
		{
			return;
		}

		for (AnimationTransition& transition : transitions_)
		{
			if (transition.toState_ == ExitState)
			{
				continue;
			}

			Bool isFromCurrentState = transition.fromState_ == currentStateIndex_;
			Bool isFromAnyState = transition.fromState_ == AnyState && transition.toState_ != currentStateIndex_;

			if ((!isFromCurrentState && !isFromAnyState) || (transition.conditions_.empty() && !transition.hasExitTime_))
			{
				continue;
			}

			if (transition.hasExitTime_)
			{
				Float normalizedTime = currentClipDuration_ > 0.0f ? currentTime_ / currentClipDuration_ : 0.0f;
				if (normalizedTime < transition.exitTime_)
				{
					continue;
				}
			}

			Bool result = true;
			if (!transition.conditions_.empty())
			{
				result = EvaluateCondition(transition.conditions_[0]);
				for (Size index = 1; index < transition.conditions_.size(); ++index)
				{
					Bool conditionResult = EvaluateCondition(transition.conditions_[index]);
					result = transition.conditions_[index].isOr_ ? (result || conditionResult) : (result && conditionResult);
				}
			}

			if (!result)
			{
				continue;
			}

			previousStateIndex_ = currentStateIndex_;
			previousStateTime_ = currentTime_;
			blendElapsed_ = 0.0f;
			blendDuration_ = transition.duration_;

			currentStateIndex_ = transition.toState_;
			currentTime_ = 0.0f;

			for (const AnimationCondition& condition : transition.conditions_)
			{
				AnimationParameter* parameter = FindParameter(condition.parameterName_);
				if (parameter && parameter->type_ == AnimationParameterType::Trigger)
				{
					parameter->value_ = 0.0f;
				}
			}

			return;
		}
	}

	Bool Animator::EvaluateCondition(const AnimationCondition& condition)const
	{
		const AnimationParameter* parameter = FindParameter(condition.parameterName_);
		if (!parameter)
		{
			return false;
		}

		if (parameter->type_ == AnimationParameterType::Trigger)
		{
			return parameter->value_ != 0.0f;
		}

		switch (condition.comparison_)
		{
		case AnimationConditionComparison::Equal:
			return parameter->value_ == condition.value_;
		case AnimationConditionComparison::NotEqual:
			return parameter->value_ != condition.value_;
		case AnimationConditionComparison::Greater:
			return parameter->value_ > condition.value_;
		case AnimationConditionComparison::Less:
			return parameter->value_ < condition.value_;
		case AnimationConditionComparison::GreaterOrEqual:
			return parameter->value_ >= condition.value_;
		case AnimationConditionComparison::LessOrEqual:
			return parameter->value_ <= condition.value_;
		default:
			return false;
		}
	}

	Bool Animator::HasRootMotionBaseline(Int stateIndex)const
	{
		return rootMotionBaselineValid_ && rootMotionBaselineStateIndex_ == stateIndex;
	}

	Float Animator::RootMotionBaselineSampleTime()const
	{
		return rootMotionBaselineSampleTime_;
	}

	Vector3 Animator::RootMotionBaselineTranslation()const
	{
		return rootMotionBaselineTranslation_;
	}

	void Animator::UpdateRootMotionBaseline(Int stateIndex, Float sampleTime, const Vector3& translation)
	{
		rootMotionBaselineValid_ = true;
		rootMotionBaselineStateIndex_ = stateIndex;
		rootMotionBaselineSampleTime_ = sampleTime;
		rootMotionBaselineTranslation_ = translation;
	}

	void Animator::UpdateCurrentClipDuration(Float duration)
	{
		currentClipDuration_ = duration;
	}

	Int Animator::BoneIndex(const String& boneName)const
	{
		for (Int index = 0; index < static_cast<Int>(boneNames_.size()); ++index)
		{
			if (boneNames_[static_cast<Size>(index)] == boneName)
			{
				return index;
			}
		}
		return -1;
	}

	Bool Animator::HasBone(const String& boneName)const
	{
		return BoneIndex(boneName) >= 0;
	}

	Matrix Animator::BoneLocalMatrix(const String& boneName)const
	{
		Int index = BoneIndex(boneName);
		if (index < 0 || static_cast<Size>(index) >= boneGlobals_.size())
		{
			return Matrix::Identity;
		}
		return boneGlobals_[static_cast<Size>(index)];
	}

	Matrix Animator::BoneWorldMatrix(const String& boneName)const
	{
		return BoneLocalMatrix(boneName) * GetActor().GetWorldMatrix();
	}

	void Animator::SetBoneNames(DynamicArray<String> boneNames)
	{
		boneNames_ = std::move(boneNames);
	}

	void Animator::SetBoneGlobals(DynamicArray<Matrix> boneGlobals)
	{
		boneGlobals_ = std::move(boneGlobals);
	}
}
