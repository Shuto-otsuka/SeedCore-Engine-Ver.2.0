#pragma once
#include <FoundationEngine/Prelude.h>
#include <PhysicsEngine/Physics/Physics.h>

namespace SeedCore
{
	class Actor;
	class World;
	class ResourceCache;
	struct LoaderSystem;

	/**
	* [EN]
	* Glue between the ECS and the physics world: builds bodies from
	* actors, keeps them in step with actor activity, delivers contact
	* events to components, and finishes components that wait on assets.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* ECS と物理ワールドのつなぎ役。Actor からボディを作り、Actor の有効状態に
	* ボディを合わせ、接触イベントをコンポーネントへ届け、アセット待ちの
	* コンポーネントを仕上げる。
	*/
	class SEEDCORE_API PhysicsSystem
	{
	public:
		/**
		* [EN]
		* Writes the actor's pose and EntityID into desc. Canvas actors (with
		* a Rect/CircleCollider) are converted from pixels to meters on the
		* z = 0 plane; others use their Position/Rotation as is.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* Actor の姿勢と EntityID を desc に書き込む。Canvas の Actor
		* (Rect/CircleCollider 付き)はピクセルからメートルへ変換して z = 0 の
		* 平面に置き、それ以外は Position/Rotation をそのまま使う。
		*/
		static void ApplyTransform(Actor actor, RigidbodyDesc& desc);

		/**
		* [EN]
		* Suspends the bodies of inactive actors and resumes those of active
		* ones, then enables only the joints whose bodies are both present.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 無効な Actor のボディを停止し、有効な Actor のボディを再開したうえで、
		* 両方のボディがそろっているジョイントだけを有効にする。
		*/
		static void ApplyActive(World& world);

		/**
		* [EN]
		* Creates the static body of a collider on its own. Returns an
		* invalid ID when the actor has a Rigidbody, which then owns the
		* single body and borrows the collider's shape instead.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* コライダー単独の静的ボディを作る。Actor に Rigidbody がある場合は無効な
		* ID を返す。その場合はボディを Rigidbody が1つだけ持ち、コライダーの
		* 形状を借りる。
		*/
		static JPH::BodyID CreateColliderBody(Actor actor, Handle<JPH::Shape> shape, Bool isTrigger = false);

		/**
		* [EN]
		* Destroys a collider's own body, if it has one, and releases its
		* reference to the shape.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* コライダー自身のボディがあれば破棄し、形状への参照を解放する。
		*/
		static void DestroyColliderBody(Actor actor, JPH::BodyID bodyID, Handle<JPH::Shape> shape);

	public:
		/**
		* [EN]
		* Tells every component of the entity that a collision with the other
		* entity began.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* エンティティの全コンポーネントへ、相手のエンティティとの衝突が
		* 始まったことを伝える。
		*/
		static void DispatchCollisionEnter(World& world, EntityID entityID, EntityID otherEntityID);

		/**
		* [EN]
		* Tells every component of the entity that a collision with the other
		* entity continues.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* エンティティの全コンポーネントへ、相手のエンティティとの衝突が
		* 続いていることを伝える。
		*/
		static void DispatchCollisionStay(World& world, EntityID entityID, EntityID otherEntityID);

		/**
		* [EN]
		* Tells every component of the entity that a collision with the other
		* entity ended.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* エンティティの全コンポーネントへ、相手のエンティティとの衝突が
		* 終わったことを伝える。
		*/
		static void DispatchCollisionExit(World& world, EntityID entityID, EntityID otherEntityID);

		/**
		* [EN]
		* Tells every component of the entity that the other entity entered a
		* trigger.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* エンティティの全コンポーネントへ、相手のエンティティがトリガーに
		* 入ったことを伝える。
		*/
		static void DispatchTriggerEnter(World& world, EntityID entityID, EntityID otherEntityID);

		/**
		* [EN]
		* Tells every component of the entity that the other entity stays in a
		* trigger.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* エンティティの全コンポーネントへ、相手のエンティティがトリガーの中に
		* 留まっていることを伝える。
		*/
		static void DispatchTriggerStay(World& world, EntityID entityID, EntityID otherEntityID);

		/**
		* [EN]
		* Tells every component of the entity that the other entity left a
		* trigger.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* エンティティの全コンポーネントへ、相手のエンティティがトリガーから
		* 出たことを伝える。
		*/
		static void DispatchTriggerExit(World& world, EntityID entityID, EntityID otherEntityID);

	public:
		/**
		* [EN]
		* Builds every MeshCollider still waiting for its collision mesh,
		* once the asset has finished loading. Called every frame.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 衝突メッシュを待っている MeshCollider を、アセットの読み込みが
		* 終わったものから構築する。毎フレーム呼ばれる。
		*/
		static void ResolveMeshCollider(LoaderSystem& loader, ResourceCache& cache, World& world);

		/**
		* [EN]
		* Builds every Softbody still waiting for the model of its actor's
		* Mesh, once the model has finished loading. Called every frame.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* Actor の Mesh のモデルを待っている Softbody を、モデルの読み込みが
		* 終わったものから構築する。毎フレーム呼ばれる。
		*/
		static void ResolveSoftbody(LoaderSystem& loader, ResourceCache& cache, World& world);
	};
}
