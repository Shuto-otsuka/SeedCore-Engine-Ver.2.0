#include <GraphicsEngine/Camera/ScreenSpace.h>

namespace SeedCore
{
	Matrix ScreenSpace::view_ = Matrix::Identity;
	Matrix ScreenSpace::projection_ = Matrix::Identity;
	Vector2 ScreenSpace::screenSize_ = Vector2(1.0f, 1.0f);

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
	Ray ScreenSpace::ScreenToWorld(const Vector2& pixelPosition)
	{
		/// [EN] Pixel -> NDC: X maps [0, screenSize_.x] to [-1, 1], Y maps
		///      [0, screenSize_.y] to [1, -1] (screen Y grows downward,
		///      NDC Y grows upward).
		/// [JP] ピクセル→NDC: Xは[0, screenSize_.x]を[-1, 1]へ、Yは
		///      [0, screenSize_.y]を[1, -1]へ写す(スクリーンYは下向き、
		///      NDCのYは上向きに増えるため)。
		Float ndcX = (pixelPosition.x / screenSize_.x) * 2.0f - 1.0f;
		Float ndcY = 1.0f - (pixelPosition.y / screenSize_.y) * 2.0f;

		Matrix inverseViewProjection = (view_ * projection_).Invert();

		/// [EN] Vector3::Transform divides by w (unlike Vector4::Transform),
		///      so these already come back as ordinary world-space points -
		///      one on the near plane, one on the far plane, both under
		///      the same (ndcX, ndcY) screen column.
		/// [JP] Vector3::Transform は(Vector4::Transformと違って)wで
		///      除算するため、これらは既に通常のワールド空間の点として
		///      返ってくる - 同じ(ndcX, ndcY)のスクリーン列上の、近平面上の
		///      点と遠平面上の点。
		Vector3 nearPoint = Vector3::Transform(Vector3(ndcX, ndcY, 0.0f), inverseViewProjection);
		Vector3 farPoint = Vector3::Transform(Vector3(ndcX, ndcY, 1.0f), inverseViewProjection);

		Ray ray;
		ray.origin_ = nearPoint;
		ray.direction_ = farPoint - nearPoint;
		if (ray.direction_.LengthSquared() > 0.0f)
		{
			ray.direction_.Normalize();
		}
		return ray;
	}

	/**
	* [EN]
	* Converts worldPosition into screen pixels (origin top-left) under
	* the current camera.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* worldPosition を、現在のカメラでのスクリーンピクセル(原点は左上)へ
	* 変換する。
	*/
	Vector2 ScreenSpace::WorldToScreen(const Vector3& worldPosition)
	{
		Vector3 clipPosition = Vector3::Transform(worldPosition, view_ * projection_);

		Float pixelX = (clipPosition.x * 0.5f + 0.5f) * screenSize_.x;
		Float pixelY = (1.0f - (clipPosition.y * 0.5f + 0.5f)) * screenSize_.y;
		return Vector2(pixelX, pixelY);
	}

	/**
	* [EN]
	* Called by CameraSystem once per frame (after computing the game's
	* active Camera's view/projection) to publish the values
	* ScreenToWorld()/WorldToScreen() use - not meant to be called from
	* gameplay code.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* CameraSystem が毎フレーム(ゲームのアクティブな Camera の
	* view/projection を計算した後に)呼び出し、
	* ScreenToWorld()/WorldToScreen() が使う値を公開する - ゲームプレイ
	* コードから呼ぶことは想定していない。
	*/
	void ScreenSpace::SetCurrentView(const Matrix& view, const Matrix& projection, const Vector2& screenSize)
	{
		view_ = view;
		projection_ = projection;
		screenSize_ = screenSize;
	}
}
