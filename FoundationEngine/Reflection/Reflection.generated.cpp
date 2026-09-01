#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/ECS/ReflectionRegistry.h>
#include <FoundationEngine/ECS/Component/Lifetime.h>
#include <FoundationEngine/ECS/Component/Name.h>
#include <FoundationEngine/ECS/Component/Position.h>
#include <FoundationEngine/ECS/Component/Rotation.h>
#include <FoundationEngine/ECS/Component/Scale.h>
#include <FoundationEngine/ECS/Component/Spawner.h>
#include <FoundationEngine/ECS/Component/Velocity.h>
#include <GraphicsEngine/Camera/Camera.h>
#include <GraphicsEngine/Camera/FreeCameraController.h>
#include <GraphicsEngine/Camera/OrbitCameraController.h>
#include <GraphicsEngine/Constraint/AttachmentConstraint.h>
#include <GraphicsEngine/Constraint/LookAtConstraint.h>
#include <GraphicsEngine/Constraint/ParentConstraint.h>
#include <GraphicsEngine/Constraint/PositionConstraint.h>
#include <GraphicsEngine/Constraint/RotationConstraint.h>
#include <GraphicsEngine/Environment/Weather.h>
#include <GraphicsEngine/Font/Text.h>
#include <GraphicsEngine/Light/DirectionalLight.h>
#include <GraphicsEngine/Light/PointLight.h>
#include <GraphicsEngine/Light/RectangleLight.h>
#include <GraphicsEngine/Light/SkyLight.h>
#include <GraphicsEngine/Light/SpotLight.h>
#include <GraphicsEngine/Model/Animation/Animator.h>
#include <GraphicsEngine/Movie/Movie.h>
#include <GraphicsEngine/PostProcess/PostProcess.h>
#include <GraphicsEngine/Texture/Image.h>
#include <PhysicsEngine/CharacterController/CharacterController.h>
#include <PhysicsEngine/Collider/BoxCollider.h>
#include <PhysicsEngine/Collider/CapsuleCollider.h>
#include <PhysicsEngine/Collider/CircleCollider.h>
#include <PhysicsEngine/Collider/CylinderCollider.h>
#include <PhysicsEngine/Collider/MeshCollider.h>
#include <PhysicsEngine/Collider/RectCollider.h>
#include <PhysicsEngine/Collider/SphereCollider.h>
#include <PhysicsEngine/Joint/FixedJoint.h>
#include <PhysicsEngine/Joint/HingeJoint.h>
#include <PhysicsEngine/Joint/SliderJoint.h>
#include <PhysicsEngine/Joint/SpringJoint.h>
#include <PhysicsEngine/Rigidbody/Rigidbody.h>
#include <PhysicsEngine/Softbody/Softbody.h>

extern "C" int _force_reflection_Lifetime = 0;
extern "C" int _force_reflection_Name = 0;
extern "C" int _force_reflection_Position = 0;
extern "C" int _force_reflection_Rotation = 0;
extern "C" int _force_reflection_Scale = 0;
extern "C" int _force_reflection_Spawner = 0;
extern "C" int _force_reflection_Velocity = 0;
extern "C" int _force_reflection_Camera = 0;
extern "C" int _force_reflection_FreeCameraController = 0;
extern "C" int _force_reflection_OrbitCameraController = 0;
extern "C" int _force_reflection_AttachmentConstraint = 0;
extern "C" int _force_reflection_LookAtConstraint = 0;
extern "C" int _force_reflection_ParentConstraint = 0;
extern "C" int _force_reflection_PositionConstraint = 0;
extern "C" int _force_reflection_RotationConstraint = 0;
extern "C" int _force_reflection_Weather = 0;
extern "C" int _force_reflection_Text = 0;
extern "C" int _force_reflection_DirectionalLight = 0;
extern "C" int _force_reflection_PointLight = 0;
extern "C" int _force_reflection_RectangleLight = 0;
extern "C" int _force_reflection_SkyLight = 0;
extern "C" int _force_reflection_SpotLight = 0;
extern "C" int _force_reflection_AnimationParameter = 0;
extern "C" int _force_reflection_AnimationCondition = 0;
extern "C" int _force_reflection_AnimationState = 0;
extern "C" int _force_reflection_AnimationTransition = 0;
extern "C" int _force_reflection_Animator = 0;
extern "C" int _force_reflection_Movie = 0;
extern "C" int _force_reflection_DepthOfFieldSettings = 0;
extern "C" int _force_reflection_BokehSettings = 0;
extern "C" int _force_reflection_LensDistortionSettings = 0;
extern "C" int _force_reflection_ChromaticAberrationSettings = 0;
extern "C" int _force_reflection_VignetteSettings = 0;
extern "C" int _force_reflection_BloomSettings = 0;
extern "C" int _force_reflection_AnamorphicFlareSettings = 0;
extern "C" int _force_reflection_LensFlareSettings = 0;
extern "C" int _force_reflection_ExposureSettings = 0;
extern "C" int _force_reflection_ColorGradingRangeSettings = 0;
extern "C" int _force_reflection_ColorGradingSettings = 0;
extern "C" int _force_reflection_ToneMappingSettings = 0;
extern "C" int _force_reflection_SharpnessSettings = 0;
extern "C" int _force_reflection_FilmGrainSettings = 0;
extern "C" int _force_reflection_PostProcess = 0;
extern "C" int _force_reflection_Image = 0;
extern "C" int _force_reflection_CharacterController = 0;
extern "C" int _force_reflection_BoxCollider = 0;
extern "C" int _force_reflection_CapsuleCollider = 0;
extern "C" int _force_reflection_CircleCollider = 0;
extern "C" int _force_reflection_CylinderCollider = 0;
extern "C" int _force_reflection_MeshCollider = 0;
extern "C" int _force_reflection_RectCollider = 0;
extern "C" int _force_reflection_SphereCollider = 0;
extern "C" int _force_reflection_FixedJoint = 0;
extern "C" int _force_reflection_HingeJoint = 0;
extern "C" int _force_reflection_SliderJoint = 0;
extern "C" int _force_reflection_SpringJoint = 0;
extern "C" int _force_reflection_Rigidbody = 0;
extern "C" int _force_reflection_Softbody = 0;

namespace SeedCore
{
	 namespace ScReflection
	 {
		// ---- FoundationEngine/ECS/Component/Lifetime.h ----
		struct Register_Lifetime
		{
			Register_Lifetime()
			{
				ReflectionRegistry::Register(String("Lifetime"), [](void* ptr, DynamicArray<FieldInfo>& outInfo) {
					Lifetime& obj = *static_cast<Lifetime*>(ptr);
					{
						FieldInfo fi;
						fi.name_ = String("生存時間(秒)");
						fi.offset_ = offsetof(Lifetime, duration_);
						fi.type_ = AttributeType::Float;
						fi.clampMin_ = 0.05f;
						fi.clampMax_ = 3600.0f;
						outInfo.push_back(std::move(fi));
					}
				});
			}
		};
		static Register_Lifetime global_Lifetime_register;

		// ---- FoundationEngine/ECS/Component/Name.h ----
		struct Register_Name
		{
			Register_Name()
			{
				ReflectionRegistry::Register(String("Name"), [](void* ptr, DynamicArray<FieldInfo>& outInfo) {
					Name& obj = *static_cast<Name*>(ptr);
					{
						FieldInfo fi;
						fi.name_ = String("name_");
						fi.offset_ = offsetof(Name, name_);
						fi.type_ = AttributeType::String;
						fi.editorVisible_ = false;
						outInfo.push_back(std::move(fi));
					}
				});
			}
		};
		static Register_Name global_Name_register;

		// ---- FoundationEngine/ECS/Component/Position.h ----
		struct Register_Position
		{
			Register_Position()
			{
				ReflectionRegistry::Register(String("Position"), [](void* ptr, DynamicArray<FieldInfo>& outInfo) {
					Position& obj = *static_cast<Position*>(ptr);
					{
						FieldInfo fi;
						fi.name_ = String("x_");
						fi.offset_ = offsetof(Position, x_);
						fi.type_ = AttributeType::Float;
						fi.editorVisible_ = false;
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("y_");
						fi.offset_ = offsetof(Position, y_);
						fi.type_ = AttributeType::Float;
						fi.editorVisible_ = false;
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("z_");
						fi.offset_ = offsetof(Position, z_);
						fi.type_ = AttributeType::Float;
						fi.editorVisible_ = false;
						outInfo.push_back(std::move(fi));
					}
				});
			}
		};
		static Register_Position global_Position_register;

		// ---- FoundationEngine/ECS/Component/Rotation.h ----
		struct Register_Rotation
		{
			Register_Rotation()
			{
				ReflectionRegistry::Register(String("Rotation"), [](void* ptr, DynamicArray<FieldInfo>& outInfo) {
					Rotation& obj = *static_cast<Rotation*>(ptr);
					{
						FieldInfo fi;
						fi.name_ = String("x_");
						fi.offset_ = offsetof(Rotation, x_);
						fi.type_ = AttributeType::Float;
						fi.editorVisible_ = false;
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("y_");
						fi.offset_ = offsetof(Rotation, y_);
						fi.type_ = AttributeType::Float;
						fi.editorVisible_ = false;
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("z_");
						fi.offset_ = offsetof(Rotation, z_);
						fi.type_ = AttributeType::Float;
						fi.editorVisible_ = false;
						outInfo.push_back(std::move(fi));
					}
				});
			}
		};
		static Register_Rotation global_Rotation_register;

		// ---- FoundationEngine/ECS/Component/Scale.h ----
		struct Register_Scale
		{
			Register_Scale()
			{
				ReflectionRegistry::Register(String("Scale"), [](void* ptr, DynamicArray<FieldInfo>& outInfo) {
					Scale& obj = *static_cast<Scale*>(ptr);
					{
						FieldInfo fi;
						fi.name_ = String("x_");
						fi.offset_ = offsetof(Scale, x_);
						fi.type_ = AttributeType::Float;
						fi.editorVisible_ = false;
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("y_");
						fi.offset_ = offsetof(Scale, y_);
						fi.type_ = AttributeType::Float;
						fi.editorVisible_ = false;
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("z_");
						fi.offset_ = offsetof(Scale, z_);
						fi.type_ = AttributeType::Float;
						fi.editorVisible_ = false;
						outInfo.push_back(std::move(fi));
					}
				});
			}
		};
		static Register_Scale global_Scale_register;

		// ---- FoundationEngine/ECS/Component/Spawner.h ----
		struct Register_Spawner
		{
			Register_Spawner()
			{
				ReflectionRegistry::Register(String("Spawner"), [](void* ptr, DynamicArray<FieldInfo>& outInfo) {
					Spawner& obj = *static_cast<Spawner*>(ptr);
					outInfo.push_back({ String("開始時に自動生成"), offsetof(Spawner, autoStart_), AttributeType::Bool });
					{
						FieldInfo fi;
						fi.name_ = String("生成間隔");
						fi.offset_ = offsetof(Spawner, spawnInterval_);
						fi.type_ = AttributeType::Float;
						fi.clampMin_ = 0.0f;
						fi.clampMax_ = 3600.0f;
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("最大生成数");
						fi.offset_ = offsetof(Spawner, maxCount_);
						fi.type_ = AttributeType::Int;
						fi.clampMin_ = 1;
						fi.clampMax_ = 1000;
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("生存時間");
						fi.offset_ = offsetof(Spawner, lifeTime_);
						fi.type_ = AttributeType::Float;
						fi.clampMin_ = 0.1f;
						fi.clampMax_ = 3600.0f;
						outInfo.push_back(std::move(fi));
					}
					outInfo.push_back({ String("ランダムスポーン"), offsetof(Spawner, randomSpawn_), AttributeType::Bool });
					{
						FieldInfo fi;
						fi.name_ = String("ランダム範囲");
						fi.offset_ = offsetof(Spawner, randomRadius_);
						fi.type_ = AttributeType::Float;
						fi.enableIf_ = [](void* p) -> Bool { auto& o = *static_cast<Spawner*>(p); return o.randomSpawn_; };
						fi.clampMin_ = 0.0f;
						fi.clampMax_ = 1000.0f;
						outInfo.push_back(std::move(fi));
					}
				});
			}
		};
		static Register_Spawner global_Spawner_register;

		// ---- FoundationEngine/ECS/Component/Velocity.h ----
		struct Register_Velocity
		{
			Register_Velocity()
			{
				ReflectionRegistry::Register(String("Velocity"), [](void* ptr, DynamicArray<FieldInfo>& outInfo) {
					Velocity& obj = *static_cast<Velocity*>(ptr);
					{
						FieldInfo fi;
						fi.name_ = String("x_");
						fi.offset_ = offsetof(Velocity, x_);
						fi.type_ = AttributeType::Float;
						fi.editorVisible_ = false;
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("y_");
						fi.offset_ = offsetof(Velocity, y_);
						fi.type_ = AttributeType::Float;
						fi.editorVisible_ = false;
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("z_");
						fi.offset_ = offsetof(Velocity, z_);
						fi.type_ = AttributeType::Float;
						fi.editorVisible_ = false;
						outInfo.push_back(std::move(fi));
					}
				});
			}
		};
		static Register_Velocity global_Velocity_register;

		// ---- GraphicsEngine/Camera/Camera.h ----
		struct Register_Camera
		{
			Register_Camera()
			{
				ReflectionRegistry::Register(String("Camera"), [](void* ptr, DynamicArray<FieldInfo>& outInfo) {
					Camera& obj = *static_cast<Camera*>(ptr);
					outInfo.push_back({ String("FOV"), offsetof(Camera, fieldOfView_), AttributeType::Float });
					outInfo.push_back({ String("近クリップ面"), offsetof(Camera, nearPlane_), AttributeType::Float });
					outInfo.push_back({ String("遠クリップ面"), offsetof(Camera, farPlane_), AttributeType::Float });
					outInfo.push_back({ String("アクティブカメラ"), offsetof(Camera, isActive_), AttributeType::Bool });
				});
			}
		};
		static Register_Camera global_Camera_register;

