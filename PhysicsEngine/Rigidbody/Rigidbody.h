#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/SeedScript.h>
#include <FoundationEngine/Utility/Handle.h>

namespace SeedCore
{
	/**
	* [EN]
	* Component that gives its actor a simulated body. It borrows the
	* shape of a collider on the same actor (a 0.5 m sphere if none) and,
	* each fixed step, copies the body's pose back to Position/Rotation.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* Actor にシミュレーションされるボディを持たせるコンポーネント。同じ
	* Actor のコライダーの形状を借り(無ければ半径 0.5 m の球)、固定ステップ
	* ごとにボディの姿勢を Position/Rotation へ書き戻す。
	*/
	class SEEDCORE_API Rigidbody :public SeedScript
	{
	public:
		/**
		* [EN]
		* How the body moves.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* ボディの動き方。
		*/
		enum class BodyType
		{
			/// [EN] Moved by forces, gravity and collisions.
			/// [JP] 力・重力・衝突で動く。
			Dynamic,

			/// [EN] Moved only by code or animation; pushes dynamic bodies but is not pushed back.
			/// [JP] コードやアニメーションでだけ動く。動的ボディを押すが、押し返されない。
			Kinematic,

			/// [EN] Never moves.
			/// [JP] 動かない。
			Static,
		};

	public:
		/// [EN] How the body moves.
		/// [JP] ボディの動き方。
		SC_REFLECTION_FIELD_EX("動作モード")
		BodyType bodyType_ = BodyType::Dynamic;

		/// [EN] Sweeps the body along its motion so a fast body does not pass through thin geometry.
		/// [JP] 移動経路に沿ってボディを掃引し、高速なボディが薄い物体をすり抜けないようにする。
		SC_REFLECTION_FIELD_CONDITION(bodyType_ == BodyType::Dynamic)
		SC_REFLECTION_FIELD_EX("連続衝突判定")
		Bool continuousCollision_ = false;

		/// [EN] Mass in kilograms; the inertia is derived from the shape at this mass.
		/// [JP] 質量(kg)。慣性はこの質量に合わせて形状から求める。
		SC_REFLECTION_CLAMPED_EX("質量", 0.001f, 100.0f)
		Float mass_ = 1.0f;

		/// [EN] Damping that slows the body's linear velocity over time.
		/// [JP] 移動速度を時間とともに弱める減衰。
		SC_REFLECTION_CLAMPED_EX("空気抵抗", 0.0f, 10.0f)
		Float linearDrag_ = 0.5f;

		/// [EN] Damping that slows the body's angular velocity over time.
		/// [JP] 回転速度を時間とともに弱める減衰。
		SC_REFLECTION_CLAMPED_EX("角抵抗", 0.0f, 10.0f)
		Float angularDrag_ = 0.5f;

		/// [EN] Friction coefficient: 0 slides freely, 1 grips strongly.
		/// [JP] 摩擦係数。0 で滑り、1 で強く止まる。
		SC_REFLECTION_CLAMPED_EX("摩擦係数", 0.0f, 1.0f)
		Float friction_ = 0.2f;

		/// [EN] Restitution: 0 does not bounce, 1 bounces back with full speed.
		/// [JP] 反発係数。0 で跳ねず、1 で速さを保って跳ね返る。
		SC_REFLECTION_CLAMPED_EX("反発係数", 0.0f, 1.0f)
		Float restitution_ = 0.5f;

		/// [EN] Whether world gravity acts on the body.
		/// [JP] ワールドの重力をボディに働かせるか。
		SC_REFLECTION_FIELD_EX("重力")
		Bool useGravity_ = true;

		/// [EN] Multiplier on world gravity.
		/// [JP] ワールドの重力に掛ける倍率。
		SC_REFLECTION_FIELD_CONDITION(useGravity_)
		SC_REFLECTION_FIELD_EX("重力倍率")
		Float gravityScale_ = 1.0f;

		/// [EN] Locks movement along world X.
		/// [JP] ワールドの X 方向の移動を固定する。
		SC_REFLECTION_FIELD_EX("位置X軸固定")
		Bool freezePositionX_ = false;

		/// [EN] Locks movement along world Y.
		/// [JP] ワールドの Y 方向の移動を固定する。
		SC_REFLECTION_FIELD_EX("位置Y軸固定")
		Bool freezePositionY_ = false;

		/// [EN] Locks movement along world Z; has no effect on a canvas body.
		/// [JP] ワールドの Z 方向の移動を固定する。Canvas のボディには効かない。
		SC_REFLECTION_FIELD_EX("位置Z軸固定")
		Bool freezePositionZ_ = false;

		/// [EN] Locks rotation about X; on a canvas body it locks the in-plane rotation.
		/// [JP] X 軸まわりの回転を固定する。Canvas のボディでは平面内の回転を固定する。
		SC_REFLECTION_FIELD_EX("回転X軸固定")
		Bool freezeRotationX_ = false;

		/// [EN] Locks rotation about Y; has no effect on a canvas body.
		/// [JP] Y 軸まわりの回転を固定する。Canvas のボディには効かない。
		SC_REFLECTION_FIELD_EX("回転Y軸固定")
		Bool freezeRotationY_ = false;

		/// [EN] Locks rotation about Z; on a canvas body it locks the in-plane rotation.
		/// [JP] Z 軸まわりの回転を固定する。Canvas のボディでは平面内の回転を固定する。
		SC_REFLECTION_FIELD_EX("回転Z軸固定")
		Bool freezeRotationZ_ = false;

		/// [EN] Whether the body reports overlaps without physical response.
		/// [JP] 物理応答を行わず、重なりだけを知らせるトリガーにするか。
		SC_REFLECTION_FIELD_EX("トリガー")
		Bool isTrigger_ = false;

	public:
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
		void OnAwake();

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
		void OnFixedTick(Float elapsedTime);

		/**
		* [EN]
		* Destroys the body and releases the reference to its shape.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* ボディを破棄し、形状への参照を解放する。
		*/
		void OnDestroy();

	public:
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
		JPH::BodyID BodyID()const;

	private:
		/// [EN] Radius of the sphere used when the actor has no collider.
		/// [JP] Actor にコライダーが無いときに使う球の半径。
		static constexpr Float defaultShapeRadius_ = 0.5f;

		/// [EN] Number of canvas pixels in one physics meter.
		/// [JP] 物理の1メートルに当たる Canvas のピクセル数。
		static constexpr Float pixelsPerMeter_ = 100.0f;

		/// [EN] Jolt ID of the body.
		/// [JP] ボディの Jolt ID。
		JPH::BodyID bodyID_;

		/// [EN] This component's own reference to the pooled shape.
		/// [JP] プールの形状に対する、このコンポーネント自身の参照。
		Handle<JPH::Shape> shapeHandle_;
	};
	REGISTER_COMPONENT(Rigidbody, "Physics");
}
