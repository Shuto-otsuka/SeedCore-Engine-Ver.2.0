#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	class EditorCamera;

	class SEEDCORE_API EditorCameraController
	{
	public:
		void Update(EditorCamera& camera, Float deltaTime);

		void MoveSpeed(Float speed);

		void RotateSpeed(Float speed);

		void ScrollSpeed(Float speed);

		void PanSpeed(Float speed);

		void ShiftSpeedMultiplier(Float multiplier);

		Float MoveSpeed()const;

		Float RotateSpeed()const;

		Float ScrollSpeed()const;

		Float PanSpeed()const;

		Float ShiftSpeedMultiplier()const;

	private:
		Quaternion rotation_ = Quaternion::Identity;

		Float moveSpeed_ = 10.0f;

		Float rotateSpeed_ = 0.15f;

		Float scrollSpeed_ = 5.0f;

		Float panSpeed_ = 0.02f;

		Float shiftSpeedMultiplier_ = 3.0f;

		Bool initialized_ = false;
	};
}