		// ---- GraphicsEngine/Camera/FreeCameraController.h ----
		struct Register_FreeCameraController
		{
			Register_FreeCameraController()
			{
				ReflectionRegistry::Register(String("FreeCameraController"), [](void* ptr, DynamicArray<FieldInfo>& outInfo) {
					FreeCameraController& obj = *static_cast<FreeCameraController*>(ptr);
					outInfo.push_back({ String("移動速度"), offsetof(FreeCameraController, moveSpeed_), AttributeType::Float });
					outInfo.push_back({ String("回転速度"), offsetof(FreeCameraController, rotateSpeed_), AttributeType::Float });
					outInfo.push_back({ String("スクロール速度"), offsetof(FreeCameraController, scrollSpeed_), AttributeType::Float });
					outInfo.push_back({ String("パン速度"), offsetof(FreeCameraController, panSpeed_), AttributeType::Float });
					outInfo.push_back({ String("Shift速度倍率"), offsetof(FreeCameraController, shiftSpeedMultiplier_), AttributeType::Float });
				});
			}
		};
		static Register_FreeCameraController global_FreeCameraController_register;

		// ---- GraphicsEngine/Camera/OrbitCameraController.h ----
		struct Register_OrbitCameraController
		{
			Register_OrbitCameraController()
			{
				ReflectionRegistry::Register(String("OrbitCameraController"), [](void* ptr, DynamicArray<FieldInfo>& outInfo) {
					OrbitCameraController& obj = *static_cast<OrbitCameraController*>(ptr);
					outInfo.push_back({ String("注視点X"), offsetof(OrbitCameraController, targetX_), AttributeType::Float });
					outInfo.push_back({ String("注視点Y"), offsetof(OrbitCameraController, targetY_), AttributeType::Float });
					outInfo.push_back({ String("注視点Z"), offsetof(OrbitCameraController, targetZ_), AttributeType::Float });
					{
						FieldInfo fi;
						fi.name_ = String("距離");
						fi.offset_ = offsetof(OrbitCameraController, distance_);
						fi.type_ = AttributeType::Float;
						fi.clampMin_ = 0.1f;
						fi.clampMax_ = 1000.0f;
						outInfo.push_back(std::move(fi));
					}
					outInfo.push_back({ String("回転速度"), offsetof(OrbitCameraController, rotateSpeed_), AttributeType::Float });
					outInfo.push_back({ String("ズーム速度"), offsetof(OrbitCameraController, zoomSpeed_), AttributeType::Float });
					{
						FieldInfo fi;
						fi.name_ = String("最小距離");
						fi.offset_ = offsetof(OrbitCameraController, minDistance_);
						fi.type_ = AttributeType::Float;
						fi.clampMin_ = 0.1f;
						fi.clampMax_ = 1000.0f;
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("最大距離");
						fi.offset_ = offsetof(OrbitCameraController, maxDistance_);
						fi.type_ = AttributeType::Float;
						fi.clampMin_ = 0.1f;
						fi.clampMax_ = 10000.0f;
						outInfo.push_back(std::move(fi));
					}
				});
			}
		};
		static Register_OrbitCameraController global_OrbitCameraController_register;

		// ---- GraphicsEngine/Constraint/AttachmentConstraint.h ----
		struct Register_AttachmentConstraint
		{
			Register_AttachmentConstraint()
			{
				ReflectionRegistry::Register(String("AttachmentConstraint"), [](void* ptr, DynamicArray<FieldInfo>& outInfo) {
					AttachmentConstraint& obj = *static_cast<AttachmentConstraint*>(ptr);
					outInfo.push_back({ String("有効"), offsetof(AttachmentConstraint, enabled_), AttributeType::Bool });
					{
						FieldInfo fi;
						fi.name_ = String("重み");
						fi.offset_ = offsetof(AttachmentConstraint, weight_);
						fi.type_ = AttributeType::Float;
						fi.clampMin_ = 0.0f;
						fi.clampMax_ = 1.0f;
						outInfo.push_back(std::move(fi));
					}
					outInfo.push_back({ String("位置オフセット"), offsetof(AttachmentConstraint, positionOffset_), AttributeType::Vector3 });
					outInfo.push_back({ String("回転オフセット(オイラー角)"), offsetof(AttachmentConstraint, rotationOffset_), AttributeType::Vector3 });
					{
						FieldInfo fi;
						fi.name_ = String("boneName_");
						fi.offset_ = offsetof(AttachmentConstraint, boneName_);
						fi.type_ = AttributeType::String;
						fi.editorVisible_ = false;
						outInfo.push_back(std::move(fi));
					}
				});
			}
		};
		static Register_AttachmentConstraint global_AttachmentConstraint_register;

		// ---- GraphicsEngine/Constraint/LookAtConstraint.h ----
		struct Register_LookAtConstraint
		{
			Register_LookAtConstraint()
			{
				ReflectionRegistry::Register(String("LookAtConstraint"), [](void* ptr, DynamicArray<FieldInfo>& outInfo) {
					LookAtConstraint& obj = *static_cast<LookAtConstraint*>(ptr);
					outInfo.push_back({ String("有効"), offsetof(LookAtConstraint, enabled_), AttributeType::Bool });
					{
						FieldInfo fi;
						fi.name_ = String("重み");
						fi.offset_ = offsetof(LookAtConstraint, weight_);
						fi.type_ = AttributeType::Float;
						fi.clampMin_ = 0.0f;
						fi.clampMax_ = 1.0f;
						outInfo.push_back(std::move(fi));
					}
					outInfo.push_back({ String("上方向"), offsetof(LookAtConstraint, upVector_), AttributeType::Vector3 });
				});
			}
		};
		static Register_LookAtConstraint global_LookAtConstraint_register;

		// ---- GraphicsEngine/Constraint/ParentConstraint.h ----
		struct Register_ParentConstraint
		{
			Register_ParentConstraint()
			{
				ReflectionRegistry::Register(String("ParentConstraint"), [](void* ptr, DynamicArray<FieldInfo>& outInfo) {
					ParentConstraint& obj = *static_cast<ParentConstraint*>(ptr);
					outInfo.push_back({ String("有効"), offsetof(ParentConstraint, enabled_), AttributeType::Bool });
					{
						FieldInfo fi;
						fi.name_ = String("重み");
						fi.offset_ = offsetof(ParentConstraint, weight_);
						fi.type_ = AttributeType::Float;
						fi.clampMin_ = 0.0f;
						fi.clampMax_ = 1.0f;
						outInfo.push_back(std::move(fi));
					}
					outInfo.push_back({ String("位置オフセット"), offsetof(ParentConstraint, positionOffset_), AttributeType::Vector3 });
					outInfo.push_back({ String("回転オフセット(オイラー角)"), offsetof(ParentConstraint, rotationOffset_), AttributeType::Vector3 });
				});
			}
		};
		static Register_ParentConstraint global_ParentConstraint_register;

		// ---- GraphicsEngine/Constraint/PositionConstraint.h ----
		struct Register_PositionConstraint
		{
			Register_PositionConstraint()
			{
				ReflectionRegistry::Register(String("PositionConstraint"), [](void* ptr, DynamicArray<FieldInfo>& outInfo) {
					PositionConstraint& obj = *static_cast<PositionConstraint*>(ptr);
					outInfo.push_back({ String("有効"), offsetof(PositionConstraint, enabled_), AttributeType::Bool });
					{
						FieldInfo fi;
						fi.name_ = String("重み");
						fi.offset_ = offsetof(PositionConstraint, weight_);
						fi.type_ = AttributeType::Float;
						fi.clampMin_ = 0.0f;
						fi.clampMax_ = 1.0f;
						outInfo.push_back(std::move(fi));
					}
					outInfo.push_back({ String("オフセット"), offsetof(PositionConstraint, offset_), AttributeType::Vector3 });
				});
			}
		};
		static Register_PositionConstraint global_PositionConstraint_register;

		// ---- GraphicsEngine/Constraint/RotationConstraint.h ----
		struct Register_RotationConstraint
		{
			Register_RotationConstraint()
			{
				ReflectionRegistry::Register(String("RotationConstraint"), [](void* ptr, DynamicArray<FieldInfo>& outInfo) {
					RotationConstraint& obj = *static_cast<RotationConstraint*>(ptr);
					outInfo.push_back({ String("有効"), offsetof(RotationConstraint, enabled_), AttributeType::Bool });
					{
						FieldInfo fi;
						fi.name_ = String("重み");
						fi.offset_ = offsetof(RotationConstraint, weight_);
						fi.type_ = AttributeType::Float;
						fi.clampMin_ = 0.0f;
						fi.clampMax_ = 1.0f;
						outInfo.push_back(std::move(fi));
					}
					outInfo.push_back({ String("オフセット(オイラー角)"), offsetof(RotationConstraint, offset_), AttributeType::Vector3 });
				});
			}
		};
		static Register_RotationConstraint global_RotationConstraint_register;

		// ---- GraphicsEngine/Environment/Weather.h ----
		struct Register_Weather
		{
			Register_Weather()
			{
				ReflectionRegistry::Register(String("Weather"), [](void* ptr, DynamicArray<FieldInfo>& outInfo) {
					Weather& obj = *static_cast<Weather*>(ptr);
					{
						FieldInfo fi;
						fi.name_ = String("天候");
						fi.offset_ = offsetof(Weather, type_);
						fi.type_ = AttributeType::Enum;
						fi.enum_.typeName_ = String("WeatherType");
						outInfo.push_back(std::move(fi));
					}
					outInfo.push_back({ String("自動サイクル"), offsetof(Weather, autoCycle_), AttributeType::Bool });
					{
						FieldInfo fi;
						fi.name_ = String("最短間隔(分)");
						fi.offset_ = offsetof(Weather, minIntervalMinutes_);
						fi.type_ = AttributeType::Float;
						fi.clampMin_ = 0.1f;
						fi.clampMax_ = 180.0f;
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("最長間隔(分)");
						fi.offset_ = offsetof(Weather, maxIntervalMinutes_);
						fi.type_ = AttributeType::Float;
						fi.clampMin_ = 0.1f;
						fi.clampMax_ = 180.0f;
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("遷移時間(秒)");
						fi.offset_ = offsetof(Weather, transitionSeconds_);
						fi.type_ = AttributeType::Float;
						fi.clampMin_ = 0.1f;
						fi.clampMax_ = 300.0f;
						outInfo.push_back(std::move(fi));
					}
					outInfo.push_back({ String("雨を季節無視で許可"), offsetof(Weather, forceRain_), AttributeType::Bool });
					outInfo.push_back({ String("雪を季節無視で許可"), offsetof(Weather, forceSnow_), AttributeType::Bool });
					outInfo.push_back({ String("雷を強制的に許可"), offsetof(Weather, forceThunder_), AttributeType::Bool });
					outInfo.push_back({ String("暴風を強制的に許可"), offsetof(Weather, forceStorm_), AttributeType::Bool });
				});
			}
		};
		static Register_Weather global_Weather_register;

		// ---- GraphicsEngine/Font/Text.h ----
		struct Register_Text
		{
			Register_Text()
			{
				ReflectionRegistry::Register(String("Text"), [](void* ptr, DynamicArray<FieldInfo>& outInfo) {
					Text& obj = *static_cast<Text*>(ptr);
					outInfo.push_back({ String("テキスト"), offsetof(Text, text_), AttributeType::String });
					outInfo.push_back({ String("文字サイズ"), offsetof(Text, fontSize_), AttributeType::Float });
					outInfo.push_back({ String("文字間隔"), offsetof(Text, letterSpacing_), AttributeType::Float });
					outInfo.push_back({ String("行間"), offsetof(Text, lineSpacing_), AttributeType::Float });
					outInfo.push_back({ String("中心点"), offsetof(Text, pivot_), AttributeType::Vector2 });
					outInfo.push_back({ String("色"), offsetof(Text, color_), AttributeType::Color });
					{
						FieldInfo fi;
						fi.name_ = String("アウトライン幅");
						fi.offset_ = offsetof(Text, outlineWidth_);
						fi.type_ = AttributeType::Float;
						fi.clampMin_ = 0.0f;
						fi.clampMax_ = 28.0f;
						outInfo.push_back(std::move(fi));
					}
					outInfo.push_back({ String("アウトライン色"), offsetof(Text, outlineColor_), AttributeType::Color });
					{
						FieldInfo fi;
						fi.name_ = String("発光量");
						fi.offset_ = offsetof(Text, glowPower_);
						fi.type_ = AttributeType::Float;
						fi.clampMin_ = 0.0f;
						fi.clampMax_ = 8.0f;
						outInfo.push_back(std::move(fi));
					}
					outInfo.push_back({ String("発光色"), offsetof(Text, glowColor_), AttributeType::Color });
					{
						FieldInfo fi;
						fi.name_ = String("表示形式");
						fi.offset_ = offsetof(Text, viewType_);
						fi.type_ = AttributeType::Enum;
						fi.enum_.typeName_ = String("ViewType");
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("カメラ正対");
						fi.offset_ = offsetof(Text, faceCamera_);
						fi.type_ = AttributeType::Bool;
						fi.enableIf_ = [](void* p) -> Bool { auto& o = *static_cast<Text*>(p); return o.viewType_ == Text::ViewType::Billboard; };
						outInfo.push_back(std::move(fi));
					}
					outInfo.push_back({ String("影"), offsetof(Text, shadowEnable_), AttributeType::Bool });
					{
						FieldInfo fi;
						fi.name_ = String("影オフセット");
						fi.offset_ = offsetof(Text, shadowOffset_);
						fi.type_ = AttributeType::Vector2;
						fi.enableIf_ = [](void* p) -> Bool { auto& o = *static_cast<Text*>(p); return o.shadowEnable_; };
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("影色");
						fi.offset_ = offsetof(Text, shadowColor_);
						fi.type_ = AttributeType::Color;
						fi.enableIf_ = [](void* p) -> Bool { auto& o = *static_cast<Text*>(p); return o.shadowEnable_; };
						outInfo.push_back(std::move(fi));
					}
				});
			}
		};
		static Register_Text global_Text_register;

