#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/ECS/PayloadRegistry.h>
#include <FoundationEngine/ECS/Component/Spawner.h>
#include <GraphicsEngine/Constraint/LookAtConstraint.h>
#include <GraphicsEngine/Constraint/ParentConstraint.h>
#include <GraphicsEngine/Constraint/PositionConstraint.h>
#include <GraphicsEngine/Constraint/RotationConstraint.h>
#include <GraphicsEngine/Font/Text.h>
#include <GraphicsEngine/Light/SkyLight.h>
#include <GraphicsEngine/Model/Animation/Animator.h>
#include <GraphicsEngine/Model/Material/Material.h>
#include <GraphicsEngine/Model/Mesh.h>
#include <GraphicsEngine/Movie/Movie.h>
#include <GraphicsEngine/Texture/Image.h>
#include <PhysicsEngine/Collider/MeshCollider.h>

extern "C" int _force_payload_Spawner = 0;
extern "C" int _force_payload_LookAtConstraint = 0;
extern "C" int _force_payload_ParentConstraint = 0;
extern "C" int _force_payload_PositionConstraint = 0;
extern "C" int _force_payload_RotationConstraint = 0;
extern "C" int _force_payload_Text = 0;
extern "C" int _force_payload_SkyLight = 0;
extern "C" int _force_payload_Mesh = 0;
extern "C" int _force_payload_Animator = 0;
extern "C" int _force_payload_Material = 0;
extern "C" int _force_payload_Movie = 0;
extern "C" int _force_payload_Image = 0;
extern "C" int _force_payload_MeshCollider = 0;

namespace SeedCore
{
	 namespace ScPayload
	 {
		// ---- FoundationEngine/ECS/Component/Spawner.h ----
		struct Register_Spawner
		{
			Register_Spawner()
			{
				PayloadRegistry::Register(String("Spawner"), [](void* ptr, DynamicArray<FieldInfo>& outInfo) {
					Spawner& obj = *static_cast<Spawner*>(ptr);
					outInfo.push_back({ String("プレハブID"), offsetof(Spawner, prefabID_), AttributeType::Int, PayloadAssetType::Prefab });
				});
			}
		};
		static Register_Spawner global_Spawner_register;

		// ---- GraphicsEngine/Constraint/LookAtConstraint.h ----
		struct Register_LookAtConstraint
		{
			Register_LookAtConstraint()
			{
				PayloadRegistry::Register(String("LookAtConstraint"), [](void* ptr, DynamicArray<FieldInfo>& outInfo) {
					LookAtConstraint& obj = *static_cast<LookAtConstraint*>(ptr);
					outInfo.push_back({ String("ターゲット"), offsetof(LookAtConstraint, target_), AttributeType::Int, PayloadAssetType::Actor });
				});
			}
		};
		static Register_LookAtConstraint global_LookAtConstraint_register;

		// ---- GraphicsEngine/Constraint/ParentConstraint.h ----
		struct Register_ParentConstraint
		{
			Register_ParentConstraint()
			{
				PayloadRegistry::Register(String("ParentConstraint"), [](void* ptr, DynamicArray<FieldInfo>& outInfo) {
					ParentConstraint& obj = *static_cast<ParentConstraint*>(ptr);
					outInfo.push_back({ String("ターゲット"), offsetof(ParentConstraint, target_), AttributeType::Int, PayloadAssetType::Actor });
				});
			}
		};
		static Register_ParentConstraint global_ParentConstraint_register;

		// ---- GraphicsEngine/Constraint/PositionConstraint.h ----
		struct Register_PositionConstraint
		{
			Register_PositionConstraint()
			{
				PayloadRegistry::Register(String("PositionConstraint"), [](void* ptr, DynamicArray<FieldInfo>& outInfo) {
					PositionConstraint& obj = *static_cast<PositionConstraint*>(ptr);
					outInfo.push_back({ String("ターゲット"), offsetof(PositionConstraint, target_), AttributeType::Int, PayloadAssetType::Actor });
				});
			}
		};
		static Register_PositionConstraint global_PositionConstraint_register;

		// ---- GraphicsEngine/Constraint/RotationConstraint.h ----
		struct Register_RotationConstraint
		{
			Register_RotationConstraint()
			{
				PayloadRegistry::Register(String("RotationConstraint"), [](void* ptr, DynamicArray<FieldInfo>& outInfo) {
					RotationConstraint& obj = *static_cast<RotationConstraint*>(ptr);
					outInfo.push_back({ String("ターゲット"), offsetof(RotationConstraint, target_), AttributeType::Int, PayloadAssetType::Actor });
				});
			}
		};
		static Register_RotationConstraint global_RotationConstraint_register;

		// ---- GraphicsEngine/Font/Text.h ----
		struct Register_Text
		{
			Register_Text()
			{
				PayloadRegistry::Register(String("Text"), [](void* ptr, DynamicArray<FieldInfo>& outInfo) {
					Text& obj = *static_cast<Text*>(ptr);
					outInfo.push_back({ String("フォントID"), offsetof(Text, fontID_), AttributeType::Int, PayloadAssetType::Font });
				});
			}
		};
		static Register_Text global_Text_register;

