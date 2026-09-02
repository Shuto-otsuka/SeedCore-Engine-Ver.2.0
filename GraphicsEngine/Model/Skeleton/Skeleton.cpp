#include <GraphicsEngine/Model/Skeleton/Skeleton.h>
#include <GraphicsEngine/Model/Skeleton/SkeletonState.h>
#include <FoundationEngine/ECS/Actor.h>

namespace SeedCore
{
	Bool SkeletonControllerRequest::requested_ = false;

	Uint32 SkeletonRig::SourceModelID()const
	{
		return sourceModelID_;
	}

	const std::string& SkeletonRig::RootBoneName()const
	{
		return rootBoneName_;
	}

	const DynamicArray<SkeletonSocket>& SkeletonRig::Sockets()const
	{
		return sockets_;
	}

	const SkeletonSocket* SkeletonRig::FindSocket(const std::string& name)const
	{
		auto found = std::ranges::find(sockets_, name, &SkeletonSocket::name_);
		return found != sockets_.end() ? &*found : nullptr;
	}
}

namespace SeedCore
{
	void Skeleton::OnInspectorGUI()
	{
		ImGui::Dummy(ImVec2(0.0f, ImGui::GetTextLineHeightWithSpacing()));

		if (ImGui::Button("スケルトンコントローラーを開く"))
		{
			SkeletonControllerRequest::requested_ = true;
		}

		ImGui::Spacing();

		if (!valid_)
		{
			ImGui::TextDisabled("スケルトン未解決");
			return;
		}

		ImGui::Text("ボーン数: %d", static_cast<Int>(boneNames_.size()));

		if (ImGui::BeginChild("##bones", ImVec2(0.0f, ImGui::GetTextLineHeightWithSpacing() * 8.0f), ImGuiChildFlags_Borders))
		{
			for (Int index = 0; index < static_cast<Int>(boneNames_.size()); ++index)
			{
				const Char* label = boneNames_[static_cast<Size>(index)].c_str();
				if (!label || *label == '\0')
				{
					continue;
				}

				ImGui::TextUnformatted(label);
			}
		}
		ImGui::EndChild();
	}

	const DynamicArray<String>& Skeleton::BoneNames()const
	{
		return boneNames_;
	}

	const DynamicArray<Matrix>& Skeleton::GlobalTransforms()const
	{
		return globalTransforms_;
	}

	Bool Skeleton::Valid()const
	{
		return valid_;
	}

	Bool Skeleton::Animated()const
	{
		return animated_;
	}

	Int Skeleton::BoneIndex(const String& boneName)const
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

	Bool Skeleton::HasBone(const String& boneName)const
	{
		return BoneIndex(boneName) >= 0;
	}

	Matrix Skeleton::BoneLocalMatrix(const String& boneName)const
	{
		Int index = BoneIndex(boneName);
		if (index < 0 || static_cast<Size>(index) >= globalTransforms_.size())
		{
			return Matrix::Identity;
		}
		return globalTransforms_[static_cast<Size>(index)];
	}

	Matrix Skeleton::BoneWorldMatrix(const String& boneName)const
	{
		return BoneLocalMatrix(boneName) * GetActor().GetWorldMatrix();
	}
}