		// ---- GraphicsEngine/Light/DirectionalLight.h ----
		struct Register_DirectionalLight
		{
			Register_DirectionalLight()
			{
				ReflectionRegistry::Register(String("DirectionalLight"), [](void* ptr, DynamicArray<FieldInfo>& outInfo) {
					DirectionalLight& obj = *static_cast<DirectionalLight*>(ptr);
					outInfo.push_back({ String("色"), offsetof(DirectionalLight, color_), AttributeType::Color });
					{
						FieldInfo fi;
						fi.name_ = String("強度");
						fi.offset_ = offsetof(DirectionalLight, intensity_);
						fi.type_ = AttributeType::Float;
						fi.clampMin_ = 0.0f;
						fi.clampMax_ = 200.0f;
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("向き");
						fi.offset_ = offsetof(DirectionalLight, direction_);
						fi.type_ = AttributeType::Vector3;
						fi.clampMin_ = -1.0f;
						fi.clampMax_ = 1.0f;
						outInfo.push_back(std::move(fi));
					}
				});
			}
		};
		static Register_DirectionalLight global_DirectionalLight_register;

		// ---- GraphicsEngine/Light/PointLight.h ----
		struct Register_PointLight
		{
			Register_PointLight()
			{
				ReflectionRegistry::Register(String("PointLight"), [](void* ptr, DynamicArray<FieldInfo>& outInfo) {
					PointLight& obj = *static_cast<PointLight*>(ptr);
					outInfo.push_back({ String("色"), offsetof(PointLight, color_), AttributeType::Color });
					outInfo.push_back({ String("範囲"), offsetof(PointLight, range_), AttributeType::Float });
					{
						FieldInfo fi;
						fi.name_ = String("強度");
						fi.offset_ = offsetof(PointLight, intensity_);
						fi.type_ = AttributeType::Float;
						fi.clampMin_ = 0.0f;
						fi.clampMax_ = 200.0f;
						outInfo.push_back(std::move(fi));
					}
				});
			}
		};
		static Register_PointLight global_PointLight_register;

		// ---- GraphicsEngine/Light/RectangleLight.h ----
		struct Register_RectangleLight
		{
			Register_RectangleLight()
			{
				ReflectionRegistry::Register(String("RectangleLight"), [](void* ptr, DynamicArray<FieldInfo>& outInfo) {
					RectangleLight& obj = *static_cast<RectangleLight*>(ptr);
					outInfo.push_back({ String("色"), offsetof(RectangleLight, color_), AttributeType::Color });
					{
						FieldInfo fi;
						fi.name_ = String("正面の向き");
						fi.offset_ = offsetof(RectangleLight, direction_);
						fi.type_ = AttributeType::Vector3;
						fi.clampMin_ = -1.0f;
						fi.clampMax_ = 1.0f;
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("上方向");
						fi.offset_ = offsetof(RectangleLight, up_);
						fi.type_ = AttributeType::Vector3;
						fi.clampMin_ = -1.0f;
						fi.clampMax_ = 1.0f;
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("幅");
						fi.offset_ = offsetof(RectangleLight, width_);
						fi.type_ = AttributeType::Float;
						fi.clampMin_ = 0.01f;
						fi.clampMax_ = 100.0f;
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("高さ");
						fi.offset_ = offsetof(RectangleLight, height_);
						fi.type_ = AttributeType::Float;
						fi.clampMin_ = 0.01f;
						fi.clampMax_ = 100.0f;
						outInfo.push_back(std::move(fi));
					}
					outInfo.push_back({ String("範囲"), offsetof(RectangleLight, range_), AttributeType::Float });
					{
						FieldInfo fi;
						fi.name_ = String("強度");
						fi.offset_ = offsetof(RectangleLight, intensity_);
						fi.type_ = AttributeType::Float;
						fi.clampMin_ = 0.0f;
						fi.clampMax_ = 200.0f;
						outInfo.push_back(std::move(fi));
					}
				});
			}
		};
		static Register_RectangleLight global_RectangleLight_register;

		// ---- GraphicsEngine/Light/SkyLight.h ----
		struct Register_SkyLight
		{
			Register_SkyLight()
			{
				ReflectionRegistry::Register(String("SkyLight"), [](void* ptr, DynamicArray<FieldInfo>& outInfo) {
					SkyLight& obj = *static_cast<SkyLight*>(ptr);
					outInfo.push_back({ String("スカイマップ使用"), offsetof(SkyLight, useSkymap_), AttributeType::Bool });
					{
						FieldInfo fi;
						fi.name_ = String("強度");
						fi.offset_ = offsetof(SkyLight, intensity_);
						fi.type_ = AttributeType::Float;
						fi.clampMin_ = 0.0f;
						fi.clampMax_ = 1.0f;
						outInfo.push_back(std::move(fi));
					}
				});
			}
		};
		static Register_SkyLight global_SkyLight_register;

		// ---- GraphicsEngine/Light/SpotLight.h ----
		struct Register_SpotLight
		{
			Register_SpotLight()
			{
				ReflectionRegistry::Register(String("SpotLight"), [](void* ptr, DynamicArray<FieldInfo>& outInfo) {
					SpotLight& obj = *static_cast<SpotLight*>(ptr);
					outInfo.push_back({ String("色"), offsetof(SpotLight, color_), AttributeType::Color });
					{
						FieldInfo fi;
						fi.name_ = String("向き");
						fi.offset_ = offsetof(SpotLight, direction_);
						fi.type_ = AttributeType::Vector3;
						fi.clampMin_ = -1.0f;
						fi.clampMax_ = 1.0f;
						outInfo.push_back(std::move(fi));
					}
					outInfo.push_back({ String("範囲"), offsetof(SpotLight, range_), AttributeType::Float });
					{
						FieldInfo fi;
						fi.name_ = String("強度");
						fi.offset_ = offsetof(SpotLight, intensity_);
						fi.type_ = AttributeType::Float;
						fi.clampMin_ = 0.0f;
						fi.clampMax_ = 200.0f;
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("スポット角度");
						fi.offset_ = offsetof(SpotLight, spotAngle_);
						fi.type_ = AttributeType::Float;
						fi.clampMin_ = 1.0f;
						fi.clampMax_ = 179.0f;
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("ソフトネス");
						fi.offset_ = offsetof(SpotLight, softness_);
						fi.type_ = AttributeType::Float;
						fi.clampMin_ = 0.0f;
						fi.clampMax_ = 1.0f;
						outInfo.push_back(std::move(fi));
					}
				});
			}
		};
		static Register_SpotLight global_SpotLight_register;

		// ---- GraphicsEngine/Model/Animation/Animator.h ----
		struct Register_AnimationParameter
		{
			Register_AnimationParameter()
			{
				ReflectionRegistry::Register(String("AnimationParameter"), [](void* ptr, DynamicArray<FieldInfo>& outInfo) {
					AnimationParameter& obj = *static_cast<AnimationParameter*>(ptr);
					{
						FieldInfo fi;
						fi.name_ = String("name_");
						fi.offset_ = offsetof(AnimationParameter, name_);
						fi.type_ = AttributeType::String;
						fi.editorVisible_ = false;
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("type_");
						fi.offset_ = offsetof(AnimationParameter, type_);
						fi.type_ = AttributeType::Enum;
						fi.enum_.typeName_ = String("AnimationParameterType");
						fi.editorVisible_ = false;
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("value_");
						fi.offset_ = offsetof(AnimationParameter, value_);
						fi.type_ = AttributeType::Float;
						fi.editorVisible_ = false;
						outInfo.push_back(std::move(fi));
					}
				});
			}
		};
		static Register_AnimationParameter global_AnimationParameter_register;

		struct Register_AnimationCondition
		{
			Register_AnimationCondition()
			{
				ReflectionRegistry::Register(String("AnimationCondition"), [](void* ptr, DynamicArray<FieldInfo>& outInfo) {
					AnimationCondition& obj = *static_cast<AnimationCondition*>(ptr);
					{
						FieldInfo fi;
						fi.name_ = String("parameterName_");
						fi.offset_ = offsetof(AnimationCondition, parameterName_);
						fi.type_ = AttributeType::String;
						fi.editorVisible_ = false;
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("comparison_");
						fi.offset_ = offsetof(AnimationCondition, comparison_);
						fi.type_ = AttributeType::Enum;
						fi.enum_.typeName_ = String("AnimationConditionComparison");
						fi.editorVisible_ = false;
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("value_");
						fi.offset_ = offsetof(AnimationCondition, value_);
						fi.type_ = AttributeType::Float;
						fi.editorVisible_ = false;
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("isOr_");
						fi.offset_ = offsetof(AnimationCondition, isOr_);
						fi.type_ = AttributeType::Bool;
						fi.editorVisible_ = false;
						outInfo.push_back(std::move(fi));
					}
				});
			}
		};
		static Register_AnimationCondition global_AnimationCondition_register;

		struct Register_AnimationState
		{
			Register_AnimationState()
			{
				ReflectionRegistry::Register(String("AnimationState"), [](void* ptr, DynamicArray<FieldInfo>& outInfo) {
					AnimationState& obj = *static_cast<AnimationState*>(ptr);
					{
						FieldInfo fi;
						fi.name_ = String("name_");
						fi.offset_ = offsetof(AnimationState, name_);
						fi.type_ = AttributeType::String;
						fi.editorVisible_ = false;
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("animationID_");
						fi.offset_ = offsetof(AnimationState, animationID_);
						fi.type_ = AttributeType::Int;
						fi.editorVisible_ = false;
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("nodePositionX_");
						fi.offset_ = offsetof(AnimationState, nodePositionX_);
						fi.type_ = AttributeType::Float;
						fi.editorVisible_ = false;
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("nodePositionY_");
						fi.offset_ = offsetof(AnimationState, nodePositionY_);
						fi.type_ = AttributeType::Float;
						fi.editorVisible_ = false;
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("useRootMotion_");
						fi.offset_ = offsetof(AnimationState, useRootMotion_);
						fi.type_ = AttributeType::Bool;
						fi.editorVisible_ = false;
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("useIK_");
						fi.offset_ = offsetof(AnimationState, useIK_);
						fi.type_ = AttributeType::Bool;
						fi.editorVisible_ = false;
						outInfo.push_back(std::move(fi));
					}
				});
			}
		};
		static Register_AnimationState global_AnimationState_register;

		struct Register_AnimationTransition
		{
			Register_AnimationTransition()
			{
				ReflectionRegistry::Register(String("AnimationTransition"), [](void* ptr, DynamicArray<FieldInfo>& outInfo) {
					AnimationTransition& obj = *static_cast<AnimationTransition*>(ptr);
					{
						FieldInfo fi;
						fi.name_ = String("fromState_");
						fi.offset_ = offsetof(AnimationTransition, fromState_);
						fi.type_ = AttributeType::Int;
						fi.editorVisible_ = false;
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("toState_");
						fi.offset_ = offsetof(AnimationTransition, toState_);
						fi.type_ = AttributeType::Int;
						fi.editorVisible_ = false;
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("duration_");
						fi.offset_ = offsetof(AnimationTransition, duration_);
						fi.type_ = AttributeType::Float;
						fi.editorVisible_ = false;
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("hasExitTime_");
						fi.offset_ = offsetof(AnimationTransition, hasExitTime_);
						fi.type_ = AttributeType::Bool;
						fi.editorVisible_ = false;
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("exitTime_");
						fi.offset_ = offsetof(AnimationTransition, exitTime_);
						fi.type_ = AttributeType::Float;
						fi.editorVisible_ = false;
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("fromOffsetX_");
						fi.offset_ = offsetof(AnimationTransition, fromOffsetX_);
						fi.type_ = AttributeType::Float;
						fi.editorVisible_ = false;
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("fromOffsetY_");
						fi.offset_ = offsetof(AnimationTransition, fromOffsetY_);
						fi.type_ = AttributeType::Float;
						fi.editorVisible_ = false;
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("toOffsetX_");
						fi.offset_ = offsetof(AnimationTransition, toOffsetX_);
						fi.type_ = AttributeType::Float;
						fi.editorVisible_ = false;
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("toOffsetY_");
						fi.offset_ = offsetof(AnimationTransition, toOffsetY_);
						fi.type_ = AttributeType::Float;
						fi.editorVisible_ = false;
						outInfo.push_back(std::move(fi));
					}
					{
						auto& arr = obj.conditions_;
						FieldInfo header;
						header.name_ = String("conditions_");
						header.offset_ = 0;
						header.type_ = AttributeType::Struct;
						header.nestedTypeName_ = String("AnimationCondition");
						header.array_.size_ = arr.size();
						header.array_.add_ = [&obj]() { obj.conditions_.push_back({}); };
						header.array_.remove_ = [&obj](Size idx) { if (idx < obj.conditions_.size()) obj.conditions_.erase(obj.conditions_.begin() + idx); };
						header.editorVisible_ = false;
						outInfo.push_back(std::move(header));
						for (Size i = 0; i < arr.size(); ++i)
						{
							FieldInfo elementInfo;
							elementInfo.offset_ = 0;
							elementInfo.type_ = AttributeType::Struct;
							elementInfo.directPtr_ = &arr[i];
							elementInfo.nestedTypeName_ = String("AnimationCondition");
							elementInfo.editorVisible_ = false;
							outInfo.push_back(std::move(elementInfo));
						}
					}
				});
			}
		};
		static Register_AnimationTransition global_AnimationTransition_register;

