#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/SeedScript.h>

namespace SeedCore
{
	struct SkeletonSocket
	{
		std::string name_;
		std::string parentBoneName_;
		Vector3 positionOffset_ = { 0.0f, 0.0f, 0.0f };
		Vector3 rotationOffset_ = { 0.0f, 0.0f, 0.0f };
		Vector3 scale_ = { 1.0f, 1.0f, 1.0f };

		template<class Archive>
		void Serialize(Archive& archive)
		{
			archive.Field("name", name_);
			archive.Field("parent_bone_name", parentBoneName_);
			archive.Field("position_offset", positionOffset_);
			archive.Field("rotation_offset", rotationOffset_);
			archive.Field("scale", scale_);
		}
	};

	class SEEDCORE_API SkeletonRig :public NonCopyable
	{
	private:
		friend class SkeletonLoader;

		Uint32 sourceModelID_ = 0;

		std::string rootBoneName_;

		DynamicArray<SkeletonSocket> sockets_;

	public:
		SkeletonRig() = default;
		~SkeletonRig() = default;

		SkeletonRig(SkeletonRig&&)noexcept = default;
		SkeletonRig& operator=(SkeletonRig&&)noexcept = default;

		[[nodiscard]] Uint32 SourceModelID()const;

		[[nodiscard]] const std::string& RootBoneName()const;

		[[nodiscard]] const DynamicArray<SkeletonSocket>& Sockets()const;

		[[nodiscard]] const SkeletonSocket* FindSocket(const std::string& name)const;

		template<class Archive>
		void Serialize(Archive& archive)
		{
			archive.Field("source_model_id", sourceModelID_);
			archive.Field("root_bone_name", rootBoneName_);
			archive.Field("sockets", sockets_);
		}
	};

	class SEEDCORE_API Skeleton :public SeedScript
	{
	public:
		SC_PAYLOAD_FIELD_EX("スケルトン", Skeleton)
		Uint32 skeletonID_ = 0;

	public:
		void OnInspectorGUI();

	public:
		[[nodiscard]] const DynamicArray<String>& BoneNames()const;

		[[nodiscard]] const DynamicArray<Matrix>& GlobalTransforms()const;

		[[nodiscard]] Bool Valid()const;

		[[nodiscard]] Bool Animated()const;

		[[nodiscard]] Bool HasBone(const String& boneName)const;

		[[nodiscard]] Matrix BoneLocalMatrix(const String& boneName)const;

		[[nodiscard]] Matrix BoneWorldMatrix(const String& boneName)const;

	private:
		friend class AnimationSystem;

		Int BoneIndex(const String& boneName)const;

		DynamicArray<Matrix> globalTransforms_;

		DynamicArray<String> boneNames_;

		Uint32 meshID_ = 0;

		Bool valid_ = false;

		Bool animated_ = false;
	};
	REGISTER_COMPONENT(Skeleton, "Animation", ComponentStorage::SparseSet);
}
