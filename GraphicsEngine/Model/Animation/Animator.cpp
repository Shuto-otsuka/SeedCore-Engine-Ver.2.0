#include <GraphicsEngine/Model/Animation/Animator.h>
#include <GraphicsEngine/Model/Animation/AnimatorControllerState.h>

namespace SeedCore
{
	void Animator::OnTick(Float elapsedTime)
	{
		currentTime_ += elapsedTime * animationSpeed_;

		if (currentStateIndex_ < 0 && !states_.empty())
		{
			currentStateIndex_ = (entryStateIndex_ >= 0 && static_cast<Size>(entryStateIndex_) < states_.size()) ? entryStateIndex_ : 0;
			currentTime_ = 0.0f;
		}

		EvaluateTransitions();
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

	Int Animator::CurrentStateIndex()const
	{
		return currentStateIndex_;
	}

	Float Animator::CurrentTime()const
	{
		return currentTime_;
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

	void Animator::EvaluateTransitions()
	{
		if (currentStateIndex_ < 0)
		{
			return;
		}

		for (AnimationTransition& transition : transitions_)
		{
			if (transition.toState_ == ExitState)
			{
				continue;
			}

			if (transition.fromState_ != currentStateIndex_ || transition.conditions_.empty())
			{
				continue;
			}

			Bool result = EvaluateCondition(transition.conditions_[0]);
			for (Size index = 1; index < transition.conditions_.size(); ++index)
			{
				Bool conditionResult = EvaluateCondition(transition.conditions_[index]);
				result = transition.conditions_[index].isOr_ ? (result || conditionResult) : (result && conditionResult);
			}

			if (!result)
			{
				continue;
			}

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
}