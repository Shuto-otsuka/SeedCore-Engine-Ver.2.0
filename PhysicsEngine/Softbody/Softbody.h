#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/SeedScript.h>

namespace SeedCore
{
	class Crister;

	/**
	* [EN]
	* Component that simulates its actor's Mesh as a Jolt soft body. The
	* body is built from a coarse proxy of the model once it has loaded,
	* and each fixed step the proxy's vertices are read back in local
	* space for SoftbodyMesh to deform the render mesh with.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* Actor の Mesh を Jolt のソフトボディとしてシミュレートするコンポーネント。
	* モデルの読み込み後に粗いプロキシからボディを作り、固定ステップごとに
	* プロキシの頂点をローカル空間で読み戻す。SoftbodyMesh はそれで描画
	* メッシュを変形させる。
	*/
	class SEEDCORE_API Softbody :public SeedScript
	{
	private:
		friend class PhysicsSystem;

	public:
		/// [EN] Mass in kilograms; not passed to the simulation yet.
		/// [JP] 質量(kg)。まだシミュレーションには渡していない。
		SC_REFLECTION_CLAMPED_EX("質量", 0.001f, 100.0f)
		Float mass_ = 1.0f;

		/// [EN] Resistance to shearing of each face: 1 is rigid, 0 the softest.
		/// [JP] 各面のせん断に対する硬さ。1 で硬く、0 で最も柔らかい。
		SC_REFLECTION_CLAMPED_EX("面積弾性力", 0.0f, 1.0f)
		Float areaStiffness_ = 0.5f;

		/// [EN] Resistance to volume change; not passed to the simulation yet.
		/// [JP] 体積変化に対する硬さ。まだシミュレーションには渡していない。
		SC_REFLECTION_CLAMPED_EX("体積弾性力", 0.0f, 1.0f)
		Float volumeStiffness_ = 0.5f;

		/// [EN] Damping that slows the vertices' velocity over time.
		/// [JP] 頂点の速度を時間とともに弱める減衰。
		SC_REFLECTION_CLAMPED_EX("抵抗力", 0.0f, 1.0f)
		Float damping_ = 0.5f;

		/// [EN] Poisson's ratio of the material; not passed to the simulation yet.
		/// [JP] 材質のポアソン比。まだシミュレーションには渡していない。
		SC_REFLECTION_CLAMPED_EX("ポアソン比", 0.0f, 0.5f)
		Float poissonRatio_ = 0.3f;

		/// [EN] How far a vertex may drift from its bind pose; not passed to the simulation yet.
		/// [JP] 頂点がバインドポーズから離れてよい距離。まだシミュレーションには渡していない。
		SC_REFLECTION_CLAMPED_EX("最大許容距離", 0.0f, 10.0f)
		Float maxDistance_ = 0.0f;

		/// [EN] Resistance to stretching along each edge: 1 is rigid, 0 the softest.
		/// [JP] 各辺の伸びに対する硬さ。1 で硬く、0 で最も柔らかい。
		SC_REFLECTION_CLAMPED_EX("辺補強係数", 0.0f, 1.0f)
		Float edgeStiffness_ = 0.5f;

		/// [EN] Resistance to bending between neighbouring faces: 1 is rigid, 0 the softest.
		/// [JP] 隣り合う面の間の曲げに対する硬さ。1 で硬く、0 で最も柔らかい。
		SC_REFLECTION_CLAMPED_EX("曲耐性", 0.0f, 1.0f)
		Float bendStiffness_ = 0.5f;

		/// [EN] Pressure inside a closed mesh; positive inflates it, negative deflates it.
		/// [JP] 閉じたメッシュの内部圧力。正で膨らみ、負でしぼむ。
		SC_REFLECTION_CLAMPED_EX("内部圧力", -10.0f, 10.0f)
		Float pressure_ = 0.0f;

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

		/// [EN] Number of substeps per physics step; not passed to the simulation yet.
		/// [JP] 物理ステップ1回あたりのサブステップ数。まだシミュレーションには渡していない。
		SC_REFLECTION_CLAMPED_EX("サブステップ数", 1, 20)
		Int subSteps_ = 4;

		/// [EN] Solver iterations per step; more iterations hold the constraints more firmly.
		/// [JP] ステップごとのソルバの反復回数。多いほど拘束がしっかり保たれる。
		SC_REFLECTION_CLAMPED_EX("反復回数", 1, 10)
		Int iterationCount_ = 4;

	public:
		/**
		* [EN]
		* Marks the soft body as waiting to be built when the actor has a
		* Mesh with an asset assigned.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* Actor にアセット付きの Mesh があれば、ソフトボディを構築待ちにする。
		*/
		void OnAwake();

		/**
		* [EN]
		* Reads the simulated vertex positions back and moves them into the
		* actor's local space.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* シミュレーション後の頂点位置を読み戻し、Actor のローカル空間へ移す。
		*/
		void OnFixedTick(Float elapsedTime);

		/**
		* [EN]
		* Destroys the body and returns to waiting, so the soft body is
		* built again if the actor wakes up once more.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* ボディを破棄して構築待ちに戻す。Actor が再び起動すれば、
		* ソフトボディはもう一度構築される。
		*/
		void OnDestroy();

	public:
		/**
		* [EN]
		* Returns the Jolt ID of the body; invalid until Build succeeds and
		* after OnDestroy.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* ボディの Jolt ID を返す。Build が成功するまでと OnDestroy 後は無効な ID。
		*/
		JPH::BodyID BodyID()const;

		/**
		* [EN]
		* Returns this step's simulated positions of the coarse proxy's
		* vertices, in the actor's local space. SoftbodyMesh transfers their
		* displacement onto the full-resolution render mesh.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* このステップでシミュレートした粗いプロキシの頂点位置を、Actor の
		* ローカル空間で返す。SoftbodyMesh がその変位をフル解像度の描画
		* メッシュへ移す。
		*/
		const DynamicArray<Vector3>& VertexPositionList()const;

	private:
		/**
		* [EN]
		* Builds the body from the model's coarse proxy mesh, the actor's
		* pose and this component's settings. Stays pending if it fails.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* モデルの粗いプロキシメッシュ、Actor の姿勢、このコンポーネントの
		* 設定からボディを作る。失敗した場合は構築待ちのまま残る。
		*/
		void Build(const Crister& crister);

		/**
		* [EN]
		* Reports whether the soft body is still waiting for its model.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* ソフトボディがまだモデルを待っているかを返す。
		*/
		Bool Pending()const;

	private:
		/// [EN] Jolt ID of the body.
		/// [JP] ボディの Jolt ID。
		JPH::BodyID bodyID_;

		/// [EN] Simulated proxy vertex positions in the actor's local space, refreshed every fixed step.
		/// [JP] Actor のローカル空間での、シミュレート済みプロキシ頂点位置。固定ステップごとに更新する。
		DynamicArray<Vector3> vertexPositions_;

		/// [EN] Whether the body still has to be built once the model is loaded.
		/// [JP] モデルの読み込み後に、まだボディを構築する必要があるか。
		Bool pending_ = true;
	};
	REGISTER_COMPONENT(Softbody, "Physics");
}
