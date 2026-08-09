#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	class SEEDCORE_API CanvasCamera
	{
	public:
		void Tick(Float elapsedTime);

		void Resize(Float width, Float height);

		void Eye(Vector3 eye);

		void Focus(Vector3 focus);

		void Up(Vector3 up);

		void Near(Float nearPlane);

		void Far(Float farPlane);

		void Fov(Float fieldOfView);

		void Zoom(Float zoom);

		Vector3 Eye()const;

		Vector3 Focus()const;

		Vector3 Up()const;

		Vector3 Forward()const;

		Vector3 Right()const;

		Float Near()const;

		Float Far()const;

		Float Fov()const;

		Float Zoom()const;

		Float VisibleHeight()const;

		Float AspectRatio()const;

		Float Width()const;

		Float Height()const;

		Matrix View()const;

		Matrix InverseView()const;

		Matrix Projection()const;

		Matrix InverseProjection()const;

		Matrix NonJitterProjection()const;

		Matrix CurrentViewProjection()const;

		Matrix PreviousViewProjection()const;

		Matrix InverseViewProjection()const;

		Matrix NonJitterViewProjection()const;

	private:
		Vector3 eye_ = { 100640,100360,99990 };

		Vector3 focus_ = { 100640,100360,100000 };

		Vector3 up_ = { 0,1,0 };

		Vector3 forward_ = { 0,0,1 };

		Vector3 right_ = { 1,0,0 };

		Float nearPlane_ = 0.001f;

		Float farPlane_ = 1000.0f;

		Float fieldOfView_ = 60.0f;

		Float zoom_ = 1.0f;

		Float baseViewHeight_ = 720.0f;

		Float aspectRatio_ = 16.0f / 9.0f;

		Vector2 jitter_ = { 0.5f,0.5f };

		Uint32 frameIndex_ = 0;

		Matrix view_ = Matrix::Identity;

		Matrix inverseView_ = Matrix::Identity;

		Matrix projection_ = Matrix::Identity;

		Matrix inverseProjection_ = Matrix::Identity;

		Matrix nonJitterProjection_ = Matrix::Identity;

		Matrix currentViewProjection_ = Matrix::Identity;

		Matrix previousViewProjection_ = Matrix::Identity;

		Matrix inverseViewProjection_ = Matrix::Identity;

		Matrix nonJitterViewProjection_ = Matrix::Identity;

		Float width_ = 0.0f;

		Float height_ = 0.0f;
	};
}