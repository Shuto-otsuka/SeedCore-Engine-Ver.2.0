#include <GraphicsEngine/System/CameraSystem.h>
#include <GraphicsEngine/Camera/Camera.h>
#include <GraphicsEngine/Camera/CameraBrain.h>
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
		Float fieldOfView = 60.0f;
		Bool lensFound = false;
		Query<Read<Active>, Read<Camera>> lensQuery(world);
		lensQuery.ForEach([&](const Active& active, const Camera& camera)
			{
				if (lensFound || !active.active_ || !camera.isActive_)
				{
					return;
				}
				lensFound = true;
				fieldOfView = camera.fieldOfView_;
			});

		Query<Write<CameraBrain>, Write<Rotation>> syncQuery(world);
		syncQuery.ForEach([&](CameraBrain& brain, Rotation& rotation)
			{
				brain.fieldOfView_ = fieldOfView;
				brain.aspectRatio_ = width / height;

				Vector3 rotationEuler(rotation.x_, rotation.y_, rotation.z_);
				Bool directionChanged = brain.synced_ && (brain.direction_ - brain.syncedDirection_).LengthSquared() > 1e-10f;
				Bool rotationChanged = !brain.synced_ || (rotationEuler - brain.syncedRotation_).LengthSquared() > 1e-10f;

				if (directionChanged)
				{
					Vector3 direction = brain.direction_;
					if (direction.LengthSquared() < 1e-8f)
					{
						direction = brain.syncedDirection_;
					}
					direction.Normalize();
					brain.direction_ = direction;

					rotation.x_ = ToDegrees(Asin(Clamp(-direction.y, -1.0f, 1.0f)));
					if (direction.x * direction.x + direction.z * direction.z > 1e-8f)
					{
						rotation.y_ = ToDegrees(Atan2(direction.x, direction.z));
					}
				}
				else if (rotationChanged)
				{
					Matrix rotationMatrix = Matrix::CreateFromYawPitchRoll(ToRadians(rotation.y_), ToRadians(rotation.x_), ToRadians(rotation.z_));
					Vector3 direction = Vector3::TransformNormal(Vector3::Forward, rotationMatrix);
					direction.Normalize();
					brain.direction_ = direction;
				}

				brain.syncedDirection_ = brain.direction_;
				brain.syncedRotation_ = Vector3(rotation.x_, rotation.y_, rotation.z_);
				brain.synced_ = true;
			});

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

		const Camera* activeCamera = nullptr;
		Vector3 eye = Vector3::Zero;
		Quaternion orientation = Quaternion::Identity;

		Query<Read<Active>, Read<Camera>, Read<Position>, Read<Rotation>> query(world);
		query.ForEach([&](const Active& active, const Camera& camera, const Position& position, const Rotation& rotation)
			{
				if (hasActiveCamera_ || !active.active_ || !camera.isActive_)
				{
					return;
				}
				hasActiveCamera_ = true;
				activeCamera = &camera;
				eye = Vector3(position.x_, position.y_, position.z_);
				orientation = Quaternion::CreateFromYawPitchRoll(ToRadians(rotation.y_), ToRadians(rotation.x_), ToRadians(rotation.z_));
			});

		if (!hasActiveCamera_)
		{
			return;
		}

		CameraBrain* activeBrain = nullptr;
		EntityID activeBrainEntity;
		Bool activeBrainCut = false;
		Vector3 brainEye = Vector3::Zero;
		Quaternion brainOrientation = Quaternion::Identity;

		Query<Read<Active>, Write<CameraBrain>, Read<Position>, Read<Rotation>> brainQuery(world);
		brainQuery.ForEach([&](EntityID entity, const Active& active, CameraBrain& brain, const Position& position, const Rotation& rotation)
			{
				Bool brainCut = brain.cut_;
				brain.cut_ = false;

				if (!active.active_)
				{
					return;
				}
				if (activeBrain && (brain.weight_ < activeBrain->weight_ || (brain.weight_ == activeBrain->weight_ && entity != activeBrain_)))
				{
					return;
				}

				activeBrain = &brain;
				activeBrainEntity = entity;
				activeBrainCut = brainCut;
				brainEye = Vector3(position.x_, position.y_, position.z_);
				brainOrientation = Quaternion::CreateFromYawPitchRoll(ToRadians(rotation.y_ + brain.shakeAngles_.y), ToRadians(rotation.x_ + brain.shakeAngles_.x), ToRadians(rotation.z_ + brain.shakeAngles_.z));
			});

		Bool cut = false;
		if (activeBrain)
		{
			if (activeBrainEntity != activeBrain_)
			{
				Bool blend = timer.Playing() && activeBrain_ != EntityID() && activeBrain->blendTime_ > 0.0f;
				blendFromEye_ = lastEye_;
				blendFromOrientation_ = lastOrientation_;
				blendElapsed_ = 0.0f;
				blendDuration_ = blend ? activeBrain->blendTime_ : 0.0f;
				activeBrain_ = activeBrainEntity;
			}
			if (activeBrainCut)
			{
				cut = true;
				blendDuration_ = 0.0f;
			}
			eye = brainEye;
			orientation = brainOrientation;
		}
		else
		{
			activeBrain_ = EntityID();
			blendDuration_ = 0.0f;
		}

		if (blendElapsed_ < blendDuration_)
		{
			blendElapsed_ += timer.DeltaTime();
			Float blendRatio = Clamp(blendElapsed_ / blendDuration_, 0.0f, 1.0f);
			blendRatio = blendRatio * blendRatio * (3.0f - 2.0f * blendRatio);
			eye = Vector3::Lerp(blendFromEye_, eye, blendRatio);
			orientation = Quaternion::Slerp(blendFromOrientation_, orientation, blendRatio);
		}
		lastEye_ = eye;
		lastOrientation_ = orientation;

		const Vector3 forward = Vector3::Transform(Vector3::Forward, orientation);
		const Vector3 focus = eye + forward;
		const Vector3 up = Vector3::Transform(Vector3::Up, orientation);

		const Float aspectRatio = width / height;
		const Matrix view = Matrix::CreateLookAt(eye, focus, up);

		Matrix nonJitterProjection = Matrix::CreatePerspectiveFieldOfView(ToRadians(activeCamera->fieldOfView_), aspectRatio, activeCamera->nearPlane_, activeCamera->farPlane_);
		nonJitterProjection._33 = activeCamera->nearPlane_ / (activeCamera->nearPlane_ - activeCamera->farPlane_);
		nonJitterProjection._43 = (activeCamera->farPlane_ * activeCamera->nearPlane_) / (activeCamera->farPlane_ - activeCamera->nearPlane_);

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

		if (cut)
		{
			previousViewProjection_ = viewProjection;
			previousNonJitterViewProjection_ = nonJitterViewProjection;
		}

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
		sceneConstantBuffer_.fieldOfView_ = activeCamera->fieldOfView_;
		sceneConstantBuffer_.nearPlane_ = activeCamera->nearPlane_;
		sceneConstantBuffer_.farPlane_ = activeCamera->farPlane_;
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
