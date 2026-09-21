#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/SeedScript.h>
#include <FoundationEngine/Utility/Handle.h>

namespace SeedCore
{
	/**
	* [EN]
	* Component that creates a capsule-shaped collider body for its actor.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* アクター用のカプセル型コライダーボディを生成するコンポーネント。
	*/
	class SEEDCORE_API CapsuleCollider :public SeedScript
	{
	public:
		/// [EN] Total height of the capsule.
		/// [JP] カプセル全体の高さ。
		SC_REFLECTION_CLAMPED_EX("高さ", 0.001f, 100.0f)
		Float height_ = 2.0f;

		/// [EN] Radius of the capsule.
		/// [JP] カプセルの半径。
		SC_REFLECTION_CLAMPED_EX("半径", 0.001f, 100.0f)
		Float radius_ = 0.5f;

		/// [EN] Whether the collider reports overlaps without physical response.
		/// [JP] 物理応答を行わず重なりを通知するトリガーとして扱うか。
		SC_REFLECTION_FIELD_EX("トリガー")
		Bool isTrigger_ = false;

	public:
		/**
		* [EN]
		* Creates the capsule shape and collider body.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* カプセル形状とコライダーボディを生成する。
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
		* Creates and returns a capsule shape from the current settings.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 現在の設定からカプセル形状を生成して返す。
		*/
		Handle<JPH::Shape> GetShapeHandle()const;

	private:
		/// [EN] Jolt body identifier of the generated collider body.
		/// [JP] 生成したコライダーボディの Jolt ボディ ID。
		JPH::BodyID bodyID_;

		/// [EN] Pool handle of the shape attached to the collider body.
		/// [JP] コライダーボディへ取り付けた形状のプールハンドル。
		Handle<JPH::Shape> shapeHandle_;
	};
	REGISTER_COMPONENT(CapsuleCollider, "Collider");
}
