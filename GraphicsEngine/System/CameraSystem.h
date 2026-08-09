#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/Math/Halton.h>
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

		EditorCamera freeCamera_;
		EditorCameraController freeCameraController_;
	};
}
