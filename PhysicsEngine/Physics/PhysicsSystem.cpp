#include <PhysicsEngine/Physics/PhysicsSystem.h>
#include <PhysicsEngine/Rigidbody/Rigidbody.h>
#include <PhysicsEngine/JoltPhysics/JoltLayerdef.h>
#include <PhysicsEngine/Collider/RectCollider.h>
#include <PhysicsEngine/Collider/CircleCollider.h>
#include <PhysicsEngine/Collider/MeshCollider.h>
#include <PhysicsEngine/Softbody/Softbody.h>
#include <FoundationEngine/ECS/World.h>
#include <FoundationEngine/ECS/Actor.h>
#include <FoundationEngine/ECS/Component/Position.h>
#include <FoundationEngine/ECS/Component/Rotation.h>
#include <FoundationEngine/Resource/ResourceCache.h>
#include <GraphicsEngine/Model/Collision/MeshCollisionResource.h>
#include <GraphicsEngine/Model/ModelResource.h>
#include <GraphicsEngine/Model/Crister.h>
#include <GraphicsEngine/Model/Mesh.h>

namespace SeedCore
{
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
	void PhysicsSystem::ApplyTransform(Actor actor, RigidbodyDesc& desc)
	{
		/// [EN] An actor without Position or Rotation is placed at the origin with no rotation.
		/// [JP] Position や Rotation を持たない Actor は、原点に回転無しで置く。
		const Position* position = actor.GetComponent<Position>();
		const Rotation* rotation = actor.GetComponent<Rotation>();

		/// [EN] A Rect or Circle collider marks the actor as a 2D canvas actor.
		/// [JP] Rect か Circle のコライダーがあれば、2D Canvas の Actor として扱う。
		if (actor.GetComponent<RectCollider>() || actor.GetComponent<CircleCollider>())
		{
			/// [EN] Number of canvas pixels in one physics meter.
			/// [JP] 物理の1メートルに当たる Canvas のピクセル数。
			static constexpr Float pixelsPerMeter = 100.0f;

			/// [EN] Canvas Y points down and physics Y up, so Y and the Z rotation (stored in Rotation::x_) are negated.
			/// [JP] Canvas の Y は下向き、物理の Y は上向きなので、Y と Z 軸回転(Rotation::x_ に入っている)の符号を反転する。
			desc.position_ = position ? Vector3{ position->x_ / pixelsPerMeter, -position->y_ / pixelsPerMeter, 0.0f } : Vector3{ 0.0f, 0.0f, 0.0f };
			desc.rotation_ = rotation ? Quaternion::CreateFromAxisAngle(Vector3::UnitZ, -ToRadians(rotation->x_)) : Quaternion::Identity;
		}
		else
		{
			/// [EN] Rotation holds Euler angles in degrees: x pitch, y yaw, z roll.
			/// [JP] Rotation は度単位のオイラー角で、x がピッチ、y がヨー、z がロール。
			desc.position_ = position ? Vector3{ position->x_, position->y_, position->z_ } : Vector3{ 0.0f, 0.0f, 0.0f };
			desc.rotation_ = rotation ? Quaternion::CreateFromYawPitchRoll(ToRadians(rotation->y_), ToRadians(rotation->x_), ToRadians(rotation->z_)) : Quaternion::Identity;
		}

		/// [EN] Lets physics queries and contacts find their way back to the actor.
		/// [JP] 物理クエリや接触から Actor へたどり着けるようにする。
		desc.userData_ = actor.GetEntity().GetID();
	}

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
	void PhysicsSystem::ApplyActive(World& world)
	{
		/// [EN] A world without a physics facade has no bodies to update.
		/// [JP] 物理ファサードを持たないワールドには、更新するボディが無い。
		if (!world.GetPhysics())
		{
			return;
		}

		Physics& physics = *world.GetPhysics();

		/// [EN] Walks every body, suspended ones included, so a reactivated actor gets its body back.
		/// [JP] 停止中も含めた全ボディを回り、再び有効になった Actor にボディを戻す。
		for (JPH::BodyID bodyID : physics.BodyList())
		{
			/// [EN] Bodies with no living actor behind them are left alone.
			/// [JP] 生きている Actor が背後にいないボディには触れない。
			Actor actor = world.GetActor(physics.BodyEntityID(bodyID));
			if (!actor)
			{
				continue;
			}

			/// [EN] Both calls do nothing when the body is already in the wanted state.
			/// [JP] ボディが既に望む状態なら、どちらの呼び出しも何もしない。
			if (actor.GetActive())
			{
				physics.ResumeBody(bodyID);
			}
			else
			{
				physics.SuspendBody(bodyID);
			}
		}

		/// [EN] Joints follow the bodies, so they are refreshed after all bodies are settled.
		/// [JP] ジョイントはボディに従うので、全ボディが決まってから更新する。
		physics.RefleshJoint();
	}

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
	JPH::BodyID PhysicsSystem::CreateColliderBody(Actor actor, Handle<JPH::Shape> shape, Bool isTrigger)
	{
		/// [EN] One actor gets one body; with a Rigidbody present, that body is the Rigidbody's.
		/// [JP] 1つの Actor にボディは1つ。Rigidbody があれば、そのボディは Rigidbody のもの。
		if (actor.GetComponent<Rigidbody>())
		{
			return JPH::BodyID();
		}

		/// [EN] A lone collider never moves, so it is a static body on the actor's layer.
		/// [JP] 単独のコライダーは動かないので、Actor のレイヤー上の静的ボディにする。
		RigidbodyDesc desc;
		desc.shape_ = shape;
		desc.motionType_ = JPH::EMotionType::Static;
		desc.layer_ = Layers::Pack(Layers::STATIC, actor.GetLayer());

		/// [EN] Canvas colliders only collide with other canvas bodies.
		/// [JP] Canvas のコライダーは、他の Canvas のボディとだけ衝突する。
		if (actor.GetComponent<RectCollider>() || actor.GetComponent<CircleCollider>())
		{
			desc.layer_ |= Layers::PLANAR;
		}

		/// [EN] A trigger reports overlaps but does not push anything.
		/// [JP] トリガーは重なりを知らせるだけで、何も押し返さない。
		desc.isSensor_ = isTrigger;
		ApplyTransform(actor, desc);

		return actor.GetPhysics().CreateRigidbody(desc);
	}

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
	void PhysicsSystem::DestroyColliderBody(Actor actor, JPH::BodyID bodyID, Handle<JPH::Shape> shape)
	{
		/// [EN] A collider under a Rigidbody has no body of its own.
		/// [JP] Rigidbody の下にあるコライダーは、自分のボディを持たない。
		if (!bodyID.IsInvalid())
		{
			actor.GetPhysics().DestroyBody(bodyID);
		}

		/// [EN] The shape is released either way, since the collider holds its own reference.
		/// [JP] コライダーは自分の参照を持っているので、形状はどちらの場合も解放する。
		actor.GetPhysics().ReleaseShape(shape);
	}

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
	void PhysicsSystem::DispatchCollisionEnter(World& world, EntityID entityID, EntityID otherEntityID)
	{
		/// [EN] Either actor may have been destroyed between the contact and its delivery.
		/// [JP] 接触から配送までの間に、どちらかの Actor が破棄されていることがある。
		Actor actor = world.GetActor(entityID);
		Actor otherActor = world.GetActor(otherEntityID);
		if (!actor || !otherActor)
		{
			return;
		}

		/// [EN] Every ComponentBase-derived component on the entity receives the event with the other entity.
		/// [JP] エンティティの、ComponentBase を継承した全コンポーネントが、相手のエンティティと一緒にイベントを受け取る。
		Entity otherEntity = otherActor.GetEntity();
		for (ComponentID id : actor.ComponentBaseIDList())
		{
			if (ComponentBase* component = reinterpret_cast<ComponentBase*>(world.GetComponent(entityID, id)))
			{
				component->DispatchCollisionEnter(otherEntity);
			}
		}
	}
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
	void PhysicsSystem::DispatchCollisionStay(World& world, EntityID entityID, EntityID otherEntityID)
	{
		/// [EN] Either actor may have been destroyed between the contact and its delivery.
		/// [JP] 接触から配送までの間に、どちらかの Actor が破棄されていることがある。
		Actor actor = world.GetActor(entityID);
		Actor otherActor = world.GetActor(otherEntityID);
		if (!actor || !otherActor)
		{
			return;
		}

		/// [EN] Every ComponentBase-derived component on the entity receives the event with the other entity.
		/// [JP] エンティティの、ComponentBase を継承した全コンポーネントが、相手のエンティティと一緒にイベントを受け取る。
		Entity otherEntity = otherActor.GetEntity();
		for (ComponentID id : actor.ComponentBaseIDList())
		{
			if (ComponentBase* component = reinterpret_cast<ComponentBase*>(world.GetComponent(entityID, id)))
			{
				component->DispatchCollisionStay(otherEntity);
			}
		}
	}
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
	void PhysicsSystem::DispatchCollisionExit(World& world, EntityID entityID, EntityID otherEntityID)
	{
		/// [EN] Either actor may have been destroyed between the contact and its delivery.
		/// [JP] 接触から配送までの間に、どちらかの Actor が破棄されていることがある。
		Actor actor = world.GetActor(entityID);
		Actor otherActor = world.GetActor(otherEntityID);
		if (!actor || !otherActor)
		{
			return;
		}

		/// [EN] Every ComponentBase-derived component on the entity receives the event with the other entity.
		/// [JP] エンティティの、ComponentBase を継承した全コンポーネントが、相手のエンティティと一緒にイベントを受け取る。
		Entity otherEntity = otherActor.GetEntity();
		for (ComponentID id : actor.ComponentBaseIDList())
		{
			if (ComponentBase* component = reinterpret_cast<ComponentBase*>(world.GetComponent(entityID, id)))
			{
				component->DispatchCollisionExit(otherEntity);
			}
		}
	}
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
	void PhysicsSystem::DispatchTriggerEnter(World& world, EntityID entityID, EntityID otherEntityID)
	{
		/// [EN] Either actor may have been destroyed between the contact and its delivery.
		/// [JP] 接触から配送までの間に、どちらかの Actor が破棄されていることがある。
		Actor actor = world.GetActor(entityID);
		Actor otherActor = world.GetActor(otherEntityID);
		if (!actor || !otherActor)
		{
			return;
		}

		/// [EN] Every ComponentBase-derived component on the entity receives the event with the other entity.
		/// [JP] エンティティの、ComponentBase を継承した全コンポーネントが、相手のエンティティと一緒にイベントを受け取る。
		Entity otherEntity = otherActor.GetEntity();
		for (ComponentID id : actor.ComponentBaseIDList())
		{
			if (ComponentBase* component = reinterpret_cast<ComponentBase*>(world.GetComponent(entityID, id)))
			{
				component->DispatchTriggerEnter(otherEntity);
			}
		}
	}
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
	void PhysicsSystem::DispatchTriggerStay(World& world, EntityID entityID, EntityID otherEntityID)
	{
		/// [EN] Either actor may have been destroyed between the contact and its delivery.
		/// [JP] 接触から配送までの間に、どちらかの Actor が破棄されていることがある。
		Actor actor = world.GetActor(entityID);
		Actor otherActor = world.GetActor(otherEntityID);
		if (!actor || !otherActor)
		{
			return;
		}

		/// [EN] Every ComponentBase-derived component on the entity receives the event with the other entity.
		/// [JP] エンティティの、ComponentBase を継承した全コンポーネントが、相手のエンティティと一緒にイベントを受け取る。
		Entity otherEntity = otherActor.GetEntity();
		for (ComponentID id : actor.ComponentBaseIDList())
		{
			if (ComponentBase* component = reinterpret_cast<ComponentBase*>(world.GetComponent(entityID, id)))
			{
				component->DispatchTriggerStay(otherEntity);
			}
		}
	}
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
	void PhysicsSystem::DispatchTriggerExit(World& world, EntityID entityID, EntityID otherEntityID)
	{
		/// [EN] Either actor may have been destroyed between the contact and its delivery.
		/// [JP] 接触から配送までの間に、どちらかの Actor が破棄されていることがある。
		Actor actor = world.GetActor(entityID);
		Actor otherActor = world.GetActor(otherEntityID);
		if (!actor || !otherActor)
		{
			return;
		}

		/// [EN] Every ComponentBase-derived component on the entity receives the event with the other entity.
		/// [JP] エンティティの、ComponentBase を継承した全コンポーネントが、相手のエンティティと一緒にイベントを受け取る。
		Entity otherEntity = otherActor.GetEntity();
		for (ComponentID id : actor.ComponentBaseIDList())
		{
			if (ComponentBase* component = reinterpret_cast<ComponentBase*>(world.GetComponent(entityID, id)))
			{
				component->DispatchTriggerExit(otherEntity);
			}
		}
	}
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
	void PhysicsSystem::ResolveMeshCollider(LoaderSystem& loader, ResourceCache& cache, World& world)
	{
		/// [EN] Without the collision-mesh manager nothing can be resolved yet.
		/// [JP] 衝突メッシュの管理機構が無ければ、まだ何も解決できない。
		MeshCollisionResource* meshCollisionResource = cache.GetResource<MeshCollisionResource>(AssetType::MeshCollision);
		if (!meshCollisionResource)
		{
			return;
		}

		for (EntityID id : world.GetComponents<MeshCollider>())
		{
			Actor actor = world.GetActor(id);
			if (!actor)
			{
				continue;
			}

			/// [EN] Only colliders still waiting are processed; built ones are skipped.
			/// [JP] 待っているコライダーだけを処理し、構築済みのものは飛ばす。
			MeshCollider* collider = actor.GetComponent<MeshCollider>();
			if (!collider || !collider->Pending())
			{
				continue;
			}

			/// [EN] An asset ID that is not registered yields no handle.
			/// [JP] 登録されていないアセット ID ではハンドルが得られない。
			Handle<MeshCollision> handle = meshCollisionResource->GetHandle(collider->meshID_);
			if (handle.empty())
			{
				continue;
			}

			/// [EN] Null while the asset is still loading; the collider stays pending and is tried again next frame.
			/// [JP] 読み込み中は null。コライダーは待ちのまま残り、次のフレームで再び試す。
			MeshCollision* meshCollision = meshCollisionResource->Resolve(loader, handle);
			if (!meshCollision)
			{
				continue;
			}

			collider->Build(*meshCollision);
		}
	}

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
	void PhysicsSystem::ResolveSoftbody(LoaderSystem& loader, ResourceCache& cache, World& world)
	{
		/// [EN] Without the model manager nothing can be resolved yet.
		/// [JP] モデルの管理機構が無ければ、まだ何も解決できない。
		ModelResource* modelResource = cache.GetResource<ModelResource>(AssetType::Model);
		if (!modelResource)
		{
			return;
		}

		for (EntityID id : world.GetComponents<Softbody>())
		{
			Actor actor = world.GetActor(id);
			if (!actor)
			{
				continue;
			}

			/// [EN] Only soft bodies still waiting are processed; built ones are skipped.
			/// [JP] 待っているソフトボディだけを処理し、構築済みのものは飛ばす。
			Softbody* softbody = actor.GetComponent<Softbody>();
			if (!softbody || !softbody->Pending())
			{
				continue;
			}

			/// [EN] The soft body takes its geometry from the Mesh on the same actor.
			/// [JP] ソフトボディは、同じ Actor の Mesh から形を取る。
			const Mesh* mesh = actor.GetComponent<Mesh>();
			if (!mesh)
			{
				continue;
			}

			/// [EN] An asset ID that is not registered yields no handle.
			/// [JP] 登録されていないアセット ID ではハンドルが得られない。
			Handle<Crister> handle = modelResource->GetHandle(mesh->meshID_);
			if (handle.empty())
			{
				continue;
			}

			/// [EN] Null while the model is still loading; the soft body stays pending and is tried again next frame.
			/// [JP] 読み込み中は null。ソフトボディは待ちのまま残り、次のフレームで再び試す。
			Crister* crister = modelResource->Resolve(loader, handle);
			if (!crister)
			{
				continue;
			}

			softbody->Build(*crister);
		}
	}
}
