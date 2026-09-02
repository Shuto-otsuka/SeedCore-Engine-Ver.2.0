#include <GraphicsEngine/Constraint/IKConstraint.h>
#include <GraphicsEngine/Model/Skeleton/Skeleton.h>
#include <FoundationEngine/ECS/Actor.h>

namespace SeedCore
{
	void IKConstraint::OnInspectorGUI()
	{
		Skeleton* skeleton = GetActor().GetComponent<Skeleton>();
		if (!skeleton || skeleton->BoneNames().empty())
		{
			ImGui::TextDisabled("スケルタル未取得（このアクターに Skeleton が必要）");
			return;
		}

		const Char* preview = effectorBoneName_.c_str();
		if (ImGui::BeginCombo("エフェクタ", preview ? preview : ""))
		{
			for (const String& name : skeleton->BoneNames())
			{
				const Char* label = name.c_str();
				if (!label || *label == '\0')
				{
					continue;
				}

				Bool selected = name == effectorBoneName_;
				if (ImGui::Selectable(label, selected))
				{
					effectorBoneName_ = name;
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
