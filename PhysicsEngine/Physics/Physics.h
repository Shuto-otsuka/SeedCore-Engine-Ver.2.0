#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/ECS/EcsID.h>
#include <FoundationEngine/Utility/Handle.h>

namespace SeedCore
{
	class JoltManager;

	/**
	* [EN]
	* Describes the initial state of a virtual character.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 仮想キャラクターの初期状態を表す。
	*/
	struct CharacterDesc
	{
		Vector3 position_ = { 0.0f, 0.0f, 0.0f };
		Quaternion rotation_ = Quaternion::Identity;
		Float radius_ = 0.3f;
		Float height_ = 1.8f;
		Float maxSlopeAngle_ = ToRadians(50.0f);
		Float mass_ = 70.0f;
		Float maxStrength_ = 100.0f;
		JPH::ObjectLayer layer_ = 0;
		EntityID userData_;
	};

	/**
	* [EN]
	* Describes the shape and physical properties of a rigid body.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 剛体の形状と物理特性を表す。
	*/
	struct RigidbodyDesc
	{
		Handle<JPH::Shape> shape_;
		Vector3 position_ = { 0.0f, 0.0f, 0.0f };
		Quaternion rotation_ = Quaternion::Identity;
		JPH::EMotionType motionType_ = JPH::EMotionType::Dynamic;
		Bool continuousCollision_ = false;
		JPH::ObjectLayer layer_ = 0;
		Float mass_ = 1.0f;
		Float linearDamping_ = 0.05f;
		Float angularDamping_ = 0.05f;
		Float friction_ = 0.2f;
		Float restitution_ = 0.0f;
		Float gravityFactor_ = 1.0f;
		JPH::EAllowedDOFs allowedDOFs_ = JPH::EAllowedDOFs::All;
		EntityID userData_;
		Bool isSensor_ = false;
	};

	/**
	* [EN]
	* Describes the anchor, axis, and angular limits of a hinge joint.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* ヒンジジョイントのアンカー、軸、角度制限を表す。
	*/
	struct HingeJointDesc
	{
		Vector3 anchor_ = { 0.0f, 0.0f, 0.0f };
		Vector3 axis_ = { 0.0f, 1.0f, 0.0f };
		Bool useLimits_ = false;
		Float minAngle_ = 0.0f;
		Float maxAngle_ = 0.0f;
	};

	/**
	* [EN]
	* Describes a fixed joint using automatically detected attachment points.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 接続点を自動検出する固定ジョイントを表す。
	*/
	struct FixedJointDesc
	{
		/// No Code
	};

	/**
	* [EN]
	* Describes the distance range and spring response of a spring joint.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* スプリングジョイントの距離範囲とばね応答を表す。
	*/
	struct SpringJointDesc
	{
		Vector3 anchor_ = { 0.0f, 0.0f, 0.0f };
		Float minDistance_ = 0.0f;
		Float maxDistance_ = 0.0f;
		Float frequency_ = 2.0f;
		Float damping_ = 0.5f;
	};

	/**
	* [EN]
	* Describes the axis and translation limits of a slider joint.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* スライダージョイントの軸と移動制限を表す。
	*/
	struct SliderJointDesc
	{
		Vector3 axis_ = { 1.0f, 0.0f, 0.0f };
		Bool useLimits_ = false;
		Float minDistance_ = 0.0f;
		Float maxDistance_ = 0.0f;
	};

	/**
	* [EN]
	* Stores the result of a three-dimensional physics query.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 3次元物理クエリの結果を保持する。
	*/
	struct RaycastHit
	{
		Vector3 position_ = { 0.0f, 0.0f, 0.0f };
		Vector3 normal_ = { 0.0f, 0.0f, 0.0f };
		Float distance_ = 0.0f;
		EntityID entityID_;
	};

	/**
	* [EN]
	* Stores the result of a two-dimensional physics query.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 2次元物理クエリの結果を保持する。
	*/
	struct RaycastHit2D
	{
		Vector2 position_ = { 0.0f, 0.0f };
		Vector2 normal_ = { 0.0f, 0.0f };
		Float distance_ = 0.0f;
		EntityID entityID_;
	};

