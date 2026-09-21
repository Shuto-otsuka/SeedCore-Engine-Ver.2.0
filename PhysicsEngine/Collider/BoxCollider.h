#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/SeedScript.h>
#include <FoundationEngine/Utility/Handle.h>

namespace SeedCore
{
	/**
	* [EN]
	* Component that creates a box-shaped collider body for its actor.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* アクター用の箱型コライダーボディを生成するコンポーネント。
	*/
	class SEEDCORE_API BoxCollider :public SeedScript
	{
	public:
		/// [EN] Full dimensions of the box along each local axis.
		/// [JP] 各ローカル軸に沿った箱の全寸法。
		SC_REFLECTION_FIELD_EX("サイズ")
		Vector3 size_ = { 1.0f, 1.0f, 1.0f };

		/// [EN] Local offset of the box center from the actor origin.
		/// [JP] アクター原点から見た箱の中心のローカルオフセット。
		SC_REFLECTION_FIELD_EX("中心オフセット")
		Vector3 center_ = { 0.0f, 0.0f, 0.0f };

		/// [EN] Whether the collider reports overlaps without physical response.
		/// [JP] 物理応答を行わず重なりを通知するトリガーとして扱うか。
		SC_REFLECTION_FIELD_EX("トリガー")
		Bool isTrigger_ = false;

	public:
		/**
		* [EN]
		* Creates the box shape and collider body.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 箱形状とコライダーボディを生成する。
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
		* Creates and returns a box shape from the current settings.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 現在の設定から箱形状を生成して返す。
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
	REGISTER_COMPONENT(BoxCollider, "Collider");
}
