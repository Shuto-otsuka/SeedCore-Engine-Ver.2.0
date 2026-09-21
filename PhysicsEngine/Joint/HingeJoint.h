#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/SeedScript.h>
#include <PhysicsEngine/JoltPhysics/JoltConstraintPool.h>

namespace SeedCore
{
	/**
	* [EN]
	* Component that constrains two bodies to rotate around one hinge axis.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 2つのボディを1本のヒンジ軸まわりに回転させるコンポーネント。
	*/
	class SEEDCORE_API HingeJoint :public SeedScript
	{
	public:
		/// [EN] Whether the joint is created when the component starts.
		/// [JP] コンポーネント開始時にジョイントを生成するか。
		SC_REFLECTION_FIELD_EX("有効")
		Bool enabled_ = true;

		/// [EN] Entity ID of the connected actor; zero connects to the world.
		/// [JP] 接続先アクターのエンティティ ID。0 はワールドへ接続する。
		SC_PAYLOAD_FIELD_EX("接続先アクター", Actor)
		Uint32 connectedActor_ = 0;

		/// [EN] Joint anchor in the owning actor's local space.
		/// [JP] 所有アクターのローカル空間におけるジョイントアンカー。
		SC_REFLECTION_FIELD_EX("アンカー(ローカル)")
		Vector3 anchor_ = { 0.0f, 0.0f, 0.0f };

		/// [EN] Hinge axis in the owning actor's local space.
		/// [JP] 所有アクターのローカル空間におけるヒンジ軸。
		SC_REFLECTION_FIELD_EX("ヒンジ軸(ローカル)")
		Vector3 axis_ = { 0.0f, 1.0f, 0.0f };

		/// [EN] Whether angular limits constrain hinge rotation.
		/// [JP] 角度制限でヒンジ回転を拘束するか。
		SC_REFLECTION_FIELD_EX("角度制限を使う")
		Bool useLimits_ = false;

		/// [EN] Minimum permitted hinge angle in degrees.
		/// [JP] 許可する最小ヒンジ角度。
		SC_REFLECTION_FIELD_CONDITION(useLimits_)
		SC_REFLECTION_CLAMPED_EX("最小角(度)", -180.0f, 0.0f)
		Float minAngle_ = 0.0f;

		/// [EN] Maximum permitted hinge angle in degrees.
		/// [JP] 許可する最大ヒンジ角度。
		SC_REFLECTION_FIELD_CONDITION(useLimits_)
		SC_REFLECTION_CLAMPED_EX("最大角(度)", 0.0f, 180.0f)
		Float maxAngle_ = 0.0f;

	public:
		/**
		* [EN]
		* Creates the configured hinge constraint.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 設定されたヒンジ拘束を生成する。
		*/
		void OnStart();

		/**
		* [EN]
		* Destroys the created constraint.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 生成した拘束を破棄する。
		*/
		void OnDestroy();

	private:
		/// [EN] Pool handle of the created Jolt constraint.
		/// [JP] 生成した Jolt 拘束のプールハンドル。
		Handle<JPH::Constraint> handle_;
	};
	REGISTER_COMPONENT(HingeJoint, "Physics");
}
