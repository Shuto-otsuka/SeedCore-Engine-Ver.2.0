#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	class SEEDCORE_API PreviewCamera
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

		Vector3 Eye()const;

		Vector3 Focus()const;

		Vector3 Up()const;

		Vector3 Forward()const;

		Vector3 Right()const;

		Float Near()const;

		Float Far()const;

		Float Fov()const;

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
		Vector3 eye_ = { 0,7.0f,30.0f };

		Vector3 focus_ = { 0,1.0f,0 };

		Vector3 up_ = { 0,1,0 };

		Vector3 forward_ = { 0,0,1 };

		Vector3 right_ = { 1,0,0 };

		Float nearPlane_ = 0.01f;

		Float farPlane_ = 1000.0f;

		Float fieldOfView_ = 60.0f;

		Float aspectRatio_ = 16.0f / 9.0f;

		Matrix view_ = Matrix::Identity;

		Matrix inverseView_ = Matrix::Identity;

		Matrix projection_ = Matrix::Identity;

		Matrix inverseProjection_ = Matrix::Identity;

		Matrix nonJitterProjection_ = Matrix::Identity;

		Matrix currentViewProjection_ = Matrix::Identity;

		Matrix previousViewProjection_ = Matrix::Identity;

		Matrix inverseViewProjection_ = Matrix::Identity;

		Matrix nonJitterViewProjection_ = Matrix::Identity;

		Float width_ = 1280.0f;

		Float height_ = 720.0f;
	};
}
