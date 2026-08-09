#include <GraphicsEngine/Camera/CanvasCamera.h>

namespace SeedCore
{
	void CanvasCamera::Tick(Float elapsedTime)
	{
		previousViewProjection_ = currentViewProjection_;

		view_ = Matrix::CreateLookAt(eye_, focus_, up_);

		aspectRatio_ = width_ / height_;

		Float orthoHeight = baseViewHeight_ / zoom_;
		Float orthoWidth = orthoHeight * aspectRatio_;

		nonJitterProjection_ = Matrix::CreateOrthographic(orthoWidth, orthoHeight, nearPlane_, farPlane_);

		nonJitterProjection_._33 = 1.0f / (nearPlane_ - farPlane_);
		nonJitterProjection_._43 = farPlane_ / (farPlane_ - nearPlane_);

		projection_ = nonJitterProjection_;

		if (jitter_.LengthSquared() > 0.0f)
		{
			projection_._31 += (jitter_.x * 2.0f) / static_cast<Float>(width_);
			projection_._32 -= (jitter_.y * 2.0f) / static_cast<Float>(height_);
		}

		currentViewProjection_ = view_ * projection_;

		nonJitterViewProjection_ = view_ * nonJitterProjection_;

		inverseView_ = view_.Invert();

		inverseProjection_ = projection_.Invert();

		inverseViewProjection_ = currentViewProjection_.Invert();
	}

	void CanvasCamera::Resize(Float width, Float height)
	{
		Bool resolutionChanged = (width != width_) || (height != height_);

		width_ = width;
		height_ = height;

		baseViewHeight_ = height;

		if (resolutionChanged)
		{
			focus_ = { 100000.0f + width * 0.5f, 100000.0f + height * 0.5f, 100000.0f };
			eye_ = { focus_.x, focus_.y, focus_.z - 10.0f };
		}
	}

	void CanvasCamera::Eye(Vector3 eye)
	{
		eye_ = eye;
	}

	void CanvasCamera::Focus(Vector3 focus)
	{
		focus_ = focus;
	}

	void CanvasCamera::Up(Vector3 up)
	{
		up_ = up;
	}

	void CanvasCamera::Near(Float nearPlane)
	{
		nearPlane_ = nearPlane;
	}

	void CanvasCamera::Far(Float farPlane)
	{
		farPlane_ = farPlane;
	}

	void CanvasCamera::Fov(Float fieldOfView)
	{
		fieldOfView_ = fieldOfView;
	}

	void CanvasCamera::Zoom(Float zoom)
	{
		zoom_ = zoom;
	}

	Vector3 CanvasCamera::Eye()const
	{
		return eye_;
	}

	Vector3 CanvasCamera::Focus()const
	{
		return focus_;
	}

	Vector3 CanvasCamera::Up()const
	{
		return up_;
	}

	Vector3 CanvasCamera::Forward()const
	{
		return forward_;
	}

	Vector3 CanvasCamera::Right()const
	{
		return right_;
	}

	Float CanvasCamera::Near()const
	{
		return nearPlane_;
	}

	Float CanvasCamera::Far()const
	{
		return farPlane_;
	}

	Float CanvasCamera::Fov()const
	{
		return fieldOfView_;
	}

	Float CanvasCamera::Zoom()const
	{
		return zoom_;
	}

	Float CanvasCamera::VisibleHeight()const
	{
		return baseViewHeight_ / zoom_;
	}

	Float CanvasCamera::AspectRatio()const
	{
		return aspectRatio_;
	}

	Float CanvasCamera::Width()const
	{
		return width_;
	}

	Float CanvasCamera::Height()const
	{
		return height_;
	}

	Matrix CanvasCamera::View()const
	{
		return view_;
	}

	Matrix CanvasCamera::InverseView()const
	{
		return inverseView_;
	}

	Matrix CanvasCamera::Projection()const
	{
		return projection_;
	}

	Matrix CanvasCamera::InverseProjection()const
	{
		return inverseProjection_;
	}

	Matrix CanvasCamera::NonJitterProjection()const
	{
		return nonJitterProjection_;
	}

	Matrix CanvasCamera::CurrentViewProjection()const
	{
		return currentViewProjection_;
	}

	Matrix CanvasCamera::PreviousViewProjection()const
	{
		return previousViewProjection_;
	}

	Matrix CanvasCamera::InverseViewProjection()const
	{
		return inverseViewProjection_;
	}

	Matrix CanvasCamera::NonJitterViewProjection()const
	{
		return nonJitterViewProjection_;
	}
}
