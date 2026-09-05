#include <FoundationEngine/ECS/PayloadRegistry.h>

// [PAYLOAD_AUTO_BEGIN]
#pragma comment(linker, "/include:_force_payload_Spawner")
#pragma comment(linker, "/include:_force_payload_AttachmentConstraint")
#pragma comment(linker, "/include:_force_payload_Effector")
#pragma comment(linker, "/include:_force_payload_LookAtConstraint")
#pragma comment(linker, "/include:_force_payload_ParentConstraint")
#pragma comment(linker, "/include:_force_payload_PositionConstraint")
#pragma comment(linker, "/include:_force_payload_RotationConstraint")
#pragma comment(linker, "/include:_force_payload_Text")
#pragma comment(linker, "/include:_force_payload_SkyLight")
#pragma comment(linker, "/include:_force_payload_Mesh")
#pragma comment(linker, "/include:_force_payload_Animator")
#pragma comment(linker, "/include:_force_payload_Material")
#pragma comment(linker, "/include:_force_payload_Skeleton")
#pragma comment(linker, "/include:_force_payload_Movie")
#pragma comment(linker, "/include:_force_payload_Image")
#pragma comment(linker, "/include:_force_payload_MeshCollider")
#pragma comment(linker, "/include:_force_payload_FixedJoint")
#pragma comment(linker, "/include:_force_payload_HingeJoint")
#pragma comment(linker, "/include:_force_payload_SliderJoint")
#pragma comment(linker, "/include:_force_payload_SpringJoint")
// [PAYLOAD_AUTO_END]

namespace SeedCore
{
	/**
	* [EN]
	* Returns the full registry mapping reflected type names to their PayloadFunc.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* リフレクションされた型名をその PayloadFunc へ対応付ける、完全な
	* レジストリを返す。
	*/
	FlatMap<String, PayloadRegistry::PayloadFunc>& PayloadRegistry::GetRegistry()
	{
		static FlatMap<String, PayloadFunc> instance;
		return instance;
	}

	/**
	* [EN]
	* Registers function as the payload reflector for the type named name.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* name という名前の型に対するペイロードリフレクタとして function
	* を登録する。
	*/
	void PayloadRegistry::Register(String name, PayloadFunc function)
	{
		GetRegistry()[name] = function;
	}
}
