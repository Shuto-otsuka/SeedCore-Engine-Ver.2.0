#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/SeedScript.h>
#include <PhysicsEngine/JoltPhysics/JoltCharacterContactListener.h>

namespace SeedCore
{
	/**
	* [EN]
	* Physics-driven character component that controls movement, rotation,
	* jumping, crouching and contact-state queries through Jolt CharacterVirtual.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* Jolt CharacterVirtual を通じて移動、回転、ジャンプ、しゃがみ、接触状態の
	* 問い合わせを制御する物理キャラクターコンポーネント。
	*/
	class SEEDCORE_API CharacterController :public SeedScript
	{
	public:
		/// [EN] Radius of the character capsule.
		/// [JP] キャラクターカプセルの半径。
		SC_REFLECTION_CLAMPED_EX("半径", 0.001f, 100.0f)
		Float radius_ = 0.3f;

		/// [EN] Standing height of the character capsule.
		/// [JP] 立っているときのキャラクターカプセルの高さ。
		SC_REFLECTION_CLAMPED_EX("高さ", 0.001f, 100.0f)
		Float height_ = 1.8f;

		/// [EN] Mass used by the virtual character.
		/// [JP] 仮想キャラクターに使う質量。
		SC_REFLECTION_CLAMPED_EX("質量", 0.001f, 1000.0f)
		Float mass_ = 70.0f;

		/// [EN] Maximum force applied when pushing contacted bodies.
		/// [JP] 接触したボディを押すときに加える最大の力。
		SC_REFLECTION_CLAMPED_EX("押し出し力", 0.0f, 10000.0f)
		Float pushForce_ = 100.0f;

		/// [EN] Maximum horizontal movement speed.
		/// [JP] 水平方向の最大移動速度。
		SC_REFLECTION_CLAMPED_EX("最大移動速度", 0.0f, 100.0f)
		Float maxMoveSpeed_ = 5.0f;

		/// [EN] Rate used to approach the requested movement speed.
		/// [JP] 要求された移動速度へ近づける加速度。
		SC_REFLECTION_CLAMPED_EX("加速度", 0.0f, 1000.0f)
		Float acceleration_ = 20.0f;

		/// [EN] Rate used to stop horizontal movement without input.
		/// [JP] 入力がないときに水平移動を止める減速度。
		SC_REFLECTION_CLAMPED_EX("減速度", 0.0f, 1000.0f)
		Float deceleration_ = 20.0f;

		/// [EN] Maximum rotation speed in degrees per second.
		/// [JP] 1秒あたりの最大回転角度。
		SC_REFLECTION_CLAMPED_EX("回転速度", 0.0f, 1000.0f)
		Float turnSpeed_ = 10.0f;

		/// [EN] Horizontal damping applied while unsupported.
		/// [JP] 支持されていない間に適用する水平方向の減衰。
		SC_REFLECTION_CLAMPED_EX("空気抵抗", 0.0f, 10.0f)
		Float airDrag_ = 0.0f;

		/// [EN] Steepest walkable slope angle in degrees.
		/// [JP] 歩行可能な最大斜面角度。
		SC_REFLECTION_CLAMPED_EX("最大斜面角度", 0.0f, 89.0f)
		Float maxSlopeAngle_ = 50.0f;

		/// [EN] Highest step the character can climb automatically.
		/// [JP] キャラクターが自動で上れる最大段差高。
		SC_REFLECTION_CLAMPED_EX("最大許容段差高", 0.0f, 10.0f)
		Float maxStepHeight_ = 0.4f;

		/// [EN] Multiplier applied to the world's vertical gravity.
		/// [JP] ワールドの垂直重力へ適用する倍率。
		SC_REFLECTION_FIELD_EX("重力倍率")
		Float gravityScale_ = 1.0f;

		/// [EN] Downward speed limit applied after gravity.
		/// [JP] 重力適用後の下向き速度の上限。
		SC_REFLECTION_CLAMPED_EX("最大落下速度", 0.0f, 1000.0f)
		Float maxFallSpeed_ = 50.0f;

		/// [EN] Upward velocity assigned when jumping.
		/// [JP] ジャンプ時に設定する上向き速度。
		SC_REFLECTION_CLAMPED_EX("ジャンプ力", 0.0f, 1000.0f)
		Float jumpPower_ = 5.0f;

		/// [EN] Whether the controller requests the crouched capsule height.
		/// [JP] しゃがみ用のカプセル高を要求するか。
		SC_REFLECTION_FIELD_EX("しゃがみ")
		Bool crouch_ = false;

		/// [EN] Capsule height used while crouching.
		/// [JP] しゃがみ中に使うカプセルの高さ。
		SC_REFLECTION_CLAMPED_EX("しゃがみ時の高さ", 0.001f, 100.0f)
		Float crouchHeight_ = 1.0f;

	public:
		/**
		* [EN]
		* Creates the virtual character from the actor transform and settings.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* アクターの変換と設定から仮想キャラクターを生成する。
		*/
		void OnAwake();

		/**
		* [EN]
		* Advances character movement and writes the resulting transform to the actor.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* キャラクター移動を進め、結果の変換をアクターへ書き戻す。
		*/
		void OnFixedTick(Float elapsedTime);