	/**
	* [EN]
	* Describes the mesh and physical properties of a soft body.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* ソフトボディのメッシュと物理特性を表す。
	*/
	struct SoftbodyDesc
	{
		DynamicArray<Vector3> positions_;
		DynamicArray<Uint32> indices_;
		Vector3 position_ = { 0.0f, 0.0f, 0.0f };
		Quaternion rotation_ = Quaternion::Identity;
		JPH::ObjectLayer layer_ = 0;
		Float edgeCompliance_ = 0.0f;
		Float shearCompliance_ = 0.0f;
		Float bendCompliance_ = 0.0f;
		Uint32 numIterations_ = 5;
		Float linearDamping_ = 0.1f;
		Float pressure_ = 0.0f;
		Float friction_ = 0.2f;
		Float restitution_ = 0.0f;
		Float gravityFactor_ = 1.0f;
		EntityID userData_;
	};

	/**
	* [EN]
	* Provides engine-facing creation, control, and query operations for Jolt physics.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* Jolt物理に対するエンジン向けの生成、制御、クエリ操作を提供する。
	*/
	class Physics
	{
	public:
		/**
		* [EN]
		* Constructs a physics facade connected to the engine Jolt manager.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* エンジンのJolt管理機構へ接続された物理ファサードを構築する。
		*/
		Physics();

		/**
		* [EN]
		* Destroys the physics facade.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 物理ファサードを破棄する。
		*/
		~Physics() = default;

	public:
		/**
		* [EN]
		* Creates or reuses a box collision shape.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* ボックス衝突形状を生成または再利用する。
		*/
		Handle<JPH::Shape> CreateBoxShape(const Vector3& size, const Vector3& center = { 0.0f, 0.0f, 0.0f });

		/**
		* [EN]
		* Creates or reuses a sphere collision shape.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 球衝突形状を生成または再利用する。
		*/
		Handle<JPH::Shape> CreateSphereShape(Float radius);

		/**
		* [EN]
		* Creates or reuses a capsule collision shape.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* カプセル衝突形状を生成または再利用する。
		*/
		Handle<JPH::Shape> CreateCapsuleShape(Float height, Float radius);

		/**
		* [EN]
		* Creates or reuses a cylinder collision shape.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 円柱衝突形状を生成または再利用する。
		*/
		Handle<JPH::Shape> CreateCylinderShape(Float height, Float radius);

		/**
		* [EN]
		* Creates or reuses a rectangular 2D collision shape.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 矩形の2D衝突形状を生成または再利用する。
		*/
		Handle<JPH::Shape> CreateRectShape(const Vector2& size, const Vector2& center = { 0.0f, 0.0f });

		/**
		* [EN]
		* Creates or reuses a circular 2D collision shape.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 円形の2D衝突形状を生成または再利用する。
		*/
		Handle<JPH::Shape> CreateCircleShape(Float radius, const Vector2& center = { 0.0f, 0.0f });

		/**
		* [EN]
		* Creates or reuses a triangle mesh collision shape.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 三角形メッシュ衝突形状を生成または再利用する。
		*/
		Handle<JPH::Shape> CreateMeshShape(Uint32 assetID, const DynamicArray<Vector3>& positions, const DynamicArray<Uint32>& indices);

		/**
		* [EN]
		* Creates or reuses a convex hull collision shape.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 凸包衝突形状を生成または再利用する。
		*/
		Handle<JPH::Shape> CreateConvexShape(Uint32 assetID, const DynamicArray<Vector3>& positions);

		/**
		* [EN]
		* Releases one reference to a pooled collision shape.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* プールされた衝突形状への参照を一つ解放する。
		*/
		void ReleaseShape(Handle<JPH::Shape> handle);

	public:
		/**
		* [EN]
		* Creates a virtual character from the supplied description.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 指定した情報から仮想キャラクターを生成する。
		*/
		JPH::Ref<JPH::CharacterVirtual> CreateCharacter(const CharacterDesc& desc);

