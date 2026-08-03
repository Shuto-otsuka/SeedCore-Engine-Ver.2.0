#include <GraphicsEngine/Camera/PreviewCamera.h>

namespace SeedCore
{
	void PreviewCamera::Tick(Float elapsedTime)
	{
		previousViewProjection_ = currentViewProjection_;

		forward_ = focus_ - eye_;
		if (forward_.LengthSquared() < 1e-4)
		{
			forward_ = Vector3::Forward;
		}
		else
		{
			forward_.Normalize();
		}

		right_ = forward_.Cross(up_);
		if (right_.LengthSquared() < 1e-4)
		{
			right_ = Vector3::Right;
		}
		else
		{
			right_.Normalize();
		}

		view_ = Matrix::CreateLookAt(eye_, focus_, up_);

		aspectRatio_ = width_ / height_;
		nonJitterProjection_ = Matrix::CreatePerspectiveFieldOfView(ToRadians(Max(0.001f, fieldOfView_)), aspectRatio_, nearPlane_, farPlane_);
		nonJitterProjection_._33 = nearPlane_ / (nearPlane_ - farPlane_);
		nonJitterProjection_._43 = (farPlane_ * nearPlane_) / (farPlane_ - nearPlane_);

		projection_ = nonJitterProjection_;

		currentViewProjection_ = view_ * projection_;

		nonJitterViewProjection_ = view_ * nonJitterProjection_;

		inverseView_ = view_.Invert();

		inverseProjection_ = projection_.Invert();

		inverseViewProjection_ = currentViewProjection_.Invert();
	}

	void PreviewCamera::Resize(Float width, Float height)
	{
		width_ = width;
		height_ = height;
	}

	void PreviewCamera::Eye(Vector3 eye)
	{
		eye_ = eye;
	}

	void PreviewCamera::Focus(Vector3 focus)
	{
		focus_ = focus;
	}

	void PreviewCamera::Up(Vector3 up)
	{
		up_ = up;
	}

	void PreviewCamera::Near(Float nearPlane)
	{
		nearPlane_ = nearPlane;
	}

	void PreviewCamera::Far(Float farPlane)
	{
		farPlane_ = farPlane;
	}

	void PreviewCamera::Fov(Float fieldOfView)
	{
		fieldOfView_ = fieldOfView;
	}

	Vector3 PreviewCamera::Eye()const
	{
		return eye_;
	}

	Vector3 PreviewCamera::Focus()const
	{
		return focus_;
	}

	Vector3 PreviewCamera::Up()const
	{
		return up_;
	}

	Vector3 PreviewCamera::Forward()const
	{
		return forward_;
	}

	Vector3 PreviewCamera::Right()const
	{
		return right_;
	}

	Float PreviewCamera::Near()const
	{
		return nearPlane_;
	}

	Float PreviewCamera::Far()const
	{
		return farPlane_;
	}

	Float PreviewCamera::Fov()const
	{
		return fieldOfView_;
	}

	Float PreviewCamera::AspectRatio()const
	{
		return aspectRatio_;
	}

	Float PreviewCamera::Width()const
	{
		return width_;
	}

	Float PreviewCamera::Height()const
	{
		return height_;
	}

	Matrix PreviewCamera::View()const
	{
		return view_;
	}

	Matrix PreviewCamera::InverseView()const
	{
		return inverseView_;
	}

	Matrix PreviewCamera::Projection()const
	{
		return projection_;
	}

	Matrix PreviewCamera::InverseProjection()const
	{
		return inverseProjection_;
	}

	Matrix PreviewCamera::NonJitterProjection()const
	{
		return nonJitterProjection_;
	}

	Matrix PreviewCamera::CurrentViewProjection()const
	{
		return currentViewProjection_;
	}

	Matrix PreviewCamera::PreviousViewProjection()const
	{
		return previousViewProjection_;
	}

	Matrix PreviewCamera::InverseViewProjection()const
	{
		return inverseViewProjection_;
	}

	Matrix PreviewCamera::NonJitterViewProjection()const
	{
		return nonJitterViewProjection_;
	}
}