		/**
		* [EN]
		* Destroys the virtual character and releases its contact listener.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 仮想キャラクターを破棄し、接触リスナーを解放する。
		*/
		void OnDestroy();

	public:
		/**
		* [EN]
		* Sets the requested horizontal movement direction and magnitude.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 要求する水平移動の方向と大きさを設定する。
		*/
		void MoveDirection(const Vector3& moveDirection);

		/**
		* [EN]
		* Returns the requested movement direction and magnitude.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 要求中の移動方向と大きさを返す。
		*/
		const Vector3& MoveDirection()const;

		/**
		* [EN]
		* Sets the horizontal direction the character should face.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* キャラクターが向く水平方向を設定する。
		*/
		void ForwardDirection(const Vector3& forwardDirection);

		/**
		* [EN]
		* Returns the requested facing direction.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 要求中の正面方向を返す。
		*/
		const Vector3& ForwardDirection()const;

	public:
		/**
		* [EN]
		* Applies the configured upward jump velocity while on the ground.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 接地中に設定された上向きジャンプ速度を適用する。
		*/
		void Jump();

		/**
		* [EN]
		* Moves the character immediately and synchronizes the actor position.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* キャラクターを即座に移動し、アクターの位置を同期する。
		*/
		void Teleport(const Vector3& position);

	public:
		/**
		* [EN]
		* Reports whether Jolt classifies the character as on the ground.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* Jolt がキャラクターを接地状態と判定しているかを返す。
		*/
		Bool OnGround()const;

		/**
		* [EN]
		* Reports whether an active contact has a wall-facing normal.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 有効な接触に壁方向の法線があるかを返す。
		*/
		Bool OnWall()const;

		/**
		* [EN]
		* Reports whether an active contact faces downward from a ceiling.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 有効な接触に天井から下向きの法線があるかを返す。
		*/
		Bool OnCeiling()const;

		/**
		* [EN]
		* Reports whether the grounded surface is inclined.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 接地面が傾斜しているかを返す。
		*/
		Bool OnSlope()const;

	public:
		/**
		* [EN]
		* Returns the current ground-contact normal.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 現在の接地法線を返す。
		*/
		Vector3 GroundNormal()const;

		/**
		* [EN]
		* Returns the first active wall-contact normal.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 最初に見つかった有効な壁接触法線を返す。
		*/
		Vector3 WallNormal()const;

		/**
		* [EN]
		* Returns the first active ceiling-contact normal.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 最初に見つかった有効な天井接触法線を返す。
		*/
		Vector3 CeilingNormal()const;

	public:
		/**
		* [EN]
		* Reports whether the crouched capsule is currently active.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* しゃがみ用カプセルが現在有効かを返す。
		*/
		Bool Crouching()const;

		/**
		* [EN]
		* Reports whether an unsupported character is moving downward.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 支持されていないキャラクターが下降中かを返す。
		*/
		Bool Falling()const;

		/**
		* [EN]
		* Reports whether the character ground state is in air.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* キャラクターの接地状態が空中かを返す。
		*/
		Bool Flying()const;

		/**
		* [EN]
		* Reports whether the character is supported by a surface.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* キャラクターが面に支持されているかを返す。
		*/
		Bool Grounded()const;

		/**
		* [EN]
		* Reports whether an unsupported character is moving upward.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 支持されていないキャラクターが上昇中かを返す。
		*/
		Bool Jumping()const;

		/**
		* [EN]
		* Reports whether a supported character has horizontal velocity.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 支持されたキャラクターに水平速度があるかを返す。
		*/
		Bool Running()const;

		/**
		* [EN]
		* Reports whether a supported character has no horizontal velocity.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 支持されたキャラクターに水平速度がないかを返す。
		*/
		Bool Stopped()const;

	public:
		/// [EN] Optional callback that replaces the built-in fixed-step movement.
		/// [JP] 標準の固定ステップ移動を置き換える任意のコールバック。
		SinglecastDelegate<void(Float)> onCustomMove_;

	private:
		/// [EN] Absolute vertical-normal dot value below which a contact is treated as a wall.
		/// [JP] 接触を壁として扱う垂直法線内積の絶対値上限。
		static constexpr Float wallNormalDotLimit_ = 0.5f;

		/// [EN] Requested horizontal movement direction and magnitude.
		/// [JP] 要求中の水平移動方向と大きさ。
		Vector3 moveDirection_ = { 0.0f, 0.0f, 0.0f };

		/// [EN] Requested direction the character should face.
		/// [JP] キャラクターが向くよう要求された方向。
		Vector3 forwardDirection_ = { 0.0f, 0.0f, 1.0f };

		/// [EN] Whether the crouched capsule height was applied successfully.
		/// [JP] しゃがみ用カプセル高の適用に成功しているか。
		Bool isCrouched_ = false;

		/// [EN] Jolt virtual-character instance owned by this component.
		/// [JP] このコンポーネントが所有する Jolt 仮想キャラクター。
		JPH::Ref<JPH::CharacterVirtual> character_;

		/// [EN] Contact listener forwarding character contacts to the World.
		/// [JP] キャラクターの接触を World へ転送する接触リスナー。
		JPH::Ref<JoltCharacterContactListener> characterContactListener_;
	};
	REGISTER_COMPONENT(CharacterController, "Physics");
}
