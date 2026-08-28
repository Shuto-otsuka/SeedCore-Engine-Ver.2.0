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

		/// [EN] Starts an animated slide of eye_/focus_ so focus_ lands on
		///      focusTarget's XY, keeping the current zoom (the eye-to-focus
		///      offset) and both Z values. Advances during Tick() over
		///      focusDuration_ seconds; calling it again mid-animation
		///      restarts from the current in-flight eye/focus. Any direct
		///      Eye()/Focus() write (the panel's pan or view-reset) cancels it.
		/// [JP] eye_/focus_ をアニメーションさせながらスライドさせ、focus_ を
		///      focusTarget の XY に合わせる。現在のズーム（eye-focus
		///      オフセット）と両者の Z は保つ。Tick() 内で focusDuration_ 秒
		///      かけて進行する。アニメーション中に再度呼ぶと、その時点の
		///      （進行中の）eye/focus から再スタートする。Eye()/Focus() を
		///      直接書き込む（パネルのパン / ビューリセット）と中断される。
		void FocusOn(Vector3 focusTarget);

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

		/// [EN] FocusOn's in-flight animation state — see FocusOn's comment.
		/// [JP] FocusOn の進行中アニメーション状態 — FocusOn のコメント参照。
		Bool focusing_ = false;
		Vector3 focusStartEye_ = { 0,0,0 };
		Vector3 focusStartFocus_ = { 0,0,0 };
		Vector3 focusTargetEye_ = { 0,0,0 };
		Vector3 focusTargetFocus_ = { 0,0,0 };
		Float focusElapsed_ = 0.0f;
		Float focusDuration_ = 0.35f;

		Vector3 up_ = { 0,1,0 };

		Vector3 forward_ = { 0,0,1 };

		Vector3 right_ = { 1,0,0 };

		Float nearPlane_ = 0.001f;

		Float farPlane_ = 1000.0f;

		Float fieldOfView_ = 60.0f;

		Float zoom_ = 1.0f;

		Float baseViewHeight_ = 720.0f;

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

		Float width_ = 0.0f;

		Float height_ = 0.0f;
	};
}