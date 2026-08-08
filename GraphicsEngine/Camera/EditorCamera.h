#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	class SEEDCORE_API EditorCamera
	{
	public:
		void Tick(Float elapsedTime);

		void Resize(Float width, Float height);

		void Eye(Vector3 eye);

		void Focus(Vector3 focus);

		/// [EN] Starts an animated slide of eye_/focus_ so focus_ lands on
		///      targetPosition, keeping the camera's current look direction.
		///      If radius > 0, also dollies to a distance that fits a sphere
		///      of that radius in view (Unreal-style "frame selected" bounds
		///      fit); radius <= 0 (the default) instead keeps the current
		///      eye-to-focus distance (pan only, no dolly — used when no
		///      Bounds is available for the target). Advances during Tick()
		///      over focusDuration_ seconds; calling this again mid-animation
		///      restarts it from the camera's current (in-flight) eye/focus.
		/// [JP] eye_/focus_ をアニメーションさせながらスライドさせ、focus_ を
		///      targetPosition に合わせる。現在のカメラの向きは保ったまま。
		///      radius > 0 なら、その半径の球が画角に収まる距離までドリー
		///      もする（Unreal風の「選択対象にフレーム」のバウンズフィット）。
		///      radius <= 0（デフォルト）なら現在の eye-focus 間距離を保つ
		///      だけ（パンのみ、ドリーなし — 対象に Bounds が無い場合に使う）。
		///      Tick() 内で focusDuration_ 秒かけて進行する。アニメーション中に
		///      再度呼ぶと、その時点の（進行中の）eye/focus から再スタートする。
		void FocusOn(Vector3 targetPosition, Float radius = 0.0f);

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

		Matrix PreviousNonJitterViewProjection()const;

		Matrix InverseViewProjection()const;

		Matrix NonJitterViewProjection()const;

	private:
		Vector3 eye_ = { 0,0,-10 };

		Vector3 focus_ = { 0,0,0 };

		Vector3 up_ = { 0,1,0 };

		Vector3 forward_ = { 0,0,1 };

		Vector3 right_ = { 1,0,0 };

		/// [EN] FocusOn's in-flight animation state — see FocusOn's comment.
		/// [JP] FocusOn の進行中アニメーション状態 — FocusOn のコメント参照。
		Bool focusing_ = false;
		Vector3 focusStartEye_ = { 0,0,0 };
		Vector3 focusStartFocus_ = { 0,0,0 };
		Vector3 focusTargetEye_ = { 0,0,0 };
		Vector3 focusTargetFocus_ = { 0,0,0 };
		Float focusElapsed_ = 0.0f;
		Float focusDuration_ = 0.35f;

		Float nearPlane_ = 0.001f;

		Float farPlane_ = 1000.0f;

		Float fieldOfView_ = 60.0f;

		Float aspectRatio_ = 16.0f / 9.0f;

		Vector2 jitter_ = { 0.5f,0.5f };

		Matrix view_ = Matrix::Identity;

		Matrix inverseView_ = Matrix::Identity;

		Matrix projection_ = Matrix::Identity;

		Matrix inverseProjection_ = Matrix::Identity;

		Matrix nonJitterProjection_ = Matrix::Identity;

		Matrix currentViewProjection_ = Matrix::Identity;

		Matrix previousViewProjection_ = Matrix::Identity;

		Matrix previousNonJitterViewProjection_ = Matrix::Identity;

		Matrix inverseViewProjection_ = Matrix::Identity;

		Matrix nonJitterViewProjection_ = Matrix::Identity;

		Float width_ = 1280.0f;

		Float height_ = 720.0f;
	};
}