		struct Register_Animator
		{
			Register_Animator()
			{
				ReflectionRegistry::Register(String("Animator"), [](void* ptr, DynamicArray<FieldInfo>& outInfo) {
					Animator& obj = *static_cast<Animator*>(ptr);
					outInfo.push_back({ String("アニメーション速度"), offsetof(Animator, animationSpeed_), AttributeType::Float });
					{
						auto& arr = obj.parameters_;
						FieldInfo header;
						header.name_ = String("parameters_");
						header.offset_ = 0;
						header.type_ = AttributeType::Struct;
						header.nestedTypeName_ = String("AnimationParameter");
						header.array_.size_ = arr.size();
						header.array_.add_ = [&obj]() { obj.parameters_.push_back({}); };
						header.array_.remove_ = [&obj](Size idx) { if (idx < obj.parameters_.size()) obj.parameters_.erase(obj.parameters_.begin() + idx); };
						header.editorVisible_ = false;
						outInfo.push_back(std::move(header));
						for (Size i = 0; i < arr.size(); ++i)
						{
							FieldInfo elementInfo;
							elementInfo.offset_ = 0;
							elementInfo.type_ = AttributeType::Struct;
							elementInfo.directPtr_ = &arr[i];
							elementInfo.nestedTypeName_ = String("AnimationParameter");
							elementInfo.editorVisible_ = false;
							outInfo.push_back(std::move(elementInfo));
						}
					}
					{
						auto& arr = obj.states_;
						FieldInfo header;
						header.name_ = String("states_");
						header.offset_ = 0;
						header.type_ = AttributeType::Struct;
						header.nestedTypeName_ = String("AnimationState");
						header.array_.size_ = arr.size();
						header.array_.add_ = [&obj]() { obj.states_.push_back({}); };
						header.array_.remove_ = [&obj](Size idx) { if (idx < obj.states_.size()) obj.states_.erase(obj.states_.begin() + idx); };
						header.editorVisible_ = false;
						outInfo.push_back(std::move(header));
						for (Size i = 0; i < arr.size(); ++i)
						{
							FieldInfo elementInfo;
							elementInfo.offset_ = 0;
							elementInfo.type_ = AttributeType::Struct;
							elementInfo.directPtr_ = &arr[i];
							elementInfo.nestedTypeName_ = String("AnimationState");
							elementInfo.editorVisible_ = false;
							outInfo.push_back(std::move(elementInfo));
						}
					}
					{
						auto& arr = obj.transitions_;
						FieldInfo header;
						header.name_ = String("transitions_");
						header.offset_ = 0;
						header.type_ = AttributeType::Struct;
						header.nestedTypeName_ = String("AnimationTransition");
						header.array_.size_ = arr.size();
						header.array_.add_ = [&obj]() { obj.transitions_.push_back({}); };
						header.array_.remove_ = [&obj](Size idx) { if (idx < obj.transitions_.size()) obj.transitions_.erase(obj.transitions_.begin() + idx); };
						header.editorVisible_ = false;
						outInfo.push_back(std::move(header));
						for (Size i = 0; i < arr.size(); ++i)
						{
							FieldInfo elementInfo;
							elementInfo.offset_ = 0;
							elementInfo.type_ = AttributeType::Struct;
							elementInfo.directPtr_ = &arr[i];
							elementInfo.nestedTypeName_ = String("AnimationTransition");
							elementInfo.editorVisible_ = false;
							outInfo.push_back(std::move(elementInfo));
						}
					}
					{
						FieldInfo fi;
						fi.name_ = String("entryStateIndex_");
						fi.offset_ = offsetof(Animator, entryStateIndex_);
						fi.type_ = AttributeType::Int;
						fi.editorVisible_ = false;
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("entryNodePositionX_");
						fi.offset_ = offsetof(Animator, entryNodePositionX_);
						fi.type_ = AttributeType::Float;
						fi.editorVisible_ = false;
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("entryNodePositionY_");
						fi.offset_ = offsetof(Animator, entryNodePositionY_);
						fi.type_ = AttributeType::Float;
						fi.editorVisible_ = false;
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("exitNodePositionX_");
						fi.offset_ = offsetof(Animator, exitNodePositionX_);
						fi.type_ = AttributeType::Float;
						fi.editorVisible_ = false;
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("exitNodePositionY_");
						fi.offset_ = offsetof(Animator, exitNodePositionY_);
						fi.type_ = AttributeType::Float;
						fi.editorVisible_ = false;
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("anyNodePositionX_");
						fi.offset_ = offsetof(Animator, anyNodePositionX_);
						fi.type_ = AttributeType::Float;
						fi.editorVisible_ = false;
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("anyNodePositionY_");
						fi.offset_ = offsetof(Animator, anyNodePositionY_);
						fi.type_ = AttributeType::Float;
						fi.editorVisible_ = false;
						outInfo.push_back(std::move(fi));
					}
				});
			}
		};
		static Register_Animator global_Animator_register;

		// ---- GraphicsEngine/Movie/Movie.h ----
		struct Register_Movie
		{
			Register_Movie()
			{
				ReflectionRegistry::Register(String("Movie"), [](void* ptr, DynamicArray<FieldInfo>& outInfo) {
					Movie& obj = *static_cast<Movie*>(ptr);
					{
						FieldInfo fi;
						fi.name_ = String("表示形式");
						fi.offset_ = offsetof(Movie, displayMode_);
						fi.type_ = AttributeType::Enum;
						fi.enum_.typeName_ = String("DisplayMode");
						outInfo.push_back(std::move(fi));
					}
					outInfo.push_back({ String("自動再生"), offsetof(Movie, autoPlay_), AttributeType::Bool });
					outInfo.push_back({ String("ループ再生"), offsetof(Movie, loop_), AttributeType::Bool });
					outInfo.push_back({ String("色"), offsetof(Movie, color_), AttributeType::Color });
					{
						FieldInfo fi;
						fi.name_ = String("サイズ");
						fi.offset_ = offsetof(Movie, size_);
						fi.type_ = AttributeType::Vector2;
						fi.enableIf_ = [](void* p) -> Bool { auto& o = *static_cast<Movie*>(p); return o.displayMode_ != Movie::DisplayMode::Fullscreen; };
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("中心点");
						fi.offset_ = offsetof(Movie, pivot_);
						fi.type_ = AttributeType::Vector2;
						fi.enableIf_ = [](void* p) -> Bool { auto& o = *static_cast<Movie*>(p); return o.displayMode_ != Movie::DisplayMode::Fullscreen; };
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("カメラ正対");
						fi.offset_ = offsetof(Movie, faceCamera_);
						fi.type_ = AttributeType::Bool;
						fi.enableIf_ = [](void* p) -> Bool { auto& o = *static_cast<Movie*>(p); return o.displayMode_ == Movie::DisplayMode::Billboard; };
						outInfo.push_back(std::move(fi));
					}
				});
			}
		};
		static Register_Movie global_Movie_register;

		// ---- GraphicsEngine/PostProcess/PostProcess.h ----
		struct Register_DepthOfFieldSettings
		{
			Register_DepthOfFieldSettings()
			{
				ReflectionRegistry::Register(String("DepthOfFieldSettings"), [](void* ptr, DynamicArray<FieldInfo>& outInfo) {
					DepthOfFieldSettings& obj = *static_cast<DepthOfFieldSettings*>(ptr);
					outInfo.push_back({ String("有効"), offsetof(DepthOfFieldSettings, enabled_), AttributeType::Bool });
					{
						FieldInfo fi;
						fi.name_ = String("焦点距離");
						fi.offset_ = offsetof(DepthOfFieldSettings, focusDistance_);
						fi.type_ = AttributeType::Float;
						fi.enableIf_ = [](void* p) -> Bool { auto& o = *static_cast<DepthOfFieldSettings*>(p); return o.enabled_; };
						fi.clampMin_ = 0.0f;
						fi.clampMax_ = 100.0f;
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("焦点範囲");
						fi.offset_ = offsetof(DepthOfFieldSettings, focusRange_);
						fi.type_ = AttributeType::Float;
						fi.enableIf_ = [](void* p) -> Bool { auto& o = *static_cast<DepthOfFieldSettings*>(p); return o.enabled_; };
						fi.clampMin_ = 0.01f;
						fi.clampMax_ = 50.0f;
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("最大ぼけ半径");
						fi.offset_ = offsetof(DepthOfFieldSettings, maxBlurRadius_);
						fi.type_ = AttributeType::Float;
						fi.enableIf_ = [](void* p) -> Bool { auto& o = *static_cast<DepthOfFieldSettings*>(p); return o.enabled_; };
						fi.clampMin_ = 0.0f;
						fi.clampMax_ = 0.05f;
						outInfo.push_back(std::move(fi));
					}
				});
			}
		};
		static Register_DepthOfFieldSettings global_DepthOfFieldSettings_register;

		struct Register_BokehSettings
		{
			Register_BokehSettings()
			{
				ReflectionRegistry::Register(String("BokehSettings"), [](void* ptr, DynamicArray<FieldInfo>& outInfo) {
					BokehSettings& obj = *static_cast<BokehSettings*>(ptr);
					outInfo.push_back({ String("有効"), offsetof(BokehSettings, enabled_), AttributeType::Bool });
					{
						FieldInfo fi;
						fi.name_ = String("しきい値");
						fi.offset_ = offsetof(BokehSettings, highlightThreshold_);
						fi.type_ = AttributeType::Float;
						fi.enableIf_ = [](void* p) -> Bool { auto& o = *static_cast<BokehSettings*>(p); return o.enabled_; };
						fi.clampMin_ = 0.0f;
						fi.clampMax_ = 20.0f;
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("強度");
						fi.offset_ = offsetof(BokehSettings, highlightIntensity_);
						fi.type_ = AttributeType::Float;
						fi.enableIf_ = [](void* p) -> Bool { auto& o = *static_cast<BokehSettings*>(p); return o.enabled_; };
						fi.clampMin_ = 0.0f;
						fi.clampMax_ = 5.0f;
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("角形の頂点数");
						fi.offset_ = offsetof(BokehSettings, bladeCount_);
						fi.type_ = AttributeType::Int;
						fi.enableIf_ = [](void* p) -> Bool { auto& o = *static_cast<BokehSettings*>(p); return o.enabled_; };
						fi.clampMin_ = 3;
						fi.clampMax_ = 8;
						outInfo.push_back(std::move(fi));
					}
				});
			}
		};
		static Register_BokehSettings global_BokehSettings_register;

		struct Register_LensDistortionSettings
		{
			Register_LensDistortionSettings()
			{
				ReflectionRegistry::Register(String("LensDistortionSettings"), [](void* ptr, DynamicArray<FieldInfo>& outInfo) {
					LensDistortionSettings& obj = *static_cast<LensDistortionSettings*>(ptr);
					outInfo.push_back({ String("有効"), offsetof(LensDistortionSettings, enabled_), AttributeType::Bool });
					{
						FieldInfo fi;
						fi.name_ = String("歪曲 k1");
						fi.offset_ = offsetof(LensDistortionSettings, k1_);
						fi.type_ = AttributeType::Float;
						fi.enableIf_ = [](void* p) -> Bool { auto& o = *static_cast<LensDistortionSettings*>(p); return o.enabled_; };
						fi.clampMin_ = -0.5f;
						fi.clampMax_ = 0.5f;
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("歪曲 k2");
						fi.offset_ = offsetof(LensDistortionSettings, k2_);
						fi.type_ = AttributeType::Float;
						fi.enableIf_ = [](void* p) -> Bool { auto& o = *static_cast<LensDistortionSettings*>(p); return o.enabled_; };
						fi.clampMin_ = -0.5f;
						fi.clampMax_ = 0.5f;
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("歪曲 k3");
						fi.offset_ = offsetof(LensDistortionSettings, k3_);
						fi.type_ = AttributeType::Float;
						fi.enableIf_ = [](void* p) -> Bool { auto& o = *static_cast<LensDistortionSettings*>(p); return o.enabled_; };
						fi.clampMin_ = -0.5f;
						fi.clampMax_ = 0.5f;
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("スケール");
						fi.offset_ = offsetof(LensDistortionSettings, scale_);
						fi.type_ = AttributeType::Float;
						fi.enableIf_ = [](void* p) -> Bool { auto& o = *static_cast<LensDistortionSettings*>(p); return o.enabled_; };
						fi.clampMin_ = 0.5f;
						fi.clampMax_ = 1.5f;
						outInfo.push_back(std::move(fi));
					}
				});
			}
		};
		static Register_LensDistortionSettings global_LensDistortionSettings_register;

		struct Register_ChromaticAberrationSettings
		{
			Register_ChromaticAberrationSettings()
			{
				ReflectionRegistry::Register(String("ChromaticAberrationSettings"), [](void* ptr, DynamicArray<FieldInfo>& outInfo) {
					ChromaticAberrationSettings& obj = *static_cast<ChromaticAberrationSettings*>(ptr);
					outInfo.push_back({ String("有効"), offsetof(ChromaticAberrationSettings, enabled_), AttributeType::Bool });
					{
						FieldInfo fi;
						fi.name_ = String("強度");
						fi.offset_ = offsetof(ChromaticAberrationSettings, intensity_);
						fi.type_ = AttributeType::Float;
						fi.enableIf_ = [](void* p) -> Bool { auto& o = *static_cast<ChromaticAberrationSettings*>(p); return o.enabled_; };
						fi.clampMin_ = 0.0f;
						fi.clampMax_ = 0.05f;
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("サンプル数");
						fi.offset_ = offsetof(ChromaticAberrationSettings, sampleCount_);
						fi.type_ = AttributeType::Int;
						fi.enableIf_ = [](void* p) -> Bool { auto& o = *static_cast<ChromaticAberrationSettings*>(p); return o.enabled_; };
						fi.clampMin_ = 3;
						fi.clampMax_ = 16;
						outInfo.push_back(std::move(fi));
					}
				});
			}
		};
		static Register_ChromaticAberrationSettings global_ChromaticAberrationSettings_register;

