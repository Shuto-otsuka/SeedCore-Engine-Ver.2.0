#include <GraphicsEngine/Constraint/AttachmentConstraint.h>
#include <GraphicsEngine/Model/Animation/Animator.h>
#include <FoundationEngine/ECS/World.h>
#include <FoundationEngine/ECS/Actor.h>

namespace SeedCore
{
	void AttachmentConstraint::OnInspectorGUI()
	{
		if (target_ == 0)
		{
			ImGui::TextDisabled("ターゲット未設定");
			return;
		}

		Actor targetActor = GetWorld().FindActor(target_);
		Animator* animator = targetActor ? targetActor.GetComponent<Animator>() : nullptr;
		if (!animator || animator->BoneNames().empty())
		{
			ImGui::TextDisabled("スケルタル未取得（ターゲットに Animator が必要）");
			return;
		}

		const Char* preview = boneName_.c_str();
		if (ImGui::BeginCombo("ボーン", preview ? preview : ""))
		{
			for (const String& name : animator->BoneNames())
			{
				const Char* label = name.c_str();
				if (!label || *label == '\0')
				{
					continue;
				}

				Bool selected = name == boneName_;
				if (ImGui::Selectable(label, selected))
				{
					boneName_ = name;
				}
				if (selected)
				{
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}
	}
}
