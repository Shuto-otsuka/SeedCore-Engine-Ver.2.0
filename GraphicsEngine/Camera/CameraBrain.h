#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/SeedScript.h>

namespace SeedCore
{
	enum class CameraBrainMode
	{
		Free,
		Orbit,
		Follow,
		Lookat,
		Lockon,
		Cinematic,
	};

	class SEEDCORE_API CameraBrain :public SeedScript
	{
	public:
		SC_REFLECTION_FIELD_EX("モード")
		CameraBrainMode mode_ = CameraBrainMode::Free;

		SC_REFLECTION_FIELD_EX("カメラ向き")
		Vector3 direction_ = { 0.0f, 0.0f, 1.0f };

		SC_REFLECTION_CLAMPED_EX("重み", 1, 500)
		Int weight_ = 10;

		SC_REFLECTION_CLAMPED_EX("ブレンド時間", 0.0f, 10.0f)
		Float blendTime_ = 1.0f;

		SC_REFLECTION_FIELD_CONDITION(mode_ != CameraBrainMode::Free && mode_ != CameraBrainMode::Cinematic)
		SC_PAYLOAD_FIELD_EX("メインターゲット", Actor)
		Uint32 mainTarget_ = 0;

		SC_REFLECTION_FIELD_CONDITION(mode_ == CameraBrainMode::Lockon)
		SC_PAYLOAD_FIELD_EX("サブターゲット", Actor)
		Uint32 subTarget_ = 0;

		SC_REFLECTION_FIELD_CONDITION(mode_ == CameraBrainMode::Orbit || mode_ == CameraBrainMode::Follow || mode_ == CameraBrainMode::Lockon)
		SC_REFLECTION_CLAMPED_EX("距離", 0.1f, 1000.0f)
		Float distance_ = 5.0f;

		SC_REFLECTION_FIELD_CONDITION(mode_ == CameraBrainMode::Follow || mode_ == CameraBrainMode::Lockon)
		SC_REFLECTION_CLAMPED_EX("遅れ", 0.0f, 5.0f)
		Float damping_ = 0.15f;

	public:
		void OnLateTick(Float elapsedTime);

	public:
		void Move(const Vector3& delta);

		void Turn(Float yaw, Float pitch);

		void Teleport(const Vector3& position, const Vector3& direction);

		void Shake(Float amplitude, Float duration, Float frequency);

	private:
		friend class CameraSystem;

		Vector3 syncedDirection_ = { 0.0f, 0.0f, 1.0f };

		Vector3 syncedRotation_ = { 0.0f, 0.0f, 0.0f };

		Bool synced_ = false;

		Float fieldOfView_ = 60.0f;

		Float aspectRatio_ = 16.0f / 9.0f;

		Vector3 shakeAngles_ = { 0.0f, 0.0f, 0.0f };

		Float shakeAmplitude_ = 0.0f;

		Float shakeDuration_ = 0.0f;

		Float shakeFrequency_ = 0.0f;

		Float shakeTime_ = 0.0f;

		Float orbitYaw_ = 0.0f;

		Float orbitPitch_ = 0.0f;

		Vector3 orbitCenter_ = { 0.0f, 0.0f, 0.0f };

		Float lockonAxisYaw_ = 0.0f;

		Bool orbitInitialized_ = false;

		CameraBrainMode previousMode_ = CameraBrainMode::Free;

		Bool snap_ = false;

		Bool cut_ = false;
	};
	REGISTER_COMPONENT(CameraBrain, "Camera");
}
