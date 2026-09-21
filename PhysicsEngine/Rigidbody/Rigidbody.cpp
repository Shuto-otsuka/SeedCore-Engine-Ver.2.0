#include <PhysicsEngine/Rigidbody/Rigidbody.h>
#include <PhysicsEngine/Physics/Physics.h>
#include <PhysicsEngine/Physics/PhysicsSystem.h>
#include <PhysicsEngine/JoltPhysics/JoltLayerdef.h>
#include <PhysicsEngine/Collider/BoxCollider.h>
#include <PhysicsEngine/Collider/SphereCollider.h>
#include <PhysicsEngine/Collider/CapsuleCollider.h>
#include <PhysicsEngine/Collider/CylinderCollider.h>
#include <PhysicsEngine/Collider/RectCollider.h>
#include <PhysicsEngine/Collider/CircleCollider.h>
#include <FoundationEngine/ECS/Actor.h>
#include <FoundationEngine/ECS/World.h>
#include <FoundationEngine/ECS/Component/Position.h>
#include <FoundationEngine/ECS/Component/Rotation.h>

namespace SeedCore
{
	/**
	* [EN]
	* Creates the body from the actor's collider shape, pose and this
	* component's settings, and adds it to the simulation.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* Actor のコライダーの形状と姿勢、このコンポーネントの設定からボディを作り、
	* シミュレーションへ加える。
	*/
	void Rigidbody::OnAwake()
	{
		Actor actor = GetActor();

		/// [EN] Borrows the shape of the first collider found; each call adds a reference this component owns.
		/// [JP] 最初に見つかったコライダーの形状を借りる。呼ぶたびに、このコンポーネントが持つ参照が1つ増える。
		/// [EN] A MeshCollider is not looked for: its shape arrives later and replaces the fallback sphere.
		/// [JP] MeshCollider は探さない。その形状は後から届き、代わりの球と差し替わる。
		shapeHandle_ = [&]() -> Handle<JPH::Shape>
		{
			if (BoxCollider* collider = actor.GetComponent<BoxCollider>())
			{
				return collider->GetShapeHandle();
			}
			if (SphereCollider* collider = actor.GetComponent<SphereCollider>())
			{
				return collider->GetShapeHandle();
			}
			if (CapsuleCollider* collider = actor.GetComponent<CapsuleCollider>())
			{
				return collider->GetShapeHandle();
			}
			if (CylinderCollider* collider = actor.GetComponent<CylinderCollider>())
			{
				return collider->GetShapeHandle();
			}
			if (RectCollider* collider = actor.GetComponent<RectCollider>())
			{
				return collider->GetShapeHandle();
			}
			if (CircleCollider* collider = actor.GetComponent<CircleCollider>())
			{
				return collider->GetShapeHandle();
			}

			/// [EN] Without a collider the body still needs a shape, so a small sphere stands in.
			/// [JP] コライダーが無くてもボディには形状が要るので、小さな球で代用する。
			return actor.GetPhysics().CreateSphereShape(defaultShapeRadius_);
		}();

		/// [EN] Starts with every axis free and removes each frozen one.
		/// [JP] 全ての軸を自由にした状態から、固定した軸を1つずつ外す。
		JPH::EAllowedDOFs allowedDOFs = [&]() -> JPH::EAllowedDOFs
		{
			JPH::EAllowedDOFs dofs = JPH::EAllowedDOFs::All;
			if (freezePositionX_)
			{
				dofs &= ~JPH::EAllowedDOFs::TranslationX;
			}
			if (freezePositionY_)
			{
				dofs &= ~JPH::EAllowedDOFs::TranslationY;
			}
			if (freezePositionZ_)
			{
				dofs &= ~JPH::EAllowedDOFs::TranslationZ;
			}
			if (freezeRotationX_)
			{
				dofs &= ~JPH::EAllowedDOFs::RotationX;
			}
			if (freezeRotationY_)
			{
				dofs &= ~JPH::EAllowedDOFs::RotationY;
			}
			if (freezeRotationZ_)
			{
				dofs &= ~JPH::EAllowedDOFs::RotationZ;
			}

			return dofs;
		}();

		/// [EN] Shape, pose and EntityID, then the motion type and the layer packed from it and the actor's layer.
		/// [JP] 形状・姿勢・EntityID、続いて運動タイプと、それと Actor のレイヤーからパックしたレイヤー。
		RigidbodyDesc desc;
		desc.shape_ = shapeHandle_;
		PhysicsSystem::ApplyTransform(actor, desc);
		desc.motionType_ = ToMotionType(bodyType_);
		desc.continuousCollision_ = continuousCollision_;
		desc.layer_ = ToObjectLayer(bodyType_, actor.GetLayer());

		/// [EN] Material and motion settings; gravity off is a gravity factor of 0.
		/// [JP] 材質と運動の設定。重力を切るのは、重力の倍率を 0 にすることと同じ。
		desc.mass_ = mass_;
		desc.linearDamping_ = linearDrag_;
		desc.angularDamping_ = angularDrag_;
		desc.friction_ = friction_;
		desc.restitution_ = restitution_;
		desc.gravityFactor_ = useGravity_ ? gravityScale_ : 0.0f;
		desc.allowedDOFs_ = allowedDOFs;
		desc.isSensor_ = isTrigger_;

		/// [EN] A canvas body lives on the z = 0 plane and only collides with other canvas bodies.
		/// [JP] Canvas のボディは z = 0 の平面上にあり、他の Canvas のボディとだけ衝突する。
		if (actor.GetComponent<RectCollider>() || actor.GetComponent<CircleCollider>())
		{
			/// [EN] Only X/Y movement and rotation about Z remain; the 3D freeze settings are remapped below.
			/// [JP] 残るのは X/Y の移動と Z 軸まわりの回転だけ。3D 用の固定設定は下で置き換える。
			desc.layer_ |= Layers::PLANAR;
			desc.allowedDOFs_ = JPH::EAllowedDOFs::Plane2D;

			if (freezePositionX_)
			{
				desc.allowedDOFs_ &= ~JPH::EAllowedDOFs::TranslationX;
			}

			if (freezePositionY_)
			{
				desc.allowedDOFs_ &= ~JPH::EAllowedDOFs::TranslationY;
			}

			/// [EN] The canvas keeps its in-plane angle in Rotation::x_, so the X freeze also locks it.
			/// [JP] Canvas は平面内の角度を Rotation::x_ に持つので、X の固定でもそれを固定する。
			if (freezeRotationX_ || freezeRotationZ_)
			{
				desc.allowedDOFs_ &= ~JPH::EAllowedDOFs::RotationZ;
			}
		}

		bodyID_ = actor.GetPhysics().CreateRigidbody(desc);
	}

