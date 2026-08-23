#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/Math/Ray.h>

namespace SeedCore
{
	/**
	* [EN]
	* Converts between screen-pixel coordinates and world space, always
	* against the game's own active Camera (the one CameraSystem
	* computes each frame from the ECS's Camera component) - never the
	* Editor's own free-fly tool camera. Callers (SeedScript/UserProject
	* code) never pass in a view/projection/viewport size: CameraSystem
	* pushes the current frame's values in via SetCurrentView() before
	* gameplay code runs, so ScreenToWorld()/WorldToScreen() behave
	* identically whether called from the Editor's ゲームビュー preview
	* or the standalone Runtime - there is no "which view" to specify.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* スクリーンのピクセル座標とワールド空間を変換する。常にゲーム自身の
	* アクティブな Camera(CameraSystem が毎フレーム ECS の Camera
	* コンポーネントから計算するもの)を基準にする - Editor 自身の
	* フリーカメラ(ツールカメラ)は対象にしない。呼び出し側
	* (SeedScript/UserProject のコード)は view/projection/ビューポート
	* サイズを一切渡さない - CameraSystem がゲームプレイコードの実行前に
	* SetCurrentView() でその時点の値を反映させるため、
	* ScreenToWorld()/WorldToScreen() は Editor の ゲームビュー プレビュー
	* から呼んでも、単体の Runtime から呼んでも同じ挙動になる -
	* 「どちらのビューか」を指定する必要が無い。
	*/
	class CameraSystem;

	class SEEDCORE_API ScreenSpace
	{
		friend class CameraSystem;

	public:
		/**
		* [EN]
		* Converts pixelPosition (screen pixels, origin top-left) into a
		* world-space Ray from the current camera's near plane through
		* pixelPosition, suitable for passing straight into
		* Physics::Raycast()/Spherecast().
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* pixelPosition(スクリーンピクセル、原点は左上)を、現在のカメラの
		* 近平面から pixelPosition を通るワールド空間の Ray へ変換する。
		* Physics::Raycast()/Spherecast() にそのまま渡せる。
		*/
		static Ray ScreenToWorld(const Vector2& pixelPosition);

		/**
		* [EN]
		* Converts worldPosition into screen pixels (origin top-left)
		* under the current camera.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* worldPosition を、現在のカメラでのスクリーンピクセル(原点は左上)
		* へ変換する。
		*/
		static Vector2 WorldToScreen(const Vector3& worldPosition);

	private:
		/**
		* [EN]
		* Called by CameraSystem once per frame (after computing the
		* game's active Camera's view/projection) to publish the values
		* ScreenToWorld()/WorldToScreen() use. Private + friended to
		* CameraSystem rather than merely documented as internal-only, so
		* gameplay code can't accidentally feed it an arbitrary view.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* CameraSystem が毎フレーム(ゲームのアクティブな Camera の
		* view/projection を計算した後に)呼び出し、
		* ScreenToWorld()/WorldToScreen() が使う値を公開する。
		* 「内部専用」とドキュメントで言うだけでなく、private化して
		* CameraSystem だけを friend にすることで、ゲームプレイコードが
		* 誤って任意の view を流し込めないようにする。
		*/
		static void SetCurrentView(const Matrix& view, const Matrix& projection, const Vector2& screenSize);

	private:
		static Matrix view_;

		static Matrix projection_;

		static Vector2 screenSize_;
	};
}
