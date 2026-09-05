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

		for (Size entryIndex = 0; entryIndex < entries_.size(); entryIndex++)
		{
			Effector& entry = entries_[entryIndex];

			ImGui::PushID(static_cast<Int>(entryIndex));

			std::string label = "エフェクタボーン [" + std::to_string(entryIndex) + "]";
			const Char* preview = entry.effectorBoneName_.c_str();
			if (ImGui::BeginCombo(label.c_str(), preview ? preview : ""))
			{
				for (const String& name : skeleton->BoneNames())
				{
					const Char* nameLabel = name.c_str();
					if (!nameLabel || *nameLabel == '\0')
					{
						continue;
					}

					Bool selected = name == entry.effectorBoneName_;
					if (ImGui::Selectable(nameLabel, selected))
					{
						entry.effectorBoneName_ = name;
					}
					if (selected)
					{
						ImGui::SetItemDefaultFocus();
					}
				}
				ImGui::EndCombo();
			}

			ImGui::PopID();
		}
	}
}
