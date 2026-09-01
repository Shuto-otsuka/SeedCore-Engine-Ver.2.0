#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/SeedScript.h>

namespace SeedCore
{
	class SEEDCORE_API FreeCameraController :public SeedScript
	{
	public:
		SC_REFLECTION_FIELD_EX("移動速度")
		Float moveSpeed_ = 10.0f;

		SC_REFLECTION_FIELD_EX("回転速度")
		Float rotateSpeed_ = 0.15f;

		SC_REFLECTION_FIELD_EX("スクロール速度")
		Float scrollSpeed_ = 5.0f;

		SC_REFLECTION_FIELD_EX("パン速度")
		Float panSpeed_ = 0.02f;

		SC_REFLECTION_FIELD_EX("Shift速度倍率")
		Float shiftSpeedMultiplier_ = 3.0f;

	public:
		void OnTick(Float elapsedTime);

		void OnLateTick(Float elapsedTime);

	private:
		Quaternion rotation_ = Quaternion::Identity;

		Bool initialized_ = false;
	};
	REGISTER_COMPONENT(FreeCameraController, "Camera");
}
