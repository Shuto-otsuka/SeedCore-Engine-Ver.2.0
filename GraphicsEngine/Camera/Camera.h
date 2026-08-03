#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/SeedScript.h>

namespace SeedCore
{
	class Camera :public SeedScript
	{
	public:
		SC_REFLECTION_FIELD_EX("FOV")
		Float fieldOfView_ = 60.0f;

		SC_REFLECTION_FIELD_EX("近クリップ面")
		Float nearPlane_ = 0.001f;

		SC_REFLECTION_FIELD_EX("遠クリップ面")
		Float farPlane_ = 1000.0f;

		SC_REFLECTION_FIELD_EX("アクティブカメラ")
		Bool isActive_ = true;

	public:
		void OnTick(Float elapsedTime);

		void OnLateTick(Float elapsedTime);
	};
	REGISTER_COMPONENT(Camera, "Camera");
}