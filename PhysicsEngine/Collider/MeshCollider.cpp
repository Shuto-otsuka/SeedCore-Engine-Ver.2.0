#include <PhysicsEngine/Collider/MeshCollider.h>
#include <PhysicsEngine/Physics/Physics.h>
#include <PhysicsEngine/Physics/PhysicsSystem.h>
#include <PhysicsEngine/Rigidbody/Rigidbody.h>
#include <GraphicsEngine/Model/Collision/MeshCollision.h>
#include <FoundationEngine/ECS/World.h>
#include <FoundationEngine/ECS/Actor.h>
#include <FoundationEngine/Log/Warning.h>

namespace SeedCore
{
	/**
	* [EN]
	* Marks the configured mesh asset for shape construction.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 設定されたメッシュアセットを形状構築待ちにする。
	*/
	void MeshCollider::OnAwake()
	{
		pending_ = meshID_ != 0;
	}

	/**
	* [EN]
	* Destroys the standalone collider body and releases its shape.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 単独のコライダーボディを破棄し、形状を解放する。
	*/
	void MeshCollider::OnDestroy()
	{
		if (shapeHandle_.empty())
		{
			return;
		}

		PhysicsSystem::DestroyColliderBody(GetActor(), bodyID_, shapeHandle_);

		bodyID_ = JPH::BodyID();
		shapeHandle_ = Handle<JPH::Shape>::null();
		pending_ = meshID_ != 0;
	}

	/**
	* [EN]
	* Builds and applies a collision shape from resolved mesh data.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 解決済みメッシュデータから衝突形状を構築して適用する。
	*/
	void MeshCollider::Build(const MeshCollision& meshCollision)
	{
		Actor actor = GetActor();
		Rigidbody* rigidbody = actor.GetComponent<Rigidbody>();

		/// [EN] A Rigidbody whose body is not created yet cannot take the shape; stay pending and retry next frame.
		/// [JP] ボディがまだ無い Rigidbody には形状を渡せないので、構築待ちのまま次のフレームで再試行する。
		if (rigidbody && rigidbody->BodyID().IsInvalid())
		{
			return;
		}

		/// [EN] Dynamic rigid bodies require a convex collision representation.
		/// [JP] 動的 Rigidbody には凸形状の衝突表現を使用する。
		Bool convex = convex_;
		if (!convex && rigidbody && rigidbody->bodyType_ == Rigidbody::BodyType::Dynamic)
		{
			SC_LOG_WARNING("MeshCollider: Dynamic な Rigidbody には凹メッシュ形状を使えないため凸包で生成します (assetID: %u)", meshID_);
			convex = true;
		}

		/// [EN] Build either a convex hull or triangle mesh from the resolved asset data.
		/// [JP] 解決済みアセットデータから凸包または三角形メッシュを構築する。
		shapeHandle_ = convex ? actor.GetPhysics().CreateConvexShape(meshID_, meshCollision.Positions()) : actor.GetPhysics().CreateMeshShape(meshID_, meshCollision.Positions(), meshCollision.Indices());

		if (shapeHandle_.empty())
		{
			return;
		}

		pending_ = false;

		/// [EN] Replace an existing rigid-body shape; otherwise create a standalone collider body.
		/// [JP] 既存の Rigidbody 形状を置き換え、なければ単独のコライダーボディを生成する。
		if (rigidbody)
		{
			actor.GetPhysics().BodyShape(rigidbody->BodyID(), shapeHandle_);
			return;
		}

		bodyID_ = PhysicsSystem::CreateColliderBody(actor, shapeHandle_, isTrigger_);
	}

	/**
	* [EN]
	* Reports whether the configured mesh still needs to be built.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 設定されたメッシュをまだ構築する必要があるかを返す。
	*/
	Bool MeshCollider::Pending()const
	{
		return pending_;
	}
}