		struct Register_VignetteSettings
		{
			Register_VignetteSettings()
			{
				ReflectionRegistry::Register(String("VignetteSettings"), [](void* ptr, DynamicArray<FieldInfo>& outInfo) {
					VignetteSettings& obj = *static_cast<VignetteSettings*>(ptr);
					outInfo.push_back({ String("有効"), offsetof(VignetteSettings, enabled_), AttributeType::Bool });
					{
						FieldInfo fi;
						fi.name_ = String("強度");
						fi.offset_ = offsetof(VignetteSettings, intensity_);
						fi.type_ = AttributeType::Float;
						fi.enableIf_ = [](void* p) -> Bool { auto& o = *static_cast<VignetteSettings*>(p); return o.enabled_; };
						fi.clampMin_ = 0.0f;
						fi.clampMax_ = 8.0f;
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("減光指数");
						fi.offset_ = offsetof(VignetteSettings, exponent_);
						fi.type_ = AttributeType::Float;
						fi.enableIf_ = [](void* p) -> Bool { auto& o = *static_cast<VignetteSettings*>(p); return o.enabled_; };
						fi.clampMin_ = 1.0f;
						fi.clampMax_ = 8.0f;
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("色");
						fi.offset_ = offsetof(VignetteSettings, color_);
						fi.type_ = AttributeType::Color;
						fi.enableIf_ = [](void* p) -> Bool { auto& o = *static_cast<VignetteSettings*>(p); return o.enabled_; };
						outInfo.push_back(std::move(fi));
					}
				});
			}
		};
		static Register_VignetteSettings global_VignetteSettings_register;

		struct Register_BloomSettings
		{
			Register_BloomSettings()
			{
				ReflectionRegistry::Register(String("BloomSettings"), [](void* ptr, DynamicArray<FieldInfo>& outInfo) {
					BloomSettings& obj = *static_cast<BloomSettings*>(ptr);
					outInfo.push_back({ String("有効"), offsetof(BloomSettings, enabled_), AttributeType::Bool });
					{
						FieldInfo fi;
						fi.name_ = String("しきい値");
						fi.offset_ = offsetof(BloomSettings, threshold_);
						fi.type_ = AttributeType::Float;
						fi.enableIf_ = [](void* p) -> Bool { auto& o = *static_cast<BloomSettings*>(p); return o.enabled_; };
						fi.clampMin_ = 0.0f;
						fi.clampMax_ = 10.0f;
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("ソフトニー");
						fi.offset_ = offsetof(BloomSettings, softKnee_);
						fi.type_ = AttributeType::Float;
						fi.enableIf_ = [](void* p) -> Bool { auto& o = *static_cast<BloomSettings*>(p); return o.enabled_; };
						fi.clampMin_ = 0.0f;
						fi.clampMax_ = 1.0f;
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("強度");
						fi.offset_ = offsetof(BloomSettings, intensity_);
						fi.type_ = AttributeType::Float;
						fi.enableIf_ = [](void* p) -> Bool { auto& o = *static_cast<BloomSettings*>(p); return o.enabled_; };
						fi.clampMin_ = 0.0f;
						fi.clampMax_ = 1.0f;
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("フィルタ半径");
						fi.offset_ = offsetof(BloomSettings, filterRadius_);
						fi.type_ = AttributeType::Float;
						fi.enableIf_ = [](void* p) -> Bool { auto& o = *static_cast<BloomSettings*>(p); return o.enabled_; };
						fi.clampMin_ = 0.001f;
						fi.clampMax_ = 0.02f;
						outInfo.push_back(std::move(fi));
					}
				});
			}
		};
		static Register_BloomSettings global_BloomSettings_register;

		struct Register_AnamorphicFlareSettings
		{
			Register_AnamorphicFlareSettings()
			{
				ReflectionRegistry::Register(String("AnamorphicFlareSettings"), [](void* ptr, DynamicArray<FieldInfo>& outInfo) {
					AnamorphicFlareSettings& obj = *static_cast<AnamorphicFlareSettings*>(ptr);
					outInfo.push_back({ String("有効"), offsetof(AnamorphicFlareSettings, enabled_), AttributeType::Bool });
					{
						FieldInfo fi;
						fi.name_ = String("しきい値");
						fi.offset_ = offsetof(AnamorphicFlareSettings, threshold_);
						fi.type_ = AttributeType::Float;
						fi.enableIf_ = [](void* p) -> Bool { auto& o = *static_cast<AnamorphicFlareSettings*>(p); return o.enabled_; };
						fi.clampMin_ = 0.0f;
						fi.clampMax_ = 20.0f;
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("強度");
						fi.offset_ = offsetof(AnamorphicFlareSettings, intensity_);
						fi.type_ = AttributeType::Float;
						fi.enableIf_ = [](void* p) -> Bool { auto& o = *static_cast<AnamorphicFlareSettings*>(p); return o.enabled_; };
						fi.clampMin_ = 0.0f;
						fi.clampMax_ = 5.0f;
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("筋の長さ");
						fi.offset_ = offsetof(AnamorphicFlareSettings, streakLength_);
						fi.type_ = AttributeType::Float;
						fi.enableIf_ = [](void* p) -> Bool { auto& o = *static_cast<AnamorphicFlareSettings*>(p); return o.enabled_; };
						fi.clampMin_ = 0.0f;
						fi.clampMax_ = 1.0f;
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("減衰");
						fi.offset_ = offsetof(AnamorphicFlareSettings, attenuation_);
						fi.type_ = AttributeType::Float;
						fi.enableIf_ = [](void* p) -> Bool { auto& o = *static_cast<AnamorphicFlareSettings*>(p); return o.enabled_; };
						fi.clampMin_ = 0.80f;
						fi.clampMax_ = 0.99f;
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("色");
						fi.offset_ = offsetof(AnamorphicFlareSettings, tint_);
						fi.type_ = AttributeType::Color;
						fi.enableIf_ = [](void* p) -> Bool { auto& o = *static_cast<AnamorphicFlareSettings*>(p); return o.enabled_; };
						outInfo.push_back(std::move(fi));
					}
				});
			}
		};
		static Register_AnamorphicFlareSettings global_AnamorphicFlareSettings_register;

		struct Register_LensFlareSettings
		{
			Register_LensFlareSettings()
			{
				ReflectionRegistry::Register(String("LensFlareSettings"), [](void* ptr, DynamicArray<FieldInfo>& outInfo) {
					LensFlareSettings& obj = *static_cast<LensFlareSettings*>(ptr);
					outInfo.push_back({ String("有効"), offsetof(LensFlareSettings, enabled_), AttributeType::Bool });
					{
						FieldInfo fi;
						fi.name_ = String("しきい値");
						fi.offset_ = offsetof(LensFlareSettings, threshold_);
						fi.type_ = AttributeType::Float;
						fi.enableIf_ = [](void* p) -> Bool { auto& o = *static_cast<LensFlareSettings*>(p); return o.enabled_; };
						fi.clampMin_ = 0.0f;
						fi.clampMax_ = 20.0f;
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("全体強度");
						fi.offset_ = offsetof(LensFlareSettings, intensity_);
						fi.type_ = AttributeType::Float;
						fi.enableIf_ = [](void* p) -> Bool { auto& o = *static_cast<LensFlareSettings*>(p); return o.enabled_; };
						fi.clampMin_ = 0.0f;
						fi.clampMax_ = 5.0f;
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("スパイク長");
						fi.offset_ = offsetof(LensFlareSettings, streakLength_);
						fi.type_ = AttributeType::Float;
						fi.enableIf_ = [](void* p) -> Bool { auto& o = *static_cast<LensFlareSettings*>(p); return o.enabled_; };
						fi.clampMin_ = 0.0f;
						fi.clampMax_ = 1.0f;
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("減衰");
						fi.offset_ = offsetof(LensFlareSettings, streakAttenuation_);
						fi.type_ = AttributeType::Float;
						fi.enableIf_ = [](void* p) -> Bool { auto& o = *static_cast<LensFlareSettings*>(p); return o.enabled_; };
						fi.clampMin_ = 0.80f;
						fi.clampMax_ = 0.99f;
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("色収差");
						fi.offset_ = offsetof(LensFlareSettings, chromaticAberration_);
						fi.type_ = AttributeType::Float;
						fi.enableIf_ = [](void* p) -> Bool { auto& o = *static_cast<LensFlareSettings*>(p); return o.enabled_; };
						fi.clampMin_ = 0.0f;
						fi.clampMax_ = 0.02f;
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("回転");
						fi.offset_ = offsetof(LensFlareSettings, angleOffset_);
						fi.type_ = AttributeType::Float;
						fi.enableIf_ = [](void* p) -> Bool { auto& o = *static_cast<LensFlareSettings*>(p); return o.enabled_; };
						fi.clampMin_ = -3.14159265f;
						fi.clampMax_ = 3.14159265f;
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("ゴースト数");
						fi.offset_ = offsetof(LensFlareSettings, ghostCount_);
						fi.type_ = AttributeType::Int;
						fi.enableIf_ = [](void* p) -> Bool { auto& o = *static_cast<LensFlareSettings*>(p); return o.enabled_; };
						fi.clampMin_ = 1;
						fi.clampMax_ = 8;
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("ゴースト間隔");
						fi.offset_ = offsetof(LensFlareSettings, ghostDispersal_);
						fi.type_ = AttributeType::Float;
						fi.enableIf_ = [](void* p) -> Bool { auto& o = *static_cast<LensFlareSettings*>(p); return o.enabled_; };
						fi.clampMin_ = 0.0f;
						fi.clampMax_ = 1.0f;
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("ゴースト強度");
						fi.offset_ = offsetof(LensFlareSettings, ghostIntensity_);
						fi.type_ = AttributeType::Float;
						fi.enableIf_ = [](void* p) -> Bool { auto& o = *static_cast<LensFlareSettings*>(p); return o.enabled_; };
						fi.clampMin_ = 0.0f;
						fi.clampMax_ = 2.0f;
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("ハロー半径");
						fi.offset_ = offsetof(LensFlareSettings, haloWidth_);
						fi.type_ = AttributeType::Float;
						fi.enableIf_ = [](void* p) -> Bool { auto& o = *static_cast<LensFlareSettings*>(p); return o.enabled_; };
						fi.clampMin_ = 0.0f;
						fi.clampMax_ = 1.0f;
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("棘のばらつき");
						fi.offset_ = offsetof(LensFlareSettings, spikeVariation_);
						fi.type_ = AttributeType::Float;
						fi.enableIf_ = [](void* p) -> Bool { auto& o = *static_cast<LensFlareSettings*>(p); return o.enabled_; };
						fi.clampMin_ = 0.0f;
						fi.clampMax_ = 1.0f;
						outInfo.push_back(std::move(fi));
					}
				});
			}
		};
		static Register_LensFlareSettings global_LensFlareSettings_register;

		struct Register_ExposureSettings
		{
			Register_ExposureSettings()
			{
				ReflectionRegistry::Register(String("ExposureSettings"), [](void* ptr, DynamicArray<FieldInfo>& outInfo) {
					ExposureSettings& obj = *static_cast<ExposureSettings*>(ptr);
					{
						FieldInfo fi;
						fi.name_ = String("露出補正(EV)");
						fi.offset_ = offsetof(ExposureSettings, compensation_);
						fi.type_ = AttributeType::Float;
						fi.clampMin_ = -8.0f;
						fi.clampMax_ = 8.0f;
						outInfo.push_back(std::move(fi));
					}
					outInfo.push_back({ String("自動露出(ヒストグラム)を使う"), offsetof(ExposureSettings, enabled_), AttributeType::Bool });
					{
						FieldInfo fi;
						fi.name_ = String("最小log輝度");
						fi.offset_ = offsetof(ExposureSettings, minLogLuminance_);
						fi.type_ = AttributeType::Float;
						fi.enableIf_ = [](void* p) -> Bool { auto& o = *static_cast<ExposureSettings*>(p); return o.enabled_; };
						fi.clampMin_ = -16.0f;
						fi.clampMax_ = 0.0f;
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("最大log輝度");
						fi.offset_ = offsetof(ExposureSettings, maxLogLuminance_);
						fi.type_ = AttributeType::Float;
						fi.enableIf_ = [](void* p) -> Bool { auto& o = *static_cast<ExposureSettings*>(p); return o.enabled_; };
						fi.clampMin_ = 0.0f;
						fi.clampMax_ = 16.0f;
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("目標輝度(キー値)");
						fi.offset_ = offsetof(ExposureSettings, keyValue_);
						fi.type_ = AttributeType::Float;
						fi.enableIf_ = [](void* p) -> Bool { auto& o = *static_cast<ExposureSettings*>(p); return o.enabled_; };
						fi.clampMin_ = 0.001f;
						fi.clampMax_ = 1.0f;
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("明順応速度");
						fi.offset_ = offsetof(ExposureSettings, adaptSpeedToBright_);
						fi.type_ = AttributeType::Float;
						fi.enableIf_ = [](void* p) -> Bool { auto& o = *static_cast<ExposureSettings*>(p); return o.enabled_; };
						fi.clampMin_ = 0.01f;
						fi.clampMax_ = 20.0f;
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("暗順応速度");
						fi.offset_ = offsetof(ExposureSettings, adaptSpeedToDark_);
						fi.type_ = AttributeType::Float;
						fi.enableIf_ = [](void* p) -> Bool { auto& o = *static_cast<ExposureSettings*>(p); return o.enabled_; };
						fi.clampMin_ = 0.01f;
						fi.clampMax_ = 20.0f;
						outInfo.push_back(std::move(fi));
					}
				});
			}
		};
		static Register_ExposureSettings global_ExposureSettings_register;

