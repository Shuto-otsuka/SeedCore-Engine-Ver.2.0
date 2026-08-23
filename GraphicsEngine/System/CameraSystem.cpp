#include <GraphicsEngine/System/CameraSystem.h>
#include <GraphicsEngine/Camera/Camera.h>
#include <GraphicsEngine/Camera/ScreenSpace.h>
#include <GraphicsEngine/D3D12/SwapChain/GraphicsResolution.h>
#include <FoundationEngine/ECS/Query.h>
#include <FoundationEngine/ECS/Component/Position.h>
#include <FoundationEngine/ECS/Component/Rotation.h>
#include <FoundationEngine/ECS/Component/Active.h>
#include <FoundationEngine/Time/GameTimer.h>

namespace SeedCore
{
	void CameraSystem::Update(World& world, GameTimer& timer, Float width, Float height)
	{
		if (mode_ == Mode::Free)
		{
			UpdateFreeCamera(world, timer, width, height);
		}
		else
		{
			UpdateUserCamera(world, timer, width, height);
		}
	}

	void CameraSystem::UpdateFreeCameraInput(Float deltaTime)
	{
		freeCameraController_.Update(freeCamera_, deltaTime);
	}

	void CameraSystem::UpdateFreeCamera(World& world, GameTimer& timer, Float width, Float height)
	{
		hasActiveCamera_ = false;

		Query<Read<Active>, Read<Camera>> cameraQuery(world);
		cameraQuery.ForEach([&](const Active& active, const Camera& camera)
			{
				if (hasActiveCamera_ || !active.active_ || !camera.isActive_)
				{
					return;
				}
				hasActiveCamera_ = true;

				freeCamera_.Fov(camera.fieldOfView_);
				freeCamera_.Near(camera.nearPlane_);
				freeCamera_.Far(camera.farPlane_);
			});

		if (!hasActiveCamera_)
		{
			return;
		}

		freeCamera_.Resize(width, height);
		freeCamera_.Tick(timer.DeltaTime());

		sceneConstantBuffer_.view_ = freeCamera_.View();
		sceneConstantBuffer_.inverseView_ = freeCamera_.InverseView();
		sceneConstantBuffer_.projection_ = freeCamera_.Projection();
		sceneConstantBuffer_.inverseProjection_ = freeCamera_.InverseProjection();
		sceneConstantBuffer_.nonJitterProjection_ = freeCamera_.NonJitterProjection();
		sceneConstantBuffer_.currentViewProjection_ = freeCamera_.CurrentViewProjection();
		sceneConstantBuffer_.previousViewProjection_ = previousViewProjection_;
		sceneConstantBuffer_.inverseViewProjection_ = freeCamera_.InverseViewProjection();
		sceneConstantBuffer_.nonJitterViewProjection_ = freeCamera_.NonJitterViewProjection();
		sceneConstantBuffer_.previousNonJitterViewProjection_ = previousNonJitterViewProjection_;
		sceneConstantBuffer_.cameraPosition_ = Vector4(freeCamera_.Eye().x, freeCamera_.Eye().y, freeCamera_.Eye().z, 1.0f);
		sceneConstantBuffer_.cameraFocus_ = Vector4(freeCamera_.Focus().x, freeCamera_.Focus().y, freeCamera_.Focus().z, 1.0f);
		sceneConstantBuffer_.fieldOfView_ = freeCamera_.Fov();
		sceneConstantBuffer_.nearPlane_ = freeCamera_.Near();
		sceneConstantBuffer_.farPlane_ = freeCamera_.Far();
		sceneConstantBuffer_.totalTime_ = timer.TotalTime();
		sceneConstantBuffer_.deltaTime_ = timer.DeltaTime();
		sceneConstantBuffer_.screenSize_ = Vector2(width, height);
		sceneConstantBuffer_.inverseScreenSize_ = Vector2(1.0f / width, 1.0f / height);
		sceneConstantBuffer_.displaySize_ = sceneConstantBuffer_.screenSize_;

		previousViewProjection_ = freeCamera_.CurrentViewProjection();
		previousNonJitterViewProjection_ = freeCamera_.NonJitterViewProjection();
	}

