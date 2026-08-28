#include <GraphicsEngine/Camera/CanvasCamera.h>

namespace SeedCore
{
	void CanvasCamera::Tick(Float elapsedTime)
	{
		previousViewProjection_ = currentViewProjection_;

		if (focusing_)
		{
			focusElapsed_ += elapsedTime;
			Float t = Clamp(focusElapsed_ / focusDuration_, 0.0f, 1.0f);
			Float smoothT = t * t * (3.0f - 2.0f * t);

			eye_ = Vector3::Lerp(focusStartEye_, focusTargetEye_, smoothT);
			focus_ = Vector3::Lerp(focusStartFocus_, focusTargetFocus_, smoothT);

			if (t >= 1.0f)
			{
				focusing_ = false;
			}
		}

		view_ = Matrix::CreateLookAt(eye_, focus_, up_);

		aspectRatio_ = width_ / height_;

		Float orthoHeight = baseViewHeight_ / zoom_;
		Float orthoWidth = orthoHeight * aspectRatio_;

		nonJitterProjection_ = Matrix::CreateOrthographic(orthoWidth, orthoHeight, nearPlane_, farPlane_);

		nonJitterProjection_._33 = 1.0f / (nearPlane_ - farPlane_);
		nonJitterProjection_._43 = farPlane_ / (farPlane_ - nearPlane_);

		projection_ = nonJitterProjection_;

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
			focus_ = Vector3(100000.0f + width * 0.5f, 100000.0f + height * 0.5f, 100000.0f);
			eye_ = Vector3(focus_.x, focus_.y, focus_.z - 10.0f);
			focusing_ = false;
		}
	}

	void CanvasCamera::Eye(Vector3 eye)
	{
		eye_ = eye;
		focusing_ = false;
	}

	void CanvasCamera::Focus(Vector3 focus)
	{
		focus_ = focus;
		focusing_ = false;
	}

	void CanvasCamera::FocusOn(Vector3 focusTarget)
	{
		/// [EN] Keep the current eye-to-focus offset (zoom / direction) and both Z values; only the XY of eye_/focus_ slides. Tick() runs the interpolation.
		/// [JP] 現在の eye-focus オフセット（ズーム / 向き）と両者の Z を保ち、eye_/focus_ の XY だけをスライドさせる。補間は Tick() が行う。
		Vector3 offset = eye_ - focus_;

		focusStartEye_ = eye_;
		focusStartFocus_ = focus_;
		focusTargetFocus_ = Vector3(focusTarget.x, focusTarget.y, focus_.z);
		focusTargetEye_ = Vector3(focusTarget.x + offset.x, focusTarget.y + offset.y, eye_.z);

		focusElapsed_ = 0.0f;
		focusing_ = true;
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
