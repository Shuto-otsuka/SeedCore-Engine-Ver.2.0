#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/Math/Halton.h>
#include <FoundationEngine/World/ECS/Entity/Entity.h>
#include <GraphicsEngine/System/SceneSystem.h>
#include <GraphicsEngine/Camera/EditorCamera.h>
#include <GraphicsEngine/Camera/EditorCameraController.h>

namespace SeedCore
{
	class World;
	class GameTimer;

	class SEEDCORE_API CameraSystem
	{
	public:
		enum class Mode
		{
			Free,
			User,
		};

		void Update(World& world, GameTimer& timer, Float width, Float height);

		Bool HasActiveCamera()const;

		const SceneConstantBuffer& GetSceneConstantBuffer()const;

		void SetMode(Mode mode);

		Mode GetMode()const;

	private:
	public:
		void UpdateFreeCameraInput(Float deltaTime);

	private:
		void UpdateFreeCamera(World& world, GameTimer& timer, Float width, Float height);
		void UpdateUserCamera(World& world, GameTimer& timer, Float width, Float height);

		SceneConstantBuffer sceneConstantBuffer_{};
		Matrix previousViewProjection_ = Matrix::Identity;
		Matrix previousNonJitterViewProjection_ = Matrix::Identity;
		Vector2 jitter_ = { 0.5f, 0.5f };
		Uint32 frameIndex_ = 0;
		Bool hasActiveCamera_ = false;
		Mode mode_ = Mode::User;

		EntityID activeBrain_;
		Vector3 lastEye_ = Vector3::Zero;
		Quaternion lastOrientation_ = Quaternion::Identity;
		Vector3 blendFromEye_ = Vector3::Zero;
		Quaternion blendFromOrientation_ = Quaternion::Identity;
		Float blendElapsed_ = 0.0f;
		Float blendDuration_ = 0.0f;

		EditorCamera freeCamera_;
		EditorCameraController freeCameraController_;
	};
}
