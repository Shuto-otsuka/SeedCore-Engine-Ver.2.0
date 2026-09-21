#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/SeedScript.h>
#include <PhysicsEngine/JoltPhysics/JoltConstraintPool.h>

namespace SeedCore
{
	/**
	* [EN]
	* Component that constrains two bodies to translate along one axis.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 2つのボディを1本の軸に沿って平行移動させるコンポーネント。
	*/
	class SEEDCORE_API SliderJoint :public SeedScript
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

		/// [EN] Slide axis in the owning actor's local space.
		/// [JP] 所有アクターのローカル空間におけるスライド軸。
		SC_REFLECTION_FIELD_EX("スライド軸(ローカル)")
		Vector3 axis_ = { 1.0f, 0.0f, 0.0f };

		/// [EN] Whether distance limits constrain translation.
		/// [JP] 距離制限で平行移動を拘束するか。
		SC_REFLECTION_FIELD_EX("可動範囲制限を使う")
		Bool useLimits_ = false;

		/// [EN] Minimum permitted distance along the slide axis.
		/// [JP] スライド軸に沿って許可する最小距離。
		SC_REFLECTION_FIELD_CONDITION(useLimits_)
		SC_REFLECTION_CLAMPED_EX("最小距離", -1000.0f, 0.0f)
		Float minDistance_ = 0.0f;

		/// [EN] Maximum permitted distance along the slide axis.
		/// [JP] スライド軸に沿って許可する最大距離。
		SC_REFLECTION_FIELD_CONDITION(useLimits_)
		SC_REFLECTION_CLAMPED_EX("最大距離", 0.0f, 1000.0f)
		Float maxDistance_ = 0.0f;

	public:
		/**
		* [EN]
		* Creates the configured slider constraint.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 設定されたスライダー拘束を生成する。
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
	REGISTER_COMPONENT(SliderJoint, "Physics");
}
