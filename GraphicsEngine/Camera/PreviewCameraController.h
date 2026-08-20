#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	class PreviewCamera;

	/**
	* [EN]
	* Orbit-style controller for PreviewCamera: left-click drag orbits the eye
	* around Focus() at a fixed distance (yaw/pitch stored directly, world-up
	* locked - no roll), middle-click drag pans Focus() itself, and the wheel
	* dollies the distance in/out. Mirrors the interaction model of common
	* asset-preview viewports (Unity's Scene/model preview, Unreal's Static
	* Mesh Editor viewport) rather than EditorCameraController's WASD fly cam,
	* since a preview viewport is always looking at one object rather than
	* flying through a scene.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* PreviewCamera 向けのオービット式コントローラー: 左クリックドラッグで
	* Focus() を中心に一定距離を保ったままeyeを回転(yaw/pitchを直接保持、
	* world-up固定でロールなし)、中クリックドラッグでFocus()自体をパン、
	* ホイールで距離をドリーする。EditorCameraControllerのWASDフライカメラ
	* ではなく、Unity のシーン/モデルプレビューや Unreal の Static Mesh
	* Editor ビューポートのような、一般的なアセットプレビュー用の操作系に
	* 合わせている — プレビュービューポートはシーンを飛び回るのではなく
	* 常に1つの対象を見るものであるため。
	*/
	class SEEDCORE_API PreviewCameraController
	{
	public:
		void Update(PreviewCamera& camera, Float deltaTime);

		void RotateSpeed(Float speed);

		void ZoomSpeed(Float speed);

		void PanSpeed(Float speed);

		Float RotateSpeed()const;

		Float ZoomSpeed()const;

		Float PanSpeed()const;

	private:
		/// [EN] Orbit angles (degrees) and eye-to-focus distance, seeded from
		///      the camera's current Eye()/Focus() on the first Update() call.
		/// [JP] オービット角度(度)とeye-focus間距離。最初のUpdate()呼び出しで
		///      カメラの現在のEye()/Focus()から初期化される。
		Float yaw_ = 0.0f;

		Float pitch_ = 0.0f;

		Float distance_ = 1.0f;

		Bool initialized_ = false;

		Float rotateSpeed_ = 0.3f;

		Float zoomSpeed_ = 0.1f;

		Float panSpeed_ = 0.0025f;
	};
}
