#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/SeedScript.h>
#include <FoundationEngine/Utility/Handle.h>

namespace SeedCore
{
	class MeshCollision;

	/**
	* [EN]
	* Component that builds a collider from a mesh-collision asset and either
	* attaches it to a rigid body or creates a standalone collider body.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* メッシュコリジョンアセットから形状を構築し、Rigidbody へ取り付けるか、
	* 単独のコライダーボディを生成するコンポーネント。
	*/
	class SEEDCORE_API MeshCollider :public SeedScript
	{
	private:
		friend class PhysicsSystem;

	public:
		/// [EN] Asset ID of the mesh-collision data used to build the shape.
		/// [JP] 形状の構築に使うメッシュコリジョンデータのアセット ID。
		SC_PAYLOAD_FIELD_EX("コリジョンメッシュ", MeshCollision)
		Uint32 meshID_ = 0;

		/// [EN] Whether the mesh is converted to a convex hull.
		/// [JP] メッシュを凸包へ変換するか。
		SC_REFLECTION_FIELD_EX("凸包にする")
		Bool convex_ = false;

		/// [EN] Whether a standalone collider body acts as a trigger.
		/// [JP] 単独のコライダーボディをトリガーとして扱うか。
		SC_REFLECTION_FIELD_EX("トリガー")
		Bool isTrigger_ = false;

	public:
		/**
		* [EN]
		* Marks the configured mesh asset for shape construction.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 設定されたメッシュアセットを形状構築待ちにする。
		*/
		void OnAwake();

		/**
		* [EN]
		* Destroys the standalone collider body and releases its shape.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 単独のコライダーボディを破棄し、形状を解放する。
		*/
		void OnDestroy();

	private:
		/**
		* [EN]
		* Builds and applies a collision shape from resolved mesh data.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 解決済みメッシュデータから衝突形状を構築して適用する。
		*/
		void Build(const MeshCollision& meshCollision);

		/**
		* [EN]
		* Reports whether the configured mesh still needs to be built.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 設定されたメッシュをまだ構築する必要があるかを返す。
		*/
		Bool Pending()const;

	private:
		/// [EN] Jolt body identifier used when this collider owns a standalone body.
		/// [JP] このコライダーが単独ボディを所有するときに使う Jolt ボディ ID。
		JPH::BodyID bodyID_;

		/// [EN] Pool handle of the built collision shape.
		/// [JP] 構築した衝突形状のプールハンドル。
		Handle<JPH::Shape> shapeHandle_;

		/// [EN] Whether mesh data must be resolved and built.
		/// [JP] メッシュデータの解決と構築が必要か。
		Bool pending_ = true;
	};
	REGISTER_COMPONENT(MeshCollider, "Collider");
}