		// ---- GraphicsEngine/Light/SkyLight.h ----
		struct Register_SkyLight
		{
			Register_SkyLight()
			{
				PayloadRegistry::Register(String("SkyLight"), [](void* ptr, DynamicArray<FieldInfo>& outInfo) {
					SkyLight& obj = *static_cast<SkyLight*>(ptr);
					{
						FieldInfo fi;
						fi.name_ = String("スカイマップID");
						fi.offset_ = offsetof(SkyLight, skymapID_);
						fi.type_ = AttributeType::Int;
						fi.assetType_ = PayloadAssetType::Sky;
						fi.enableIf_ = [](void* p) -> Bool { auto& o = *static_cast<SkyLight*>(p); return o.useSkymap_; };
						outInfo.push_back(std::move(fi));
					}
				});
			}
		};
		static Register_SkyLight global_SkyLight_register;

		// ---- GraphicsEngine/Model/Mesh.h ----
		struct Register_Mesh
		{
			Register_Mesh()
			{
				PayloadRegistry::Register(String("Mesh"), [](void* ptr, DynamicArray<FieldInfo>& outInfo) {
					Mesh& obj = *static_cast<Mesh*>(ptr);
					outInfo.push_back({ String("メッシュID"), offsetof(Mesh, meshID_), AttributeType::Int, PayloadAssetType::Model });
				});
			}
		};
		static Register_Mesh global_Mesh_register;

		// ---- GraphicsEngine/Model/Animation/Animator.h ----
		struct Register_Animator
		{
			Register_Animator()
			{
				PayloadRegistry::Register(String("Animator"), [](void* ptr, DynamicArray<FieldInfo>& outInfo) {
					Animator& obj = *static_cast<Animator*>(ptr);
					{
						auto& arr = obj.animationIDs_;
						FieldInfo header;
						header.name_ = String("アニメーションID");
						header.offset_ = 0;
						header.type_ = AttributeType::Int;
						header.assetType_ = PayloadAssetType::Animation;
						header.array_.size_ = arr.size();
						header.array_.add_ = [&obj]() { obj.animationIDs_.push_back({}); };
						header.array_.remove_ = [&obj](Size idx) { if (idx < obj.animationIDs_.size()) obj.animationIDs_.erase(obj.animationIDs_.begin() + idx); };
						header.array_.lastPtr_ = [&obj]() -> void* { return &obj.animationIDs_.back(); };
						outInfo.push_back(std::move(header));
						for (Size i = 0; i < arr.size(); ++i)
						{
							outInfo.push_back({ String("[" + std::to_string(i) + "]"), 0, AttributeType::Int, PayloadAssetType::Animation, &arr[i] });
						}
					}
				});
			}
		};
		static Register_Animator global_Animator_register;

		// ---- GraphicsEngine/Model/Material/Material.h ----
		struct Register_Material
		{
			Register_Material()
			{
				PayloadRegistry::Register(String("Material"), [](void* ptr, DynamicArray<FieldInfo>& outInfo) {
					Material& obj = *static_cast<Material*>(ptr);
					{
						auto& arr = obj.materialIDs_;
						FieldInfo header;
						header.name_ = String("マテリアル");
						header.offset_ = 0;
						header.type_ = AttributeType::Int;
						header.assetType_ = PayloadAssetType::Material;
						header.array_.size_ = arr.size();
						header.array_.add_ = [&obj]() { obj.materialIDs_.push_back({}); };
						header.array_.remove_ = [&obj](Size idx) { if (idx < obj.materialIDs_.size()) obj.materialIDs_.erase(obj.materialIDs_.begin() + idx); };
						header.array_.lastPtr_ = [&obj]() -> void* { return &obj.materialIDs_.back(); };
						outInfo.push_back(std::move(header));
						for (Size i = 0; i < arr.size(); ++i)
						{
							outInfo.push_back({ String("[" + std::to_string(i) + "]"), 0, AttributeType::Int, PayloadAssetType::Material, &arr[i] });
						}
					}
				});
			}
		};
		static Register_Material global_Material_register;

		// ---- GraphicsEngine/Movie/Movie.h ----
		struct Register_Movie
		{
			Register_Movie()
			{
				PayloadRegistry::Register(String("Movie"), [](void* ptr, DynamicArray<FieldInfo>& outInfo) {
					Movie& obj = *static_cast<Movie*>(ptr);
					outInfo.push_back({ String("動画ID"), offsetof(Movie, movieID_), AttributeType::Int, PayloadAssetType::Movie });
				});
			}
		};
		static Register_Movie global_Movie_register;

		// ---- GraphicsEngine/Texture/Image.h ----
		struct Register_Image
		{
			Register_Image()
			{
				PayloadRegistry::Register(String("Image"), [](void* ptr, DynamicArray<FieldInfo>& outInfo) {
					Image& obj = *static_cast<Image*>(ptr);
					outInfo.push_back({ String("テクスチャID"), offsetof(Image, textureID_), AttributeType::Int, PayloadAssetType::Texture });
				});
			}
		};
		static Register_Image global_Image_register;

		// ---- PhysicsEngine/Collider/MeshCollider.h ----
		struct Register_MeshCollider
		{
			Register_MeshCollider()
			{
				PayloadRegistry::Register(String("MeshCollider"), [](void* ptr, DynamicArray<FieldInfo>& outInfo) {
					MeshCollider& obj = *static_cast<MeshCollider*>(ptr);
					outInfo.push_back({ String("コリジョンメッシュ"), offsetof(MeshCollider, meshID_), AttributeType::Int, PayloadAssetType::MeshCollision });
				});
			}
		};
		static Register_MeshCollider global_MeshCollider_register;

	}
}