		struct Register_ColorGradingRangeSettings
		{
			Register_ColorGradingRangeSettings()
			{
				ReflectionRegistry::Register(String("ColorGradingRangeSettings"), [](void* ptr, DynamicArray<FieldInfo>& outInfo) {
					ColorGradingRangeSettings& obj = *static_cast<ColorGradingRangeSettings*>(ptr);
					{
						FieldInfo fi;
						fi.name_ = String("色温度");
						fi.offset_ = offsetof(ColorGradingRangeSettings, temperature_);
						fi.type_ = AttributeType::Float;
						fi.clampMin_ = -1.0f;
						fi.clampMax_ = 1.0f;
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("彩度");
						fi.offset_ = offsetof(ColorGradingRangeSettings, saturation_);
						fi.type_ = AttributeType::Float;
						fi.clampMin_ = 0.0f;
						fi.clampMax_ = 4.0f;
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("コントラスト");
						fi.offset_ = offsetof(ColorGradingRangeSettings, contrast_);
						fi.type_ = AttributeType::Float;
						fi.clampMin_ = 0.1f;
						fi.clampMax_ = 4.0f;
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("ガンマ");
						fi.offset_ = offsetof(ColorGradingRangeSettings, gamma_);
						fi.type_ = AttributeType::Float;
						fi.clampMin_ = 0.1f;
						fi.clampMax_ = 4.0f;
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("ゲイン");
						fi.offset_ = offsetof(ColorGradingRangeSettings, gain_);
						fi.type_ = AttributeType::Float;
						fi.clampMin_ = 0.0f;
						fi.clampMax_ = 4.0f;
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("オフセット");
						fi.offset_ = offsetof(ColorGradingRangeSettings, offset_);
						fi.type_ = AttributeType::Float;
						fi.clampMin_ = -1.0f;
						fi.clampMax_ = 1.0f;
						outInfo.push_back(std::move(fi));
					}
				});
			}
		};
		static Register_ColorGradingRangeSettings global_ColorGradingRangeSettings_register;

		struct Register_ColorGradingSettings
		{
			Register_ColorGradingSettings()
			{
				ReflectionRegistry::Register(String("ColorGradingSettings"), [](void* ptr, DynamicArray<FieldInfo>& outInfo) {
					ColorGradingSettings& obj = *static_cast<ColorGradingSettings*>(ptr);
					outInfo.push_back({ String("有効"), offsetof(ColorGradingSettings, enabled_), AttributeType::Bool });
					{
						FieldInfo fi;
						fi.name_ = String("全体");
						fi.offset_ = offsetof(ColorGradingSettings, global_);
						fi.type_ = AttributeType::Struct;
						fi.nestedTypeName_ = String("ColorGradingRangeSettings");
						fi.enableIf_ = [](void* p) -> Bool { auto& o = *static_cast<ColorGradingSettings*>(p); return o.enabled_; };
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("シャドウ");
						fi.offset_ = offsetof(ColorGradingSettings, shadows_);
						fi.type_ = AttributeType::Struct;
						fi.nestedTypeName_ = String("ColorGradingRangeSettings");
						fi.enableIf_ = [](void* p) -> Bool { auto& o = *static_cast<ColorGradingSettings*>(p); return o.enabled_; };
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("中間調");
						fi.offset_ = offsetof(ColorGradingSettings, midtones_);
						fi.type_ = AttributeType::Struct;
						fi.nestedTypeName_ = String("ColorGradingRangeSettings");
						fi.enableIf_ = [](void* p) -> Bool { auto& o = *static_cast<ColorGradingSettings*>(p); return o.enabled_; };
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("ハイライト");
						fi.offset_ = offsetof(ColorGradingSettings, highlights_);
						fi.type_ = AttributeType::Struct;
						fi.nestedTypeName_ = String("ColorGradingRangeSettings");
						fi.enableIf_ = [](void* p) -> Bool { auto& o = *static_cast<ColorGradingSettings*>(p); return o.enabled_; };
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("シャドウ上限");
						fi.offset_ = offsetof(ColorGradingSettings, shadowsMax_);
						fi.type_ = AttributeType::Float;
						fi.enableIf_ = [](void* p) -> Bool { auto& o = *static_cast<ColorGradingSettings*>(p); return o.enabled_; };
						fi.clampMin_ = 0.0f;
						fi.clampMax_ = 1.0f;
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("ハイライト下限");
						fi.offset_ = offsetof(ColorGradingSettings, highlightsMin_);
						fi.type_ = AttributeType::Float;
						fi.enableIf_ = [](void* p) -> Bool { auto& o = *static_cast<ColorGradingSettings*>(p); return o.enabled_; };
						fi.clampMin_ = 0.0f;
						fi.clampMax_ = 2.0f;
						outInfo.push_back(std::move(fi));
					}
				});
			}
		};
		static Register_ColorGradingSettings global_ColorGradingSettings_register;

		struct Register_ToneMappingSettings
		{
			Register_ToneMappingSettings()
			{
				ReflectionRegistry::Register(String("ToneMappingSettings"), [](void* ptr, DynamicArray<FieldInfo>& outInfo) {
					ToneMappingSettings& obj = *static_cast<ToneMappingSettings*>(ptr);
					outInfo.push_back({ String("有効"), offsetof(ToneMappingSettings, enabled_), AttributeType::Bool });
					{
						FieldInfo fi;
						fi.name_ = String("方式");
						fi.offset_ = offsetof(ToneMappingSettings, mode_);
						fi.type_ = AttributeType::Enum;
						fi.enum_.typeName_ = String("ToneCurve");
						fi.enableIf_ = [](void* p) -> Bool { auto& o = *static_cast<ToneMappingSettings*>(p); return o.enabled_; };
						outInfo.push_back(std::move(fi));
					}
				});
			}
		};
		static Register_ToneMappingSettings global_ToneMappingSettings_register;

		struct Register_SharpnessSettings
		{
			Register_SharpnessSettings()
			{
				ReflectionRegistry::Register(String("SharpnessSettings"), [](void* ptr, DynamicArray<FieldInfo>& outInfo) {
					SharpnessSettings& obj = *static_cast<SharpnessSettings*>(ptr);
					outInfo.push_back({ String("有効"), offsetof(SharpnessSettings, enabled_), AttributeType::Bool });
					{
						FieldInfo fi;
						fi.name_ = String("強さ");
						fi.offset_ = offsetof(SharpnessSettings, amount_);
						fi.type_ = AttributeType::Float;
						fi.enableIf_ = [](void* p) -> Bool { auto& o = *static_cast<SharpnessSettings*>(p); return o.enabled_; };
						fi.clampMin_ = 0.0f;
						fi.clampMax_ = 2.0f;
						outInfo.push_back(std::move(fi));
					}
				});
			}
		};
		static Register_SharpnessSettings global_SharpnessSettings_register;

		struct Register_FilmGrainSettings
		{
			Register_FilmGrainSettings()
			{
				ReflectionRegistry::Register(String("FilmGrainSettings"), [](void* ptr, DynamicArray<FieldInfo>& outInfo) {
					FilmGrainSettings& obj = *static_cast<FilmGrainSettings*>(ptr);
					outInfo.push_back({ String("有効"), offsetof(FilmGrainSettings, enabled_), AttributeType::Bool });
					{
						FieldInfo fi;
						fi.name_ = String("強度");
						fi.offset_ = offsetof(FilmGrainSettings, intensity_);
						fi.type_ = AttributeType::Float;
						fi.enableIf_ = [](void* p) -> Bool { auto& o = *static_cast<FilmGrainSettings*>(p); return o.enabled_; };
						fi.clampMin_ = 0.0f;
						fi.clampMax_ = 1.0f;
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("粒の大きさ");
						fi.offset_ = offsetof(FilmGrainSettings, size_);
						fi.type_ = AttributeType::Float;
						fi.enableIf_ = [](void* p) -> Bool { auto& o = *static_cast<FilmGrainSettings*>(p); return o.enabled_; };
						fi.clampMin_ = 1.0f;
						fi.clampMax_ = 8.0f;
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("階調応答");
						fi.offset_ = offsetof(FilmGrainSettings, luminanceResponse_);
						fi.type_ = AttributeType::Float;
						fi.enableIf_ = [](void* p) -> Bool { auto& o = *static_cast<FilmGrainSettings*>(p); return o.enabled_; };
						fi.clampMin_ = 0.0f;
						fi.clampMax_ = 1.0f;
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("色付き");
						fi.offset_ = offsetof(FilmGrainSettings, colored_);
						fi.type_ = AttributeType::Bool;
						fi.enableIf_ = [](void* p) -> Bool { auto& o = *static_cast<FilmGrainSettings*>(p); return o.enabled_; };
						outInfo.push_back(std::move(fi));
					}
				});
			}
		};
		static Register_FilmGrainSettings global_FilmGrainSettings_register;

		struct Register_PostProcess
		{
			Register_PostProcess()
			{
				ReflectionRegistry::Register(String("PostProcess"), [](void* ptr, DynamicArray<FieldInfo>& outInfo) {
					PostProcess& obj = *static_cast<PostProcess*>(ptr);
					{
						FieldInfo fi;
						fi.name_ = String("被写界深度");
						fi.offset_ = offsetof(PostProcess, depthOfField_);
						fi.type_ = AttributeType::Struct;
						fi.nestedTypeName_ = String("DepthOfFieldSettings");
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("ボケ");
						fi.offset_ = offsetof(PostProcess, bokeh_);
						fi.type_ = AttributeType::Struct;
						fi.nestedTypeName_ = String("BokehSettings");
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("レンズ歪曲");
						fi.offset_ = offsetof(PostProcess, lensDistortion_);
						fi.type_ = AttributeType::Struct;
						fi.nestedTypeName_ = String("LensDistortionSettings");
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("色収差");
						fi.offset_ = offsetof(PostProcess, chromaticAberration_);
						fi.type_ = AttributeType::Struct;
						fi.nestedTypeName_ = String("ChromaticAberrationSettings");
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("ビネット");
						fi.offset_ = offsetof(PostProcess, vignette_);
						fi.type_ = AttributeType::Struct;
						fi.nestedTypeName_ = String("VignetteSettings");
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("ブルーム");
						fi.offset_ = offsetof(PostProcess, bloom_);
						fi.type_ = AttributeType::Struct;
						fi.nestedTypeName_ = String("BloomSettings");
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("アナモルフィックフレア");
						fi.offset_ = offsetof(PostProcess, anamorphicFlare_);
						fi.type_ = AttributeType::Struct;
						fi.nestedTypeName_ = String("AnamorphicFlareSettings");
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("レンズフレア");
						fi.offset_ = offsetof(PostProcess, lensFlare_);
						fi.type_ = AttributeType::Struct;
						fi.nestedTypeName_ = String("LensFlareSettings");
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("露出");
						fi.offset_ = offsetof(PostProcess, exposure_);
						fi.type_ = AttributeType::Struct;
						fi.nestedTypeName_ = String("ExposureSettings");
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("カラーグレーディング");
						fi.offset_ = offsetof(PostProcess, colorGrading_);
						fi.type_ = AttributeType::Struct;
						fi.nestedTypeName_ = String("ColorGradingSettings");
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("トーンマップ");
						fi.offset_ = offsetof(PostProcess, toneMapping_);
						fi.type_ = AttributeType::Struct;
						fi.nestedTypeName_ = String("ToneMappingSettings");
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("シャープネス");
						fi.offset_ = offsetof(PostProcess, sharpness_);
						fi.type_ = AttributeType::Struct;
						fi.nestedTypeName_ = String("SharpnessSettings");
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("フィルムグレイン");
						fi.offset_ = offsetof(PostProcess, filmGrain_);
						fi.type_ = AttributeType::Struct;
						fi.nestedTypeName_ = String("FilmGrainSettings");
						outInfo.push_back(std::move(fi));
					}
				});
			}
		};
		static Register_PostProcess global_PostProcess_register;

		// ---- GraphicsEngine/Texture/Image.h ----
		struct Register_Image
		{
			Register_Image()
			{
				ReflectionRegistry::Register(String("Image"), [](void* ptr, DynamicArray<FieldInfo>& outInfo) {
					Image& obj = *static_cast<Image*>(ptr);
					outInfo.push_back({ String("テクスチャサイズ"), offsetof(Image, textureSize_), AttributeType::Vector2 });
					outInfo.push_back({ String("テクスチャ位置"), offsetof(Image, texturePosition_), AttributeType::Vector2 });
					outInfo.push_back({ String("中心点"), offsetof(Image, pivot_), AttributeType::Vector2 });
					outInfo.push_back({ String("色"), offsetof(Image, color_), AttributeType::Color });
					{
						FieldInfo fi;
						fi.name_ = String("表示形式");
						fi.offset_ = offsetof(Image, viewType_);
						fi.type_ = AttributeType::Enum;
						fi.enum_.typeName_ = String("ViewType");
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("カメラ正対");
						fi.offset_ = offsetof(Image, faceCamera_);
						fi.type_ = AttributeType::Bool;
						fi.enableIf_ = [](void* p) -> Bool { auto& o = *static_cast<Image*>(p); return o.viewType_ == Image::ViewType::Billboard; };
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("テクスチャタイプ");
						fi.offset_ = offsetof(Image, motionType_);
						fi.type_ = AttributeType::Enum;
						fi.enum_.typeName_ = String("MotionType");
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("スクロール速度");
						fi.offset_ = offsetof(Image, scrollSpeed_);
						fi.type_ = AttributeType::Float;
						fi.enableIf_ = [](void* p) -> Bool { auto& o = *static_cast<Image*>(p); return o.motionType_ == Image::MotionType::Dynamic; };
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("スクロール向き");
						fi.offset_ = offsetof(Image, scrollDirection_);
						fi.type_ = AttributeType::Vector2;
						fi.enableIf_ = [](void* p) -> Bool { auto& o = *static_cast<Image*>(p); return o.motionType_ == Image::MotionType::Dynamic; };
						fi.clampMin_ = -1.0f;
						fi.clampMax_ = 1.0f;
						outInfo.push_back(std::move(fi));
					}
				});
			}
		};
		static Register_Image global_Image_register;