		/**
		* [EN]
		* Advances character movement and stair handling for one frame.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 1フレーム分のキャラクター移動と段差処理を更新する。
		*/
		void UpdateCharacter(JPH::CharacterVirtual* character, Float elapsedTime, Float maxSlopeAngle, Float stepHeight);

		/**
		* [EN]
		* Refreshes the contacts of a virtual character.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 仮想キャラクターの接触情報を更新する。
		*/
		void RefreshCharacter(JPH::CharacterVirtual* character);

		/**
		* [EN]
		* Releases a virtual character reference.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 仮想キャラクターへの参照を解放する。
		*/
		void DestroyCharacter(JPH::Ref<JPH::CharacterVirtual>& character);

		/**
		* [EN]
		* Replaces the character shape using the specified height and radius.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 指定した高さと半径でキャラクター形状を置き換える。
		*/
		Bool CharacterHeight(JPH::CharacterVirtual* character, Float height, Float radius);

		/**
		* [EN]
		* Returns the gravity vector used by the physics world.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 物理ワールドで使用する重力ベクトルを返す。
		*/
		Vector3 Gravity()const;

	public:
		/**
		* [EN]
		* Creates and activates a rigid body.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 剛体を生成して有効化する。
		*/
		JPH::BodyID CreateRigidbody(const RigidbodyDesc& desc);

		/**
		* [EN]
		* Creates and activates a soft body from mesh data.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* メッシュデータからソフトボディを生成して有効化する。
		*/
		JPH::BodyID CreateSoftbody(const SoftbodyDesc& desc);

		/**
		* [EN]
		* Removes a body from simulation without destroying it.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* ボディを破棄せずシミュレーションから外す。
		*/
		void SuspendBody(JPH::BodyID bodyID);

		/**
		* [EN]
		* Adds a suspended body back to simulation.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 停止中のボディをシミュレーションへ戻す。
		*/
		void ResumeBody(JPH::BodyID bodyID);

		/**
		* [EN]
		* Removes and destroys a physics body.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 物理ボディを削除して破棄する。
		*/
		void DestroyBody(JPH::BodyID bodyID);

		/**
		* [EN]
		* Replaces the collision shape assigned to a body. A dynamic body
		* keeps its mass; only its inertia follows the new shape.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* ボディに割り当てられた衝突形状を置き換える。動的ボディは質量を保ち、
		* 慣性だけを新しい形状に合わせる。
		*/
		void BodyShape(JPH::BodyID bodyID, Handle<JPH::Shape> shape);

		/**
		* [EN]
		* Writes the world position and rotation of a body.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* ボディのワールド位置と回転を書き出す。
		*/
		void BodyTransform(JPH::BodyID bodyID, Vector3& outPosition, Quaternion& outRotation)const;

		/**
		* [EN]
		* Writes the world positions of all soft-body vertices.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* ソフトボディの全頂点のワールド位置を書き出す。
		*/
		void VertexPositionList(JPH::BodyID bodyID, DynamicArray<Vector3>& outPositions)const;

		/**
		* [EN]
		* Returns the entity identifier stored in a body.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* ボディに格納されたエンティティ識別子を返す。
		*/
		EntityID BodyEntityID(JPH::BodyID bodyID)const;

		/**
		* [EN]
		* Returns the identifiers of all physics bodies.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* すべての物理ボディ識別子を返す。
		*/
		DynamicArray<JPH::BodyID> BodyList()const;

	public:
		/**
		* [EN]
		* Creates a hinge constraint between two bodies.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 2つのボディ間にヒンジ拘束を生成する。
		*/
		Handle<JPH::Constraint> CreateHingeJoint(JPH::BodyID bodyA, JPH::BodyID bodyB, const HingeJointDesc& desc);

		/**
		* [EN]
		* Creates a fixed constraint between two bodies.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 2つのボディ間に固定拘束を生成する。
		*/
		Handle<JPH::Constraint> CreateFixedJoint(JPH::BodyID bodyA, JPH::BodyID bodyB, const FixedJointDesc& desc);

