#include <PhysicsEngine/Softbody/Softbody.h>
#include <PhysicsEngine/Physics/Physics.h>
#include <PhysicsEngine/JoltPhysics/JoltLayerdef.h>
#include <FoundationEngine/ECS/World.h>
#include <FoundationEngine/ECS/Actor.h>
#include <FoundationEngine/ECS/Component/Position.h>
#include <FoundationEngine/ECS/Component/Rotation.h>
#include <GraphicsEngine/Model/Crister.h>
#include <GraphicsEngine/Model/Mesh.h>

namespace SeedCore
{
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
	void Softbody::OnAwake()
	{
		/// [EN] Without a model there is nothing to build, so the soft body never waits.
		/// [JP] モデルが無ければ構築するものが無いので、待ち状態にしない。
		const Mesh* mesh = GetActor().GetComponent<Mesh>();
		pending_ = mesh != nullptr && mesh->meshID_ != 0;
	}

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
	void Softbody::OnFixedTick(Float elapsedTime)
	{
		if (bodyID_.IsInvalid())
		{
			return;
		}

		/// [EN] Jolt returns world positions.
		/// [JP] Jolt はワールド位置を返す。
		GetActor().GetPhysics().VertexPositionList(bodyID_, vertexPositions_);

		/// [EN] The render mesh is drawn with the actor's world matrix, so the positions are taken back to local space.
		/// [JP] 描画メッシュは Actor のワールド行列で描かれるので、位置をローカル空間へ戻す。
		Matrix inverseWorld = GetActor().GetWorldMatrix().Invert();
		for (Vector3& position : vertexPositions_)
		{
			position = Vector3::Transform(position, inverseWorld);
		}
	}

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
	void Softbody::OnDestroy()
	{
		if (bodyID_.IsInvalid())
		{
			return;
		}

		GetActor().GetPhysics().DestroyBody(bodyID_);

		/// [EN] An invalid ID and empty positions make the renderer fall back to the bind pose.
		/// [JP] 無効な ID と空の頂点位置で、描画はバインドポーズへ戻る。
		bodyID_ = JPH::BodyID();
		vertexPositions_.clear();

		/// [EN] Same condition as OnAwake.
		/// [JP] OnAwake と同じ条件。
		const Mesh* mesh = GetActor().GetComponent<Mesh>();
		pending_ = mesh != nullptr && mesh->meshID_ != 0;
	}

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
	JPH::BodyID Softbody::BodyID()const
	{
		return bodyID_;
	}

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
	const DynamicArray<Vector3>& Softbody::VertexPositionList()const
	{
		return vertexPositions_;
	}

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
	void Softbody::Build(const Crister& crister)
	{
		Actor actor = GetActor();

		/// [EN] Simulates the coarsest cluster of each SubMesh; the full-resolution mesh is far beyond what the solver is meant for.
		/// [JP] 各 SubMesh の最も粗いクラスタをシミュレートする。フル解像度のメッシュはソルバが想定する規模を大きく超える。
		DynamicArray<Vertex> vertices;
		SoftbodyDesc desc;
		if (!crister.SoftbodyCoarsestVertices(vertices, desc.indices_))
		{
			return;
		}

		/// [EN] Only the positions are simulated.
		/// [JP] シミュレートするのは位置だけ。
		desc.positions_.reserve(vertices.size());
		for (const Vertex& vertex : vertices)
		{
			desc.positions_.push_back(vertex.position_);
		}

		/// [EN] A soft body always moves, so it sits on the DYNAMIC layer of the actor's layer.
		/// [JP] ソフトボディは常に動くので、Actor のレイヤーの DYNAMIC に置く。
		desc.layer_ = Layers::Pack(Layers::DYNAMIC, actor.GetLayer());

		/// [EN] Stiffness 1 becomes compliance 0 (rigid) and stiffness 0 becomes 1e-4 (soft).
		/// [JP] 硬さ 1 はコンプライアンス 0(硬い)に、硬さ 0 は 1e-4(柔らかい)になる。
		desc.edgeCompliance_ = (1.0f - Clamp(edgeStiffness_, 0.0f, 1.0f)) * 1.0e-4f;
		desc.shearCompliance_ = (1.0f - Clamp(areaStiffness_, 0.0f, 1.0f)) * 1.0e-4f;
		desc.bendCompliance_ = (1.0f - Clamp(bendStiffness_, 0.0f, 1.0f)) * 1.0e-4f;

		/// [EN] Solver and material settings; gravity off is a gravity factor of 0.
		/// [JP] ソルバと材質の設定。重力を切るのは、重力の倍率を 0 にすることと同じ。
		desc.numIterations_ = static_cast<Uint32>(iterationCount_);
		desc.linearDamping_ = damping_;
		desc.pressure_ = pressure_;
		desc.friction_ = friction_;
		desc.restitution_ = restitution_;
		desc.gravityFactor_ = useGravity_ ? gravityScale_ : 0.0f;

		/// [EN] Lets physics queries and contacts find their way back to the actor.
		/// [JP] 物理クエリや接触から Actor へたどり着けるようにする。
		desc.userData_ = actor.GetEntity().GetID();

		/// [EN] Rotation holds Euler angles in degrees: x pitch, y yaw, z roll.
		/// [JP] Rotation は度単位のオイラー角で、x がピッチ、y がヨー、z がロール。
		const Position* position = actor.GetComponent<Position>();
		const Rotation* rotation = actor.GetComponent<Rotation>();
		desc.position_ = position ? Vector3(position->x_, position->y_, position->z_) : Vector3(0.0f, 0.0f, 0.0f);
		desc.rotation_ = rotation ? Quaternion::CreateFromYawPitchRoll(ToRadians(rotation->y_), ToRadians(rotation->x_), ToRadians(rotation->z_)) : Quaternion::Identity;

		/// [EN] On failure the soft body stays pending and is tried again next frame.
		/// [JP] 失敗したら構築待ちのまま残り、次のフレームで再び試す。
		bodyID_ = actor.GetPhysics().CreateSoftbody(desc);
		if (bodyID_.IsInvalid())
		{
			return;
		}

		pending_ = false;
	}

	/**
	* [EN]
	* Reports whether the soft body is still waiting for its model.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* ソフトボディがまだモデルを待っているかを返す。
	*/
	Bool Softbody::Pending()const
	{
		return pending_;
	}
}
