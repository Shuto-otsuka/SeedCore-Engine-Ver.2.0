#include <FoundationEngine/ECS/ReflectionRegistry.h>

// [REFLECTION_AUTO_BEGIN]
#pragma comment(linker, "/include:_force_reflection_Name")
#pragma comment(linker, "/include:_force_reflection_Position")
#pragma comment(linker, "/include:_force_reflection_Rotation")
#pragma comment(linker, "/include:_force_reflection_Scale")
#pragma comment(linker, "/include:_force_reflection_Velocity")
#pragma comment(linker, "/include:_force_reflection_Camera")
#pragma comment(linker, "/include:_force_reflection_FreeCameraController")
#pragma comment(linker, "/include:_force_reflection_OrbitCameraController")
#pragma comment(linker, "/include:_force_reflection_LookAtConstraint")
#pragma comment(linker, "/include:_force_reflection_ParentConstraint")
#pragma comment(linker, "/include:_force_reflection_PositionConstraint")
#pragma comment(linker, "/include:_force_reflection_RotationConstraint")
#pragma comment(linker, "/include:_force_reflection_Weather")
#pragma comment(linker, "/include:_force_reflection_Text")
#pragma comment(linker, "/include:_force_reflection_DirectionalLight")
#pragma comment(linker, "/include:_force_reflection_PointLight")
#pragma comment(linker, "/include:_force_reflection_RectangleLight")
#pragma comment(linker, "/include:_force_reflection_SkyLight")
#pragma comment(linker, "/include:_force_reflection_SpotLight")
#pragma comment(linker, "/include:_force_reflection_AnimationParameter")
#pragma comment(linker, "/include:_force_reflection_AnimationCondition")
#pragma comment(linker, "/include:_force_reflection_AnimationState")
#pragma comment(linker, "/include:_force_reflection_AnimationTransition")
#pragma comment(linker, "/include:_force_reflection_Animator")
#pragma comment(linker, "/include:_force_reflection_Movie")
#pragma comment(linker, "/include:_force_reflection_ExposureSettings")
#pragma comment(linker, "/include:_force_reflection_ToneMappingSettings")
#pragma comment(linker, "/include:_force_reflection_LensFlareSettings")
#pragma comment(linker, "/include:_force_reflection_DepthOfFieldSettings")
#pragma comment(linker, "/include:_force_reflection_BokehSettings")
#pragma comment(linker, "/include:_force_reflection_SharpnessSettings")
#pragma comment(linker, "/include:_force_reflection_PostProcess")
#pragma comment(linker, "/include:_force_reflection_Image")
#pragma comment(linker, "/include:_force_reflection_BoxCollider")
#pragma comment(linker, "/include:_force_reflection_CapsuleCollider")
#pragma comment(linker, "/include:_force_reflection_CircleCollider")
#pragma comment(linker, "/include:_force_reflection_CylinderCollider")
#pragma comment(linker, "/include:_force_reflection_MeshCollider")
#pragma comment(linker, "/include:_force_reflection_RectCollider")
#pragma comment(linker, "/include:_force_reflection_SphereCollider")
#pragma comment(linker, "/include:_force_reflection_Rigidbody")
#pragma comment(linker, "/include:_force_reflection_Softbody")
// [REFLECTION_AUTO_END]

namespace SeedCore
{
	/**
	* [EN]
	* Returns the full registry mapping reflected type names to their ReflectFunc.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* リフレクションされた型名をその ReflectFunc へ対応付ける、完全な
	* レジストリを返す。
	*/
	FlatMap<String, ReflectionRegistry::ReflectFunc>& ReflectionRegistry::GetRegistry()
	{
		static FlatMap<String, ReflectFunc> instance;
		return instance;
	}

	/**
	* [EN]
	* Registers function as the reflector for the type named name.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* name という名前の型に対するリフレクタとして function を登録する。
	*/
	void ReflectionRegistry::Register(String name, ReflectFunc function)
	{
		GetRegistry()[name] = function;
	}

	/**
	* [EN]
	* Returns the full registry mapping enum type names to their entries.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 列挙型名をそのエントリ一覧へ対応付ける、完全なレジストリを返す。
	*/
	FlatMap<String, DynamicArray<EnumEntry>>& EnumRegistry::GetRegistry()
	{
		static FlatMap<String, DynamicArray<EnumEntry>> instance;
		return instance;
	}

	/**
	* [EN]
	* Registers entries as the named/valued entries of the enum type typeName.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* typeName という列挙型の名前付きエントリ一覧として entries を
	* 登録する。
	*/
	void EnumRegistry::Register(String typeName, DynamicArray<EnumEntry> entries)
	{
		GetRegistry()[typeName] = std::move(entries);
	}

	/**
	* [EN]
	* Returns the entries registered for typeName, or nullptr if that
	* enum type is unregistered.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* typeName に対して登録されているエントリ一覧を返す。その列挙型が
	* 未登録であれば nullptr を返す。
	*/
	const DynamicArray<EnumEntry>* EnumRegistry::GetEntries(const String& typeName)
	{
		auto& registry = GetRegistry();
		auto it = registry.find(typeName);
		if (it != registry.end())
		{
			return &it->second;
		}
		return nullptr;
	}
}