	void CameraSystem::UpdateUserCamera(World& world, GameTimer& timer, Float width, Float height)
	{
		hasActiveCamera_ = false;

		Query<Read<Active>, Read<Camera>, Read<Position>, Read<Rotation>> query(world);
		query.ForEach([&](const Active& active, const Camera& camera, const Position& position, const Rotation& rotation)
			{
				if (hasActiveCamera_ || !active.active_ || !camera.isActive_)
				{
					return;
				}
				hasActiveCamera_ = true;

				const Vector3 eye = Vector3(position.x, position.y, position.z);
				const Float rx = ToRadians(rotation.x);
				const Float ry = ToRadians(rotation.y);
				const Float rz = ToRadians(rotation.z);

				const Matrix rotMatrix = Matrix::CreateFromYawPitchRoll(ry, rx, rz);
				const Vector3 forward = Vector3::TransformNormal(Vector3::Forward, rotMatrix);
				const Vector3 focus = eye + forward;
				const Vector3 up = Vector3::TransformNormal(Vector3::Up, rotMatrix);

				const Float aspectRatio = width / height;
				const Matrix view = Matrix::CreateLookAt(eye, focus, up);

				Matrix nonJitterProjection = Matrix::CreatePerspectiveFieldOfView(ToRadians(camera.fieldOfView_), aspectRatio, camera.nearPlane_, camera.farPlane_);
				nonJitterProjection._33 = camera.nearPlane_ / (camera.nearPlane_ - camera.farPlane_);
				nonJitterProjection._43 = (camera.farPlane_ * camera.nearPlane_) / (camera.farPlane_ - camera.nearPlane_);

				jitter_ = Halton23Jitter(frameIndex_);
				++frameIndex_;

				Matrix projection = nonJitterProjection;
				if (jitter_.LengthSquared() > 0.0f)
				{
					projection._31 += (jitter_.x * 2.0f) / width;
					projection._32 -= (jitter_.y * 2.0f) / height;
				}

				const Matrix viewProjection = view * projection;
				const Matrix nonJitterViewProjection = view * nonJitterProjection;

				sceneConstantBuffer_.view_ = view;
				sceneConstantBuffer_.inverseView_ = view.Invert();
				sceneConstantBuffer_.projection_ = projection;
				sceneConstantBuffer_.inverseProjection_ = projection.Invert();
				sceneConstantBuffer_.nonJitterProjection_ = nonJitterProjection;
				sceneConstantBuffer_.currentViewProjection_ = viewProjection;
				sceneConstantBuffer_.previousViewProjection_ = previousViewProjection_;
				sceneConstantBuffer_.inverseViewProjection_ = viewProjection.Invert();
				sceneConstantBuffer_.nonJitterViewProjection_ = nonJitterViewProjection;
				sceneConstantBuffer_.previousNonJitterViewProjection_ = previousNonJitterViewProjection_;
				sceneConstantBuffer_.cameraPosition_ = Vector4(eye.x, eye.y, eye.z, 1.0f);
				sceneConstantBuffer_.cameraFocus_ = Vector4(focus.x, focus.y, focus.z, 1.0f);
				sceneConstantBuffer_.fieldOfView_ = camera.fieldOfView_;
				sceneConstantBuffer_.nearPlane_ = camera.nearPlane_;
				sceneConstantBuffer_.farPlane_ = camera.farPlane_;
				sceneConstantBuffer_.totalTime_ = timer.TotalTime();
				sceneConstantBuffer_.deltaTime_ = timer.DeltaTime();
				sceneConstantBuffer_.screenSize_ = Vector2(width, height);
				sceneConstantBuffer_.inverseScreenSize_ = Vector2(1.0f / width, 1.0f / height);
				sceneConstantBuffer_.displaySize_ = sceneConstantBuffer_.screenSize_;

				previousViewProjection_ = viewProjection;
				previousNonJitterViewProjection_ = nonJitterViewProjection;

				/// [EN] nonJitterProjection, not the TAA-jittered projection
				///      above - gameplay screen<->world picking should use
				///      the camera's true projection, not the render-only
				///      sub-pixel jitter offset.
				/// [JP] 上のTAAジッター付きprojectionではなくnonJitterProjection
				///      を使う - ゲームプレイのスクリーン⇔ワールド変換は、
				///      描画専用のサブピクセルジッターオフセットではなく
				///      カメラ本来のprojectionを使うべきなため。
				ScreenSpace::SetCurrentView(view, nonJitterProjection, Vector2(width, height));
			});
	}

	Bool CameraSystem::HasActiveCamera()const
	{
		return hasActiveCamera_;
	}

	const SceneConstantBuffer& CameraSystem::GetSceneConstantBuffer()const
	{
		return sceneConstantBuffer_;
	}

	void CameraSystem::SetMode(Mode mode)
	{
		mode_ = mode;
	}

	CameraSystem::Mode CameraSystem::GetMode()const
	{
		return mode_;
	}
}
