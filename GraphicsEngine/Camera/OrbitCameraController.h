#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/SeedScript.h>

namespace SeedCore
{
	class OrbitCameraController :public SeedScript
	{
	public:
		SC_REFLECTION_FIELD_EX("注視点X")
		Float targetX_ = 0.0f;

		SC_REFLECTION_FIELD_EX("注視点Y")
		Float targetY_ = 0.0f;

		SC_REFLECTION_FIELD_EX("注視点Z")
		Float targetZ_ = 0.0f;

		SC_REFLECTION_CLAMPED_EX("距離", 0.1f, 1000.0f)
		Float distance_ = 10.0f;

		SC_REFLECTION_FIELD_EX("回転速度")
		Float rotateSpeed_ = 0.3f;

		SC_REFLECTION_FIELD_EX("ズーム速度")
		Float zoomSpeed_ = 1.0f;

		SC_REFLECTION_CLAMPED_EX("最小距離", 0.1f, 1000.0f)
		Float minDistance_ = 0.5f;

		SC_REFLECTION_CLAMPED_EX("最大距離", 0.1f, 10000.0f)
		Float maxDistance_ = 500.0f;

	public:
		void OnTick(Float elapsedTime);

		void OnLateTick(Float elapsedTime);

	private:
		Float yaw_ = 0.0f;

		Float pitch_ = 0.0f;

		Bool initialized_ = false;
	};
	REGISTER_COMPONENT(OrbitCameraController, "Camera");
}