		/**
		* [EN]
		* Creates a spring distance constraint between two bodies.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 2つのボディ間にばね距離拘束を生成する。
		*/
		Handle<JPH::Constraint> CreateSpringJoint(JPH::BodyID bodyA, JPH::BodyID bodyB, const SpringJointDesc& desc);

		/**
		* [EN]
		* Creates a slider constraint between two bodies.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 2つのボディ間にスライダー拘束を生成する。
		*/
		Handle<JPH::Constraint> CreateSliderJoint(JPH::BodyID bodyA, JPH::BodyID bodyB, const SliderJointDesc& desc);

		/**
		* [EN]
		* Releases a joint constraint.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* ジョイント拘束を解放する。
		*/
		void DestroyJoint(Handle<JPH::Constraint> handle);

		/**
		* [EN]
		* Enables each joint whose bodies are both in the simulation and
		* disables the rest, so suspended bodies do not drag their joints.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 両方のボディがシミュレーション中のジョイントを有効にし、それ以外を
		* 無効にする。停止中のボディにジョイントが引きずられないようにする。
		*/
		void RefleshJoint();

	public:
		/**
		* [EN]
		* Finds the nearest 3D body intersected by a ray.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* レイと交差する最も近い3Dボディを検索する。
		*/
		Bool Raycast(const Vector3& origin, const Vector3& direction, Float maxDistance, RaycastHit& outHit, Uint32 layerMask = 0xFFFFFFFF)const;

		/**
		* [EN]
		* Finds the nearest 3D body intersected by a swept sphere.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 移動する球と交差する最も近い3Dボディを検索する。
		*/
		Bool Spherecast(const Vector3& origin, Float radius, const Vector3& direction, Float maxDistance, RaycastHit& outHit, Uint32 layerMask = 0xFFFFFFFF)const;

		/**
		* [EN]
		* Returns entities overlapping a 3D query shape.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 3Dクエリ形状と重なるエンティティを返す。
		*/
		DynamicArray<EntityID> Overlap(Handle<JPH::Shape> shape, const Vector3& position, const Quaternion& rotation, Uint32 layerMask = 0xFFFFFFFF)const;

		/**
		* [EN]
		* Finds the nearest planar body intersected by a 2D ray.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 2Dレイと交差する最も近い平面ボディを検索する。
		*/
		Bool Raycast2D(const Vector2& origin, const Vector2& direction, Float maxDistance, RaycastHit2D& outHit, Uint32 layerMask = 0xFFFFFFFF)const;

		/**
		* [EN]
		* Finds the nearest planar body intersected by a swept circle.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 移動する円と交差する最も近い平面ボディを検索する。
		*/
		Bool Circlecast2D(const Vector2& origin, Float radius, const Vector2& direction, Float maxDistance, RaycastHit2D& outHit, Uint32 layerMask = 0xFFFFFFFF)const;

		/**
		* [EN]
		* Returns entities overlapping a 2D query shape.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 2Dクエリ形状と重なるエンティティを返す。
		*/
		DynamicArray<EntityID> Overlap2D(Handle<JPH::Shape> shape, const Vector2& position, Float rotation, Uint32 layerMask = 0xFFFFFFFF)const;

	private:
		/**
		* [EN]
		* Creates and registers a two-body constraint.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 2ボディ拘束を生成して登録する。
		*/
		Handle<JPH::Constraint> CreateConstraint(JPH::BodyID bodyA, JPH::BodyID bodyB, const JPH::TwoBodyConstraintSettings& settings);

	private:
		/// [EN] Number of screen pixels represented by one physics meter.
		/// [JP] 物理空間の1メートルに対応する画面上のピクセル数。
		static constexpr Float pixelsPerMeter_ = 100.0f;

		/// [EN] Jolt services and pooled physics resources used by this facade.
		/// [JP] このファサードが使用するJoltサービスと物理リソースプール。
		JoltManager& joltManager_;
	};
}