	/**
	* [EN]
	* Copies the simulated pose of the body back to the actor's
	* Position and Rotation.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* シミュレーション後のボディの姿勢を、Actor の Position と Rotation へ
	* 書き戻す。
	*/
	void Rigidbody::OnFixedTick(Float elapsedTime)
	{
		if (bodyID_.IsInvalid())
		{
			return;
		}

		Actor actor = GetActor();
		World& world = actor.GetWorld();
		Entity entity = actor.GetEntity();

		/// [EN] With neither Position nor Rotation there is nothing to write back.
		/// [JP] Position も Rotation も無ければ、書き戻す先が無い。
		Position* position = world.GetComponent<Position>(entity);
		Rotation* rotation = world.GetComponent<Rotation>(entity);
		if (!position && !rotation)
		{
			return;
		}

		Vector3 outPosition;
		Quaternion outRotation;
		actor.GetPhysics().BodyTransform(bodyID_, outPosition, outRotation);

		/// [EN] Canvas bodies go back from meters (Y up) to pixels (Y down).
		/// [JP] Canvas のボディは、メートル(Y 上向き)からピクセル(Y 下向き)へ戻す。
		if (actor.GetComponent<RectCollider>() || actor.GetComponent<CircleCollider>())
		{
			if (position)
			{
				position->x_ = outPosition.x * pixelsPerMeter_;
				position->y_ = -outPosition.y * pixelsPerMeter_;
			}

			/// [EN] The body only turns about Z, so the angle is 2·atan2(z, w), negated with the flipped Y.
			/// [JP] ボディは Z 軸まわりにしか回らないので、角度は 2·atan2(z, w)。Y の反転に合わせて符号を反転する。
			if (rotation)
			{
				rotation->x_ = -ToDegrees(2.0f * std::atan2(outRotation.z, outRotation.w));
			}

			return;
		}

		if (position)
		{
			position->x_ = outPosition.x;
			position->y_ = outPosition.y;
			position->z_ = outPosition.z;
		}

		/// [EN] Rotation is stored as Euler angles in degrees.
		/// [JP] Rotation は度単位のオイラー角で持つ。
		if (rotation)
		{
			const Vector3 euler = outRotation.ToEuler();
			rotation->x_ = ToDegrees(euler.x);
			rotation->y_ = ToDegrees(euler.y);
			rotation->z_ = ToDegrees(euler.z);
		}
	}

	/**
	* [EN]
	* Destroys the body and releases the reference to its shape.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* ボディを破棄し、形状への参照を解放する。
	*/
	void Rigidbody::OnDestroy()
	{
		if (bodyID_.IsInvalid())
		{
			return;
		}

		/// [EN] Only this component's reference is released; the collider keeps its own.
		/// [JP] 解放するのはこのコンポーネントの参照だけで、コライダーは自分の参照を持ち続ける。
		Actor actor = GetActor();
		actor.GetPhysics().DestroyBody(bodyID_);
		actor.GetPhysics().ReleaseShape(shapeHandle_);

		bodyID_ = JPH::BodyID();
	}

	/**
	* [EN]
	* Returns the Jolt ID of the body; invalid before OnAwake and after
	* OnDestroy.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* ボディの Jolt ID を返す。OnAwake 前と OnDestroy 後は無効な ID。
	*/
	JPH::BodyID Rigidbody::BodyID()const
	{
		return bodyID_;
	}
}
