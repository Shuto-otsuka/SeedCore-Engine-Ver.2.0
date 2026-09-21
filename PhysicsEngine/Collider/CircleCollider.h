#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/SeedScript.h>
#include <FoundationEngine/Utility/Handle.h>

namespace SeedCore
{
	/**
	* [EN]
	* Component that creates a circular 2D collider body for its actor.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* アクター用の円形 2D コライダーボディを生成するコンポーネント。
	*/
	class SEEDCORE_API CircleCollider :public SeedScript
	{
	public:
		/// [EN] Radius of the circle.
		/// [JP] 円の半径。
		SC_REFLECTION_CLAMPED_EX("半径", 1.0f, 10000.0f)
		Float radius_ = 50.0f;

		/// [EN] Local offset of the circle center.
		/// [JP] 円の中心のローカルオフセット。
		SC_REFLECTION_FIELD_EX("中心オフセット")
		Vector2 center_ = { 0.0f, 0.0f };

		/// [EN] Whether this collider acts as a trigger.
		/// [JP] このコライダーをトリガーとして扱うか。
		SC_REFLECTION_FIELD_EX("トリガー")
		Bool isTrigger_ = false;

	public:
		/**
		* [EN]
		* Creates the circle shape and collider body.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 円形状とコライダーボディを生成する。
		*/
		void OnAwake();

		/**
		* [EN]
		* Destroys the collider body and releases its shape.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* コライダーボディを破棄し、形状を解放する。
		*/
		void OnDestroy();

	public:
		/**
		* [EN]
		* Creates and returns a circle shape from the current settings.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 現在の設定から円形状を生成して返す。
		*/
		Handle<JPH::Shape> GetShapeHandle()const;

	private:
		/// [EN] Jolt body identifier of the generated collider body.
		/// [JP] 生成したコライダーボディの Jolt ボディ ID。
		JPH::BodyID bodyID_;

		/// [EN] Pool handle of the attached shape.
		/// [JP] 取り付けた形状のプールハンドル。
		Handle<JPH::Shape> shapeHandle_;
	};
	REGISTER_COMPONENT(CircleCollider, "Collider");
}