		// ---- PhysicsEngine/CharacterController/CharacterController.h ----
		struct Register_CharacterController
		{
			Register_CharacterController()
			{
				ReflectionRegistry::Register(String("CharacterController"), [](void* ptr, DynamicArray<FieldInfo>& outInfo) {
					CharacterController& obj = *static_cast<CharacterController*>(ptr);
					{
						FieldInfo fi;
						fi.name_ = String("半径");
						fi.offset_ = offsetof(CharacterController, radius_);
						fi.type_ = AttributeType::Float;
						fi.clampMin_ = 0.001f;
						fi.clampMax_ = 100.0f;
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("高さ");
						fi.offset_ = offsetof(CharacterController, height_);
						fi.type_ = AttributeType::Float;
						fi.clampMin_ = 0.001f;
						fi.clampMax_ = 100.0f;
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("質量");
						fi.offset_ = offsetof(CharacterController, mass_);
						fi.type_ = AttributeType::Float;
						fi.clampMin_ = 0.001f;
						fi.clampMax_ = 1000.0f;
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("押し出し力");
						fi.offset_ = offsetof(CharacterController, pushForce_);
						fi.type_ = AttributeType::Float;
						fi.clampMin_ = 0.0f;
						fi.clampMax_ = 10000.0f;
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("最大移動速度");
						fi.offset_ = offsetof(CharacterController, maxMoveSpeed_);
						fi.type_ = AttributeType::Float;
						fi.clampMin_ = 0.0f;
						fi.clampMax_ = 100.0f;
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("加速度");
						fi.offset_ = offsetof(CharacterController, acceleration_);
						fi.type_ = AttributeType::Float;
						fi.clampMin_ = 0.0f;
						fi.clampMax_ = 1000.0f;
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("減速度");
						fi.offset_ = offsetof(CharacterController, deceleration_);
						fi.type_ = AttributeType::Float;
						fi.clampMin_ = 0.0f;
						fi.clampMax_ = 1000.0f;
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("回転速度");
						fi.offset_ = offsetof(CharacterController, turnSpeed_);
						fi.type_ = AttributeType::Float;
						fi.clampMin_ = 0.0f;
						fi.clampMax_ = 1000.0f;
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("空気抵抗");
						fi.offset_ = offsetof(CharacterController, airDrag_);
						fi.type_ = AttributeType::Float;
						fi.clampMin_ = 0.0f;
						fi.clampMax_ = 10.0f;
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("最大斜面角度");
						fi.offset_ = offsetof(CharacterController, maxSlopeAngle_);
						fi.type_ = AttributeType::Float;
						fi.clampMin_ = 0.0f;
						fi.clampMax_ = 89.0f;
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("最大許容段差高");
						fi.offset_ = offsetof(CharacterController, maxStepHeight_);
						fi.type_ = AttributeType::Float;
						fi.clampMin_ = 0.0f;
						fi.clampMax_ = 10.0f;
						outInfo.push_back(std::move(fi));
					}
					outInfo.push_back({ String("重力倍率"), offsetof(CharacterController, gravityScale_), AttributeType::Float });
					{
						FieldInfo fi;
						fi.name_ = String("最大落下速度");
						fi.offset_ = offsetof(CharacterController, maxFallSpeed_);
						fi.type_ = AttributeType::Float;
						fi.clampMin_ = 0.0f;
						fi.clampMax_ = 1000.0f;
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("ジャンプ力");
						fi.offset_ = offsetof(CharacterController, jumpPower_);
						fi.type_ = AttributeType::Float;
						fi.clampMin_ = 0.0f;
						fi.clampMax_ = 1000.0f;
						outInfo.push_back(std::move(fi));
					}
					outInfo.push_back({ String("しゃがみ"), offsetof(CharacterController, crouch_), AttributeType::Bool });
					{
						FieldInfo fi;
						fi.name_ = String("しゃがみ時の高さ");
						fi.offset_ = offsetof(CharacterController, crouchHeight_);
						fi.type_ = AttributeType::Float;
						fi.clampMin_ = 0.001f;
						fi.clampMax_ = 100.0f;
						outInfo.push_back(std::move(fi));
					}
				});
			}
		};
		static Register_CharacterController global_CharacterController_register;

		// ---- PhysicsEngine/Collider/BoxCollider.h ----
		struct Register_BoxCollider
		{
			Register_BoxCollider()
			{
				ReflectionRegistry::Register(String("BoxCollider"), [](void* ptr, DynamicArray<FieldInfo>& outInfo) {
					BoxCollider& obj = *static_cast<BoxCollider*>(ptr);
					outInfo.push_back({ String("サイズ"), offsetof(BoxCollider, size_), AttributeType::Vector3 });
					outInfo.push_back({ String("中心オフセット"), offsetof(BoxCollider, center_), AttributeType::Vector3 });
					outInfo.push_back({ String("トリガー"), offsetof(BoxCollider, isTrigger_), AttributeType::Bool });
				});
			}
		};
		static Register_BoxCollider global_BoxCollider_register;

		// ---- PhysicsEngine/Collider/CapsuleCollider.h ----
		struct Register_CapsuleCollider
		{
			Register_CapsuleCollider()
			{
				ReflectionRegistry::Register(String("CapsuleCollider"), [](void* ptr, DynamicArray<FieldInfo>& outInfo) {
					CapsuleCollider& obj = *static_cast<CapsuleCollider*>(ptr);
					{
						FieldInfo fi;
						fi.name_ = String("高さ");
						fi.offset_ = offsetof(CapsuleCollider, height_);
						fi.type_ = AttributeType::Float;
						fi.clampMin_ = 0.001f;
						fi.clampMax_ = 100.0f;
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("半径");
						fi.offset_ = offsetof(CapsuleCollider, radius_);
						fi.type_ = AttributeType::Float;
						fi.clampMin_ = 0.001f;
						fi.clampMax_ = 100.0f;
						outInfo.push_back(std::move(fi));
					}
					outInfo.push_back({ String("トリガー"), offsetof(CapsuleCollider, isTrigger_), AttributeType::Bool });
				});
			}
		};
		static Register_CapsuleCollider global_CapsuleCollider_register;

		// ---- PhysicsEngine/Collider/CircleCollider.h ----
		struct Register_CircleCollider
		{
			Register_CircleCollider()
			{
				ReflectionRegistry::Register(String("CircleCollider"), [](void* ptr, DynamicArray<FieldInfo>& outInfo) {
					CircleCollider& obj = *static_cast<CircleCollider*>(ptr);
					{
						FieldInfo fi;
						fi.name_ = String("半径");
						fi.offset_ = offsetof(CircleCollider, radius_);
						fi.type_ = AttributeType::Float;
						fi.clampMin_ = 0.001f;
						fi.clampMax_ = 100.0f;
						outInfo.push_back(std::move(fi));
					}
					outInfo.push_back({ String("中心オフセット"), offsetof(CircleCollider, center_), AttributeType::Vector2 });
					outInfo.push_back({ String("トリガー"), offsetof(CircleCollider, isTrigger_), AttributeType::Bool });
				});
			}
		};
		static Register_CircleCollider global_CircleCollider_register;

		// ---- PhysicsEngine/Collider/CylinderCollider.h ----
		struct Register_CylinderCollider
		{
			Register_CylinderCollider()
			{
				ReflectionRegistry::Register(String("CylinderCollider"), [](void* ptr, DynamicArray<FieldInfo>& outInfo) {
					CylinderCollider& obj = *static_cast<CylinderCollider*>(ptr);
					{
						FieldInfo fi;
						fi.name_ = String("高さ");
						fi.offset_ = offsetof(CylinderCollider, height_);
						fi.type_ = AttributeType::Float;
						fi.clampMin_ = 0.001f;
						fi.clampMax_ = 100.0f;
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("半径");
						fi.offset_ = offsetof(CylinderCollider, radius_);
						fi.type_ = AttributeType::Float;
						fi.clampMin_ = 0.001f;
						fi.clampMax_ = 100.0f;
						outInfo.push_back(std::move(fi));
					}
					outInfo.push_back({ String("トリガー"), offsetof(CylinderCollider, isTrigger_), AttributeType::Bool });
				});
			}
		};
		static Register_CylinderCollider global_CylinderCollider_register;

		// ---- PhysicsEngine/Collider/MeshCollider.h ----
		struct Register_MeshCollider
		{
			Register_MeshCollider()
			{
				ReflectionRegistry::Register(String("MeshCollider"), [](void* ptr, DynamicArray<FieldInfo>& outInfo) {
					MeshCollider& obj = *static_cast<MeshCollider*>(ptr);
					outInfo.push_back({ String("凸包にする"), offsetof(MeshCollider, convex_), AttributeType::Bool });
					outInfo.push_back({ String("トリガー"), offsetof(MeshCollider, isTrigger_), AttributeType::Bool });
				});
			}
		};
		static Register_MeshCollider global_MeshCollider_register;

		// ---- PhysicsEngine/Collider/RectCollider.h ----
		struct Register_RectCollider
		{
			Register_RectCollider()
			{
				ReflectionRegistry::Register(String("RectCollider"), [](void* ptr, DynamicArray<FieldInfo>& outInfo) {
					RectCollider& obj = *static_cast<RectCollider*>(ptr);
					outInfo.push_back({ String("サイズ"), offsetof(RectCollider, size_), AttributeType::Vector2 });
					outInfo.push_back({ String("中心オフセット"), offsetof(RectCollider, center_), AttributeType::Vector2 });
					outInfo.push_back({ String("トリガー"), offsetof(RectCollider, isTrigger_), AttributeType::Bool });
				});
			}
		};
		static Register_RectCollider global_RectCollider_register;

		// ---- PhysicsEngine/Collider/SphereCollider.h ----
		struct Register_SphereCollider
		{
			Register_SphereCollider()
			{
				ReflectionRegistry::Register(String("SphereCollider"), [](void* ptr, DynamicArray<FieldInfo>& outInfo) {
					SphereCollider& obj = *static_cast<SphereCollider*>(ptr);
					{
						FieldInfo fi;
						fi.name_ = String("半径");
						fi.offset_ = offsetof(SphereCollider, radius_);
						fi.type_ = AttributeType::Float;
						fi.clampMin_ = 0.001f;
						fi.clampMax_ = 100.0f;
						outInfo.push_back(std::move(fi));
					}
					outInfo.push_back({ String("トリガー"), offsetof(SphereCollider, isTrigger_), AttributeType::Bool });
				});
			}
		};
		static Register_SphereCollider global_SphereCollider_register;

		// ---- PhysicsEngine/Joint/FixedJoint.h ----
		struct Register_FixedJoint
		{
			Register_FixedJoint()
			{
				ReflectionRegistry::Register(String("FixedJoint"), [](void* ptr, DynamicArray<FieldInfo>& outInfo) {
					FixedJoint& obj = *static_cast<FixedJoint*>(ptr);
					outInfo.push_back({ String("有効"), offsetof(FixedJoint, enabled_), AttributeType::Bool });
				});
			}
		};
		static Register_FixedJoint global_FixedJoint_register;

		// ---- PhysicsEngine/Joint/HingeJoint.h ----
		struct Register_HingeJoint
		{
			Register_HingeJoint()
			{
				ReflectionRegistry::Register(String("HingeJoint"), [](void* ptr, DynamicArray<FieldInfo>& outInfo) {
					HingeJoint& obj = *static_cast<HingeJoint*>(ptr);
					outInfo.push_back({ String("有効"), offsetof(HingeJoint, enabled_), AttributeType::Bool });
					outInfo.push_back({ String("アンカー(ローカル)"), offsetof(HingeJoint, anchor_), AttributeType::Vector3 });
					outInfo.push_back({ String("ヒンジ軸(ローカル)"), offsetof(HingeJoint, axis_), AttributeType::Vector3 });
					outInfo.push_back({ String("角度制限を使う"), offsetof(HingeJoint, useLimits_), AttributeType::Bool });
					{
						FieldInfo fi;
						fi.name_ = String("最小角(度)");
						fi.offset_ = offsetof(HingeJoint, minAngle_);
						fi.type_ = AttributeType::Float;
						fi.enableIf_ = [](void* p) -> Bool { auto& o = *static_cast<HingeJoint*>(p); return o.useLimits_; };
						fi.clampMin_ = -180.0f;
						fi.clampMax_ = 0.0f;
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("最大角(度)");
						fi.offset_ = offsetof(HingeJoint, maxAngle_);
						fi.type_ = AttributeType::Float;
						fi.enableIf_ = [](void* p) -> Bool { auto& o = *static_cast<HingeJoint*>(p); return o.useLimits_; };
						fi.clampMin_ = 0.0f;
						fi.clampMax_ = 180.0f;
						outInfo.push_back(std::move(fi));
					}
				});
			}
		};
		static Register_HingeJoint global_HingeJoint_register;

		// ---- PhysicsEngine/Joint/SliderJoint.h ----
		struct Register_SliderJoint
		{
			Register_SliderJoint()
			{
				ReflectionRegistry::Register(String("SliderJoint"), [](void* ptr, DynamicArray<FieldInfo>& outInfo) {
					SliderJoint& obj = *static_cast<SliderJoint*>(ptr);
					outInfo.push_back({ String("有効"), offsetof(SliderJoint, enabled_), AttributeType::Bool });
					outInfo.push_back({ String("スライド軸(ローカル)"), offsetof(SliderJoint, axis_), AttributeType::Vector3 });
					outInfo.push_back({ String("可動範囲制限を使う"), offsetof(SliderJoint, useLimits_), AttributeType::Bool });
					{
						FieldInfo fi;
						fi.name_ = String("最小距離");
						fi.offset_ = offsetof(SliderJoint, minDistance_);
						fi.type_ = AttributeType::Float;
						fi.enableIf_ = [](void* p) -> Bool { auto& o = *static_cast<SliderJoint*>(p); return o.useLimits_; };
						fi.clampMin_ = -1000.0f;
						fi.clampMax_ = 0.0f;
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("最大距離");
						fi.offset_ = offsetof(SliderJoint, maxDistance_);
						fi.type_ = AttributeType::Float;
						fi.enableIf_ = [](void* p) -> Bool { auto& o = *static_cast<SliderJoint*>(p); return o.useLimits_; };
						fi.clampMin_ = 0.0f;
						fi.clampMax_ = 1000.0f;
						outInfo.push_back(std::move(fi));
					}
				});
			}
		};
		static Register_SliderJoint global_SliderJoint_register;

