#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/SeedScript.h>
#include <PhysicsEngine/JoltPhysics/JoltConstraintPool.h>

namespace SeedCore
{
	/**
	* [EN]
	* Component that connects two bodies with a damped distance spring.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 2つのボディを減衰付き距離ばねで接続するコンポーネント。
	*/
	class SEEDCORE_API SpringJoint :public SeedScript
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

		/// [EN] Minimum permitted spring distance.
		/// [JP] ばねが許容する最小距離。
		SC_REFLECTION_CLAMPED_EX("最小距離", 0.0f, 1000.0f)
		Float minDistance_ = 0.0f;

		/// [EN] Maximum permitted spring distance.
		/// [JP] ばねが許容する最大距離。
		SC_REFLECTION_CLAMPED_EX("最大距離", 0.0f, 1000.0f)
		Float maxDistance_ = 0.0f;

		/// [EN] Spring oscillation frequency in hertz.
		/// [JP] ヘルツ単位のばね振動周波数。
		SC_REFLECTION_CLAMPED_EX("剛性(Hz)", 0.0f, 30.0f)
		Float frequency_ = 2.0f;

		/// [EN] Damping ratio applied to spring motion.
		/// [JP] ばね運動へ適用する減衰比。
		SC_REFLECTION_CLAMPED_EX("減衰", 0.0f, 1.0f)
		Float damping_ = 0.5f;

	public:
		/**
		* [EN]
		* Creates the configured spring constraint.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 設定されたばね拘束を生成する。
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
	REGISTER_COMPONENT(SpringJoint, "Physics");
}