		// ---- PhysicsEngine/Joint/SpringJoint.h ----
		struct Register_SpringJoint
		{
			Register_SpringJoint()
			{
				ReflectionRegistry::Register(String("SpringJoint"), [](void* ptr, DynamicArray<FieldInfo>& outInfo) {
					SpringJoint& obj = *static_cast<SpringJoint*>(ptr);
					outInfo.push_back({ String("有効"), offsetof(SpringJoint, enabled_), AttributeType::Bool });
					outInfo.push_back({ String("アンカー(ローカル)"), offsetof(SpringJoint, anchor_), AttributeType::Vector3 });
					{
						FieldInfo fi;
						fi.name_ = String("最小距離");
						fi.offset_ = offsetof(SpringJoint, minDistance_);
						fi.type_ = AttributeType::Float;
						fi.clampMin_ = 0.0f;
						fi.clampMax_ = 1000.0f;
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("最大距離");
						fi.offset_ = offsetof(SpringJoint, maxDistance_);
						fi.type_ = AttributeType::Float;
						fi.clampMin_ = 0.0f;
						fi.clampMax_ = 1000.0f;
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("剛性(Hz)");
						fi.offset_ = offsetof(SpringJoint, frequency_);
						fi.type_ = AttributeType::Float;
						fi.clampMin_ = 0.0f;
						fi.clampMax_ = 30.0f;
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("減衰");
						fi.offset_ = offsetof(SpringJoint, damping_);
						fi.type_ = AttributeType::Float;
						fi.clampMin_ = 0.0f;
						fi.clampMax_ = 1.0f;
						outInfo.push_back(std::move(fi));
					}
				});
			}
		};
		static Register_SpringJoint global_SpringJoint_register;

		// ---- PhysicsEngine/Rigidbody/Rigidbody.h ----
		struct Register_Rigidbody
		{
			Register_Rigidbody()
			{
				ReflectionRegistry::Register(String("Rigidbody"), [](void* ptr, DynamicArray<FieldInfo>& outInfo) {
					Rigidbody& obj = *static_cast<Rigidbody*>(ptr);
					{
						FieldInfo fi;
						fi.name_ = String("動作モード");
						fi.offset_ = offsetof(Rigidbody, bodyType_);
						fi.type_ = AttributeType::Enum;
						fi.enum_.typeName_ = String("BodyType");
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("連続衝突判定");
						fi.offset_ = offsetof(Rigidbody, continuousCollision_);
						fi.type_ = AttributeType::Bool;
						fi.enableIf_ = [](void* p) -> Bool { auto& o = *static_cast<Rigidbody*>(p); return o.bodyType_ == Rigidbody::BodyType::Dynamic; };
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("質量");
						fi.offset_ = offsetof(Rigidbody, mass_);
						fi.type_ = AttributeType::Float;
						fi.clampMin_ = 0.001f;
						fi.clampMax_ = 100.0f;
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("空気抵抗");
						fi.offset_ = offsetof(Rigidbody, linearDrag_);
						fi.type_ = AttributeType::Float;
						fi.clampMin_ = 0.0f;
						fi.clampMax_ = 10.0f;
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("角抵抗");
						fi.offset_ = offsetof(Rigidbody, angularDrag_);
						fi.type_ = AttributeType::Float;
						fi.clampMin_ = 0.0f;
						fi.clampMax_ = 10.0f;
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("摩擦係数");
						fi.offset_ = offsetof(Rigidbody, friction_);
						fi.type_ = AttributeType::Float;
						fi.clampMin_ = 0.0f;
						fi.clampMax_ = 1.0f;
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("反発係数");
						fi.offset_ = offsetof(Rigidbody, restitution_);
						fi.type_ = AttributeType::Float;
						fi.clampMin_ = 0.0f;
						fi.clampMax_ = 1.0f;
						outInfo.push_back(std::move(fi));
					}
					outInfo.push_back({ String("重力"), offsetof(Rigidbody, useGravity_), AttributeType::Bool });
					{
						FieldInfo fi;
						fi.name_ = String("重力倍率");
						fi.offset_ = offsetof(Rigidbody, gravityScale_);
						fi.type_ = AttributeType::Float;
						fi.enableIf_ = [](void* p) -> Bool { auto& o = *static_cast<Rigidbody*>(p); return o.useGravity_; };
						outInfo.push_back(std::move(fi));
					}
					outInfo.push_back({ String("位置X軸固定"), offsetof(Rigidbody, freezePositionX_), AttributeType::Bool });
					outInfo.push_back({ String("位置Y軸固定"), offsetof(Rigidbody, freezePositionY_), AttributeType::Bool });
					outInfo.push_back({ String("位置Z軸固定"), offsetof(Rigidbody, freezePositionZ_), AttributeType::Bool });
					outInfo.push_back({ String("回転X軸固定"), offsetof(Rigidbody, freezeRotationX_), AttributeType::Bool });
					outInfo.push_back({ String("回転Y軸固定"), offsetof(Rigidbody, freezeRotationY_), AttributeType::Bool });
					outInfo.push_back({ String("回転Z軸固定"), offsetof(Rigidbody, freezeRotationZ_), AttributeType::Bool });
					outInfo.push_back({ String("トリガー"), offsetof(Rigidbody, isTrigger_), AttributeType::Bool });
				});
			}
		};
		static Register_Rigidbody global_Rigidbody_register;

		// ---- PhysicsEngine/Softbody/Softbody.h ----
		struct Register_Softbody
		{
			Register_Softbody()
			{
				ReflectionRegistry::Register(String("Softbody"), [](void* ptr, DynamicArray<FieldInfo>& outInfo) {
					Softbody& obj = *static_cast<Softbody*>(ptr);
					{
						FieldInfo fi;
						fi.name_ = String("質量");
						fi.offset_ = offsetof(Softbody, mass_);
						fi.type_ = AttributeType::Float;
						fi.clampMin_ = 0.001f;
						fi.clampMax_ = 100.0f;
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("面積弾性力");
						fi.offset_ = offsetof(Softbody, areaStiffness_);
						fi.type_ = AttributeType::Float;
						fi.clampMin_ = 0.0f;
						fi.clampMax_ = 1.0f;
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("体積弾性力");
						fi.offset_ = offsetof(Softbody, volumeStiffness_);
						fi.type_ = AttributeType::Float;
						fi.clampMin_ = 0.0f;
						fi.clampMax_ = 1.0f;
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("抵抗力");
						fi.offset_ = offsetof(Softbody, damping_);
						fi.type_ = AttributeType::Float;
						fi.clampMin_ = 0.0f;
						fi.clampMax_ = 1.0f;
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("ポアソン比");
						fi.offset_ = offsetof(Softbody, poissonRatio_);
						fi.type_ = AttributeType::Float;
						fi.clampMin_ = 0.0f;
						fi.clampMax_ = 0.5f;
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("最大許容距離");
						fi.offset_ = offsetof(Softbody, maxDistance_);
						fi.type_ = AttributeType::Float;
						fi.clampMin_ = 0.0f;
						fi.clampMax_ = 10.0f;
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("辺補強係数");
						fi.offset_ = offsetof(Softbody, edgeStiffness_);
						fi.type_ = AttributeType::Float;
						fi.clampMin_ = 0.0f;
						fi.clampMax_ = 1.0f;
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("曲耐性");
						fi.offset_ = offsetof(Softbody, bendStiffness_);
						fi.type_ = AttributeType::Float;
						fi.clampMin_ = 0.0f;
						fi.clampMax_ = 1.0f;
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("内部圧力");
						fi.offset_ = offsetof(Softbody, pressure_);
						fi.type_ = AttributeType::Float;
						fi.clampMin_ = -10.0f;
						fi.clampMax_ = 10.0f;
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("摩擦係数");
						fi.offset_ = offsetof(Softbody, friction_);
						fi.type_ = AttributeType::Float;
						fi.clampMin_ = 0.0f;
						fi.clampMax_ = 1.0f;
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("反発係数");
						fi.offset_ = offsetof(Softbody, restitution_);
						fi.type_ = AttributeType::Float;
						fi.clampMin_ = 0.0f;
						fi.clampMax_ = 1.0f;
						outInfo.push_back(std::move(fi));
					}
					outInfo.push_back({ String("重力"), offsetof(Softbody, useGravity_), AttributeType::Bool });
					{
						FieldInfo fi;
						fi.name_ = String("重力倍率");
						fi.offset_ = offsetof(Softbody, gravityScale_);
						fi.type_ = AttributeType::Float;
						fi.enableIf_ = [](void* p) -> Bool { auto& o = *static_cast<Softbody*>(p); return o.useGravity_; };
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("サブステップ数");
						fi.offset_ = offsetof(Softbody, subSteps_);
						fi.type_ = AttributeType::Int;
						fi.clampMin_ = 1;
						fi.clampMax_ = 20;
						outInfo.push_back(std::move(fi));
					}
					{
						FieldInfo fi;
						fi.name_ = String("反転回数");
						fi.offset_ = offsetof(Softbody, iterationCount_);
						fi.type_ = AttributeType::Int;
						fi.clampMin_ = 1;
						fi.clampMax_ = 10;
						outInfo.push_back(std::move(fi));
					}
				});
			}
		};
		static Register_Softbody global_Softbody_register;

		struct RegisterEnum_AnimationConditionComparison
		{
			RegisterEnum_AnimationConditionComparison()
			{
				EnumRegistry::Register(String("AnimationConditionComparison"), {
					{ static_cast<Int>(AnimationConditionComparison::Equal), String("Equal") },
					{ static_cast<Int>(AnimationConditionComparison::NotEqual), String("NotEqual") },
					{ static_cast<Int>(AnimationConditionComparison::Greater), String("Greater") },
					{ static_cast<Int>(AnimationConditionComparison::Less), String("Less") },
					{ static_cast<Int>(AnimationConditionComparison::GreaterOrEqual), String("GreaterOrEqual") },
					{ static_cast<Int>(AnimationConditionComparison::LessOrEqual), String("LessOrEqual") },
				});
			}
		};
		static RegisterEnum_AnimationConditionComparison global_AnimationConditionComparison_enum_register;

		struct RegisterEnum_AnimationParameterType
		{
			RegisterEnum_AnimationParameterType()
			{
				EnumRegistry::Register(String("AnimationParameterType"), {
					{ static_cast<Int>(AnimationParameterType::Bool), String("Bool") },
					{ static_cast<Int>(AnimationParameterType::Float), String("Float") },
					{ static_cast<Int>(AnimationParameterType::Int), String("Int") },
					{ static_cast<Int>(AnimationParameterType::Trigger), String("Trigger") },
				});
			}
		};
		static RegisterEnum_AnimationParameterType global_AnimationParameterType_enum_register;

		struct RegisterEnum_BodyType
		{
			RegisterEnum_BodyType()
			{
				EnumRegistry::Register(String("BodyType"), {
					{ static_cast<Int>(Rigidbody::BodyType::Dynamic), String("Dynamic") },
					{ static_cast<Int>(Rigidbody::BodyType::Kinematic), String("Kinematic") },
					{ static_cast<Int>(Rigidbody::BodyType::Static), String("Static") },
				});
			}
		};
		static RegisterEnum_BodyType global_BodyType_enum_register;

		struct RegisterEnum_DisplayMode
		{
			RegisterEnum_DisplayMode()
			{
				EnumRegistry::Register(String("DisplayMode"), {
					{ static_cast<Int>(Movie::DisplayMode::Fullscreen), String("Fullscreen") },
					{ static_cast<Int>(Movie::DisplayMode::Sprite), String("Sprite") },
					{ static_cast<Int>(Movie::DisplayMode::Billboard), String("Billboard") },
				});
			}
		};
		static RegisterEnum_DisplayMode global_DisplayMode_enum_register;

		struct RegisterEnum_MotionType
		{
			RegisterEnum_MotionType()
			{
				EnumRegistry::Register(String("MotionType"), {
					{ static_cast<Int>(Image::MotionType::Static), String("Static") },
					{ static_cast<Int>(Image::MotionType::Dynamic), String("Dynamic") },
				});
			}
		};
		static RegisterEnum_MotionType global_MotionType_enum_register;

		struct RegisterEnum_ToneCurve
		{
			RegisterEnum_ToneCurve()
			{
				EnumRegistry::Register(String("ToneCurve"), {
					{ static_cast<Int>(ToneMappingSettings::ToneCurve::None), String("None") },
					{ static_cast<Int>(ToneMappingSettings::ToneCurve::Reinhard), String("Reinhard") },
					{ static_cast<Int>(ToneMappingSettings::ToneCurve::AcesFilmic), String("AcesFilmic") },
					{ static_cast<Int>(ToneMappingSettings::ToneCurve::PbrNeutral), String("PbrNeutral") },
				});
			}
		};
		static RegisterEnum_ToneCurve global_ToneCurve_enum_register;

		struct RegisterEnum_ViewType
		{
			RegisterEnum_ViewType()
			{
				EnumRegistry::Register(String("ViewType"), {
					{ static_cast<Int>(Image::ViewType::Sprite), String("Sprite") },
					{ static_cast<Int>(Image::ViewType::Billboard), String("Billboard") },
				});
			}
		};
		static RegisterEnum_ViewType global_ViewType_enum_register;

		struct RegisterEnum_WeatherType
		{
			RegisterEnum_WeatherType()
			{
				EnumRegistry::Register(String("WeatherType"), {
					{ static_cast<Int>(WeatherType::Clear), String("Clear") },
					{ static_cast<Int>(WeatherType::Cloudy), String("Cloudy") },
					{ static_cast<Int>(WeatherType::Overcast), String("Overcast") },
					{ static_cast<Int>(WeatherType::Rain), String("Rain") },
					{ static_cast<Int>(WeatherType::Snow), String("Snow") },
					{ static_cast<Int>(WeatherType::Storm), String("Storm") },
				});
			}
		};
		static RegisterEnum_WeatherType global_WeatherType_enum_register;

	}
}