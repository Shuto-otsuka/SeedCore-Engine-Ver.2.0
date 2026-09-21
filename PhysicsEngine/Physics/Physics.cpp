#include <PhysicsEngine/Physics/Physics.h>
#include <PhysicsEngine/JoltPhysics/JoltManager.h>
#include <PhysicsEngine/JoltPhysics/JoltLayerdef.h>
#include <FoundationEngine/Resource/Gateway.h>
#include <FoundationEngine/Log/Warning.h>
#include <FoundationEngine/World/World.h>
#include <FoundationEngine/World/Actor/Actor.h>

namespace SeedCore
{
	/**
	* [EN]
	* Constructs a physics facade connected to the engine Jolt manager.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* エンジンのJolt管理機構へ接続された物理ファサードを構築する。
	*/
	Physics::Physics() :joltManager_(Gateway::GetJoltManager())
	{
		/// No Code
	}

	/**
	* [EN]
	* Creates or reuses a box collision shape.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* ボックス衝突形状を生成または再利用する。
	*/
	Handle<JPH::Shape> Physics::CreateBoxShape(const Vector3& size, const Vector3& center)
	{
		return joltManager_.ShapePool().CreateBoxShape(size, center);
	}

	/**
	* [EN]
	* Creates or reuses a sphere collision shape.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 球衝突形状を生成または再利用する。
	*/
	Handle<JPH::Shape> Physics::CreateSphereShape(Float radius)
	{
		return joltManager_.ShapePool().CreateSphereShape(radius);
	}

	/**
	* [EN]
	* Creates or reuses a capsule collision shape.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* カプセル衝突形状を生成または再利用する。
	*/
	Handle<JPH::Shape> Physics::CreateCapsuleShape(Float height, Float radius)
	{
		return joltManager_.ShapePool().CreateCapsuleShape(height, radius);
	}

	/**
	* [EN]
	* Creates or reuses a cylinder collision shape.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 円柱衝突形状を生成または再利用する。
	*/
	Handle<JPH::Shape> Physics::CreateCylinderShape(Float height, Float radius)
	{
		return joltManager_.ShapePool().CreateCylinderShape(height, radius);
	}

	/**
	* [EN]
	* Creates or reuses a rectangular 2D collision shape.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 矩形の2D衝突形状を生成または再利用する。
	*/
	Handle<JPH::Shape> Physics::CreateRectShape(const Vector2& size, const Vector2& center)
	{
		/// [EN] Canvas pixels (Y down) become physics meters (Y up), so the center's Y is negated.
		/// [JP] Canvas のピクセル(Y 下向き)を物理のメートル(Y 上向き)へ直すため、中心の Y を反転する。
		return joltManager_.ShapePool().CreateRectShape(Vector2(size.x / pixelsPerMeter_, size.y / pixelsPerMeter_), Vector2(center.x / pixelsPerMeter_, -center.y / pixelsPerMeter_));
	}

	/**
	* [EN]
	* Creates or reuses a circular 2D collision shape.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 円形の2D衝突形状を生成または再利用する。
	*/
	Handle<JPH::Shape> Physics::CreateCircleShape(Float radius, const Vector2& center)
	{
		/// [EN] Same pixel-to-meter conversion as CreateRectShape.
		/// [JP] CreateRectShape と同じ、ピクセルからメートルへの変換。
		return joltManager_.ShapePool().CreateCircleShape(radius / pixelsPerMeter_, Vector2(center.x / pixelsPerMeter_, -center.y / pixelsPerMeter_));
	}

	/**
	* [EN]
	* Creates or reuses a triangle mesh collision shape.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 三角形メッシュ衝突形状を生成または再利用する。
	*/
	Handle<JPH::Shape> Physics::CreateMeshShape(Uint32 assetID, const DynamicArray<Vector3>& positions, const DynamicArray<Uint32>& indices)
	{
		return joltManager_.ShapePool().CreateMeshShape(assetID, positions, indices);
	}

	/**
	* [EN]
	* Creates or reuses a convex hull collision shape.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 凸包衝突形状を生成または再利用する。
	*/
	Handle<JPH::Shape> Physics::CreateConvexShape(Uint32 assetID, const DynamicArray<Vector3>& positions)
	{
		return joltManager_.ShapePool().CreateConvexShape(assetID, positions);
	}

	/**
	* [EN]
	* Releases one reference to a pooled collision shape.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* プールされた衝突形状への参照を一つ解放する。
	*/
	void Physics::ReleaseShape(Handle<JPH::Shape> handle)
	{
		joltManager_.ShapePool().Release(handle);
	}

	/**
	* [EN]
	* Creates a virtual character from the supplied description.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 指定した情報から仮想キャラクターを生成する。
	*/
	JPH::Ref<JPH::CharacterVirtual> Physics::CreateCharacter(const CharacterDesc& desc)
	{
		/// [EN] Jolt's capsule takes the half height of its cylinder part; the caps add radius at each end.
		/// [JP] Jolt のカプセルは円柱部分の半分の高さを取り、両端に半径分の半球が付く。
		JPH::RefConst<JPH::Shape> capsule = new JPH::CapsuleShape(desc.height_ * 0.5f, desc.radius_);

		/// [EN] Lift the capsule so its bottom sits on the character's origin, i.e. the origin is the feet.
		/// [JP] カプセルの底がキャラクターの原点に来るよう持ち上げ、原点を足元にする。
		JPH::RefConst<JPH::Shape> shape = new JPH::RotatedTranslatedShape(JPH::Vec3(0.0f, desc.height_ * 0.5f + desc.radius_, 0.0f), JPH::Quat::sIdentity(), capsule);

		/// [EN] Slope limit, mass and the maximum push force applied to other bodies.
		/// [JP] 登れる傾斜の上限、質量、他のボディを押す力の上限。
		JPH::CharacterVirtualSettings settings;
		settings.mShape = shape;
		settings.mMaxSlopeAngle = desc.maxSlopeAngle_;
		settings.mMass = desc.mass_;
		settings.mMaxStrength = desc.maxStrength_;

		/// [EN] The EntityID is stored bit for bit as the character's user data.
		/// [JP] EntityID はビットのままキャラクターのユーザーデータに格納する。
		return new JPH::CharacterVirtual(&settings, JPH::RVec3(desc.position_.x, desc.position_.y, desc.position_.z), JPH::Quat(desc.rotation_.x, desc.rotation_.y, desc.rotation_.z, desc.rotation_.w), std::bit_cast<JPH::uint64>(desc.userData_), &joltManager_.PhysicsSystem());
	}

	/**
	* [EN]
	* Advances character movement and stair handling for one frame.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 1フレーム分のキャラクター移動と段差処理を更新する。
	*/
	void Physics::UpdateCharacter(JPH::CharacterVirtual* character, Float elapsedTime, Float maxSlopeAngle, Float stepHeight)
	{
		if (!character)
		{
			return;
		}

		/// [EN] Applied every frame so changes to the slope limit take effect immediately.
		/// [JP] 傾斜の上限の変更がすぐ反映されるよう、毎フレーム設定する。
		character->SetMaxSlopeAngle(maxSlopeAngle);

		JPH::PhysicsSystem& physicsSystem = joltManager_.PhysicsSystem();

		/// [EN] Steps up to stepHeight are climbed instead of blocking the character.
		/// [JP] stepHeight までの段差は、行く手を塞がずに登らせる。
		JPH::CharacterVirtual::ExtendedUpdateSettings updateSettings;
		updateSettings.mWalkStairsStepUp = JPH::Vec3(0.0f, stepHeight, 0.0f);

		/// [EN] Moves under world gravity, colliding as a DYNAMIC body; no body or shape filters.
		/// [JP] ワールドの重力で移動し、DYNAMIC ボディとして衝突する。ボディや形状のフィルタは使わない。
		character->ExtendedUpdate(elapsedTime, physicsSystem.GetGravity(), updateSettings, physicsSystem.GetDefaultBroadPhaseLayerFilter(Layers::DYNAMIC), physicsSystem.GetDefaultLayerFilter(Layers::DYNAMIC), { }, { }, joltManager_.PhysicsAllocator());
	}

	/**
	* [EN]
	* Refreshes the contacts of a virtual character.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 仮想キャラクターの接触情報を更新する。
	*/
	void Physics::RefreshCharacter(JPH::CharacterVirtual* character)
	{
		if (!character)
		{
			return;
		}

		JPH::PhysicsSystem& physicsSystem = joltManager_.PhysicsSystem();

		/// [EN] Re-collects contacts at the current position with the same filters as UpdateCharacter.
		/// [JP] UpdateCharacter と同じフィルタで、現在位置の接触を集め直す。
		character->RefreshContacts(physicsSystem.GetDefaultBroadPhaseLayerFilter(Layers::DYNAMIC), physicsSystem.GetDefaultLayerFilter(Layers::DYNAMIC), { }, { }, joltManager_.PhysicsAllocator());
	}

	/**
	* [EN]
	* Releases a virtual character reference.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 仮想キャラクターへの参照を解放する。
	*/
	void Physics::DestroyCharacter(JPH::Ref<JPH::CharacterVirtual>& character)
	{
		/// [EN] The character is reference counted; dropping the last reference frees it.
		/// [JP] キャラクターは参照カウントで管理され、最後の参照を外すと解放される。
		character = nullptr;
	}

	/**
	* [EN]
	* Replaces the character shape using the specified height and radius.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 指定した高さと半径でキャラクター形状を置き換える。
	*/
	Bool Physics::CharacterHeight(JPH::CharacterVirtual* character, Float height, Float radius)
	{
		if (!character)
		{
			return false;
		}

		/// [EN] Builds the same feet-origin capsule as CreateCharacter.
		/// [JP] CreateCharacter と同じ、足元を原点とするカプセルを作る。
		JPH::RefConst<JPH::Shape> capsule = new JPH::CapsuleShape(height * 0.5f, radius);
		JPH::RefConst<JPH::Shape> shape = new JPH::RotatedTranslatedShape(JPH::Vec3(0.0f, height * 0.5f + radius, 0.0f), JPH::Quat::sIdentity(), capsule);

		JPH::PhysicsSystem& physicsSystem = joltManager_.PhysicsSystem();

		/// [EN] Refused (false) if the new shape would penetrate the surroundings deeper than 0.05 m.
		/// [JP] 新しい形状が周囲へ 0.05 m より深くめり込む場合は、置き換えずに false を返す。
		return character->SetShape(shape, 0.05f, physicsSystem.GetDefaultBroadPhaseLayerFilter(Layers::DYNAMIC), physicsSystem.GetDefaultLayerFilter(Layers::DYNAMIC), { }, { }, joltManager_.PhysicsAllocator());
	}


	/**
	* [EN]
	* Returns the gravity vector used by the physics world.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 物理ワールドで使用する重力ベクトルを返す。
	*/
	Vector3 Physics::Gravity()const
	{
		/// [EN] Converts Jolt's vector to the engine's Vector3.
		/// [JP] Jolt のベクトルをエンジンの Vector3 へ変換する。
		JPH::Vec3 gravity = joltManager_.PhysicsSystem().GetGravity();
		return Vector3(gravity.GetX(), gravity.GetY(), gravity.GetZ());
	}

	/**
	* [EN]
	* Creates and activates a rigid body.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 剛体を生成して有効化する。
	*/
	JPH::BodyID Physics::CreateRigidbody(const RigidbodyDesc& desc)
	{
		/// [EN] A handle that no longer points at a pooled shape gives an invalid BodyID.
		/// [JP] プールの形状を指さなくなったハンドルなら、無効な BodyID を返す。
		JPH::ShapeRefC shape = joltManager_.ShapePool().Get(desc.shape_);
		if (!shape)
		{
			return JPH::BodyID();
		}

		/// [EN] Shape, initial pose, motion type and the packed object layer.
		/// [JP] 形状、初期姿勢、運動タイプ、パック済みのオブジェクトレイヤー。
		JPH::BodyCreationSettings settings(shape, JPH::RVec3(desc.position_.x, desc.position_.y, desc.position_.z), JPH::Quat(desc.rotation_.x, desc.rotation_.y, desc.rotation_.z, desc.rotation_.w), desc.motionType_, desc.layer_);

		/// [EN] Material and motion properties; the EntityID is stored bit for bit as user data.
		/// [JP] 材質と運動の特性。EntityID はビットのままユーザーデータに格納する。
		settings.mAllowedDOFs = desc.allowedDOFs_;
		settings.mLinearDamping = desc.linearDamping_;
		settings.mAngularDamping = desc.angularDamping_;
		settings.mFriction = desc.friction_;
		settings.mRestitution = desc.restitution_;
		settings.mGravityFactor = desc.gravityFactor_;
		settings.mUserData = std::bit_cast<JPH::uint64>(desc.userData_);
		settings.mIsSensor = desc.isSensor_;

		/// [EN] Static bodies have no mass or motion quality.
		/// [JP] 静的ボディは質量も運動品質も持たない。
		if (desc.motionType_ != JPH::EMotionType::Static)
		{
			/// [EN] The mass is given; the inertia is derived from the shape scaled to that mass.
			/// [JP] 質量は指定値を使い、慣性はその質量に合わせて形状から求める。
			settings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
			settings.mMassPropertiesOverride.mMass = desc.mass_;

			/// [EN] Continuous collision sweeps the body along its motion so fast bodies do not tunnel.
			/// [JP] 連続衝突判定は移動経路に沿ってボディを掃引し、高速なボディのすり抜けを防ぐ。
			settings.mMotionQuality = desc.continuousCollision_ ? JPH::EMotionQuality::LinearCast : JPH::EMotionQuality::Discrete;
		}

		/// [EN] Creation fails when Jolt has run out of body slots.
		/// [JP] Jolt のボディ枠が尽きていると生成に失敗する。
		JPH::BodyInterface& bodyInterface = joltManager_.BodyInterface();
		JPH::Body* body = bodyInterface.CreateBody(settings);
		if (!body)
		{
			return JPH::BodyID();
		}

		/// [EN] Adds the body to the simulation, awake.
		/// [JP] ボディを起きた状態でシミュレーションへ加える。
		bodyInterface.AddBody(body->GetID(), JPH::EActivation::Activate);
		return body->GetID();
	}

	/**
	* [EN]
	* Creates and activates a soft body from mesh data.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* メッシュデータからソフトボディを生成して有効化する。
	*/
	JPH::BodyID Physics::CreateSoftbody(const SoftbodyDesc& desc)
	{
		/// [EN] Mesh topology and constraints, shareable between soft bodies built from the same mesh.
		/// [JP] メッシュの構造と拘束。同じメッシュから作るソフトボディ間で共有できる。
		JPH::Ref<JPH::SoftBodySharedSettings> sharedSettings = new JPH::SoftBodySharedSettings();

		/// [EN] Every mesh position becomes a simulated vertex.
		/// [JP] メッシュの各位置を、シミュレーションされる頂点にする。
		sharedSettings->mVertices.reserve(desc.positions_.size());
		for (const Vector3& position : desc.positions_)
		{
			sharedSettings->mVertices.push_back(JPH::SoftBodySharedSettings::Vertex(JPH::Float3(position.x, position.y, position.z)));
		}

		/// [EN] Upper bound for the vertex indices of each face.
		/// [JP] 各面の頂点インデックスの上限。
		Uint32 vertexCount = static_cast<Uint32>(desc.positions_.size());

		/// [EN] Every three indices form one triangle face; a trailing partial triangle is ignored.
		/// [JP] インデックス3つで三角形の面1つ。末尾の3つに満たない分は無視する。
		sharedSettings->mFaces.reserve(desc.indices_.size() / 3);
		for (Size faceIndex = 0; faceIndex + 2 < desc.indices_.size(); faceIndex += 3)
		{
			Uint32 vertex0 = desc.indices_[faceIndex];
			Uint32 vertex1 = desc.indices_[faceIndex + 1];
			Uint32 vertex2 = desc.indices_[faceIndex + 2];

			/// [EN] A face referencing a vertex that does not exist is skipped.
			/// [JP] 存在しない頂点を参照する面は飛ばす。
			if (vertex0 >= vertexCount || vertex1 >= vertexCount || vertex2 >= vertexCount)
			{
				SC_LOG_WARNING("Softbody: 頂点数(%u)を超える頂点インデックス(%u, %u, %u)を持つ面をスキップしました", vertexCount, vertex0, vertex1, vertex2);
				continue;
			}

			/// [EN] A face that repeats a vertex has no area and is skipped.
			/// [JP] 同じ頂点を重複して使う面は面積を持たないので飛ばす。
			JPH::SoftBodySharedSettings::Face face(vertex0, vertex1, vertex2);
			if (face.IsDegenerate())
			{
				continue;
			}

			sharedSettings->AddFace(face);
		}

		/// [EN] Without a single valid face there is nothing to simulate.
		/// [JP] 有効な面が1つも無ければ、シミュレーションするものが無い。
		if (sharedSettings->mFaces.empty())
		{
			return JPH::BodyID();
		}

		/// [EN] Edge, shear and bend constraints from the faces; compliance 0 is rigid, higher is softer.
		/// [JP] 面から辺・せん断・曲げの拘束を作る。コンプライアンスは 0 で硬く、大きいほど柔らかい。
		JPH::SoftBodySharedSettings::VertexAttributes vertexAttributes(desc.edgeCompliance_, desc.shearCompliance_, desc.bendCompliance_);
		sharedSettings->CreateConstraints(&vertexAttributes, 1);

		/// [EN] Reorders the constraints into groups that can be solved in parallel.
		/// [JP] 拘束を、並列に解けるグループへ並べ替える。
		sharedSettings->Optimize();

		/// [EN] Pose, layer and solver/material properties; the EntityID is stored as user data.
		/// [JP] 姿勢、レイヤー、ソルバと材質の特性。EntityID はユーザーデータに格納する。
		JPH::SoftBodyCreationSettings settings(sharedSettings, JPH::RVec3(desc.position_.x, desc.position_.y, desc.position_.z), JPH::Quat(desc.rotation_.x, desc.rotation_.y, desc.rotation_.z, desc.rotation_.w), desc.layer_);
		settings.mNumIterations = desc.numIterations_;
		settings.mLinearDamping = desc.linearDamping_;
		settings.mPressure = desc.pressure_;
		settings.mFriction = desc.friction_;
		settings.mRestitution = desc.restitution_;
		settings.mGravityFactor = desc.gravityFactor_;
		settings.mUserData = std::bit_cast<JPH::uint64>(desc.userData_);

		/// [EN] Creates the body and adds it to the simulation, awake.
		/// [JP] ボディを生成し、起きた状態でシミュレーションへ加える。
		JPH::BodyInterface& bodyInterface = joltManager_.BodyInterface();
		return bodyInterface.CreateAndAddSoftBody(settings, JPH::EActivation::Activate);
	}

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
	void Physics::BodyShape(JPH::BodyID bodyID, Handle<JPH::Shape> shape)
	{
		if (bodyID.IsInvalid())
		{
			return;
		}

		/// [EN] A handle that no longer points at a pooled shape leaves the body unchanged.
		/// [JP] プールの形状を指さなくなったハンドルなら、ボディはそのままにする。
		JPH::ShapeRefC newShape = joltManager_.ShapePool().Get(shape);
		if (!newShape)
		{
			return;
		}

		/// [EN] Swaps the shape and wakes the body; Jolt's own recompute would replace the mass with shape volume times density.
		/// [JP] 形状を差し替えてボディを起こす。Jolt の再計算に任せると、質量が形状の体積×密度に置き換わる。
		JPH::BodyInterface& bodyInterface = joltManager_.BodyInterface();
		bodyInterface.SetShape(bodyID, newShape, false, JPH::EActivation::Activate);

		/// [EN] Mass properties are edited on the body directly, so it is write-locked.
		/// [JP] 質量特性はボディを直接書き換えるので、書き込みロックをかける。
		JPH::BodyLockWrite lock(joltManager_.PhysicsSystem().GetBodyLockInterface(), bodyID);
		if (!lock.Succeeded())
		{
			return;
		}

		/// [EN] Only dynamic bodies use mass; static and kinematic ones keep what they have.
		/// [JP] 質量を使うのは動的ボディだけ。静的・キネマティックはそのままにする。
		JPH::Body& body = lock.GetBody();
		if (!body.IsDynamic())
		{
			return;
		}

		/// [EN] With every translation axis locked the inverse mass is 0 and the mass cannot be read back.
		/// [JP] 移動軸が全て固定されていると逆質量は 0 になり、質量を読み戻せない。
		JPH::MotionProperties* motionProperties = body.GetMotionProperties();
		Float inverseMass = motionProperties->GetInverseMass();
		if (inverseMass <= 0.0f)
		{
			return;
		}

		/// [EN] Keeps the current mass and derives the inertia from the new shape, as at creation.
		/// [JP] 生成時と同じく、質量は今の値を保ち、慣性は新しい形状から求める。
		JPH::MassProperties massProperties = newShape->GetMassProperties();
		massProperties.ScaleToMass(1.0f / inverseMass);
		motionProperties->SetMassProperties(motionProperties->GetAllowedDOFs(), massProperties);
	}

	/**
	* [EN]
	* Removes and destroys a physics body.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 物理ボディを削除して破棄する。
	*/
	void Physics::DestroyBody(JPH::BodyID bodyID)
	{
		if (bodyID.IsInvalid())
		{
			return;
		}

		/// [EN] A suspended body is already out of the simulation; only an added one needs removing.
		/// [JP] 停止中のボディは既にシミュレーション外なので、追加済みのものだけ外す。
		JPH::BodyInterface& bodyInterface = joltManager_.BodyInterface();
		if (bodyInterface.IsAdded(bodyID))
		{
			bodyInterface.RemoveBody(bodyID);
		}
		bodyInterface.DestroyBody(bodyID);
	}

	/**
	* [EN]
	* Writes the world position and rotation of a body.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* ボディのワールド位置と回転を書き出す。
	*/
	void Physics::BodyTransform(JPH::BodyID bodyID, Vector3& outPosition, Quaternion& outRotation)const
	{
		if (bodyID.IsInvalid())
		{
			return;
		}

		JPH::BodyInterface& bodyInterface = joltManager_.BodyInterface();
		JPH::RVec3 position = bodyInterface.GetPosition(bodyID);
		JPH::Quat rotation = bodyInterface.GetRotation(bodyID);

		/// [EN] Converts Jolt's vector and quaternion to the engine's types.
		/// [JP] Jolt のベクトルとクォータニオンをエンジンの型へ変換する。
		outPosition = Vector3(position.GetX(), position.GetY(), position.GetZ());
		outRotation = Quaternion(rotation.GetX(), rotation.GetY(), rotation.GetZ(), rotation.GetW());
	}

	/**
	* [EN]
	* Writes the world positions of all soft-body vertices.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* ソフトボディの全頂点のワールド位置を書き出す。
	*/
	void Physics::VertexPositionList(JPH::BodyID bodyID, DynamicArray<Vector3>& outPositions)const
	{
		if (bodyID.IsInvalid())
		{
			return;
		}

		/// [EN] Vertices are read directly from the body, so it is read-locked while copying.
		/// [JP] 頂点はボディから直接読むので、コピーの間は読み込みロックをかける。
		const JPH::BodyLockInterface& lockInterface = joltManager_.PhysicsSystem().GetBodyLockInterface();

		JPH::BodyLockRead lock(lockInterface, bodyID);
		if (!lock.Succeeded())
		{
			return;
		}

		/// [EN] A soft body keeps its vertices in its motion properties.
		/// [JP] ソフトボディは頂点を運動特性の中に持つ。
		const JPH::Body& body = lock.GetBody();
		const JPH::SoftBodyMotionProperties* motionProperties = static_cast<const JPH::SoftBodyMotionProperties*>(body.GetMotionPropertiesUnchecked());
		if (!motionProperties)
		{
			return;
		}

		/// [EN] Vertex positions are relative to the center of mass, so this transform takes them to world space.
		/// [JP] 頂点位置は重心からの相対なので、この変換でワールド空間へ移す。
		JPH::RMat44 transform = body.GetCenterOfMassTransform();
		const JPH::Array<JPH::SoftBodyMotionProperties::Vertex>& vertices = motionProperties->GetVertices();

		/// [EN] Replaces the previous contents of outPositions.
		/// [JP] outPositions の以前の中身は置き換える。
		outPositions.clear();
		outPositions.reserve(vertices.size());
		for (const JPH::SoftBodyMotionProperties::Vertex& vertex : vertices)
		{
			JPH::RVec3 worldPosition = transform * vertex.mPosition;
			outPositions.push_back(Vector3(static_cast<Float>(worldPosition.GetX()), static_cast<Float>(worldPosition.GetY()), static_cast<Float>(worldPosition.GetZ())));
		}
	}

	/**
	* [EN]
	* Returns the entity identifier stored in a body.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* ボディに格納されたエンティティ識別子を返す。
	*/
	EntityID Physics::BodyEntityID(JPH::BodyID bodyID)const
	{
		if (bodyID.IsInvalid())
		{
			return EntityID{};
		}

		/// [EN] A body that cannot be locked (already destroyed) yields a null EntityID.
		/// [JP] ロックできない(破棄済みの)ボディなら、空の EntityID を返す。
		const JPH::BodyLockInterface& lockInterface = joltManager_.PhysicsSystem().GetBodyLockInterface();

		JPH::BodyLockRead lock(lockInterface, bodyID);
		if (!lock.Succeeded())
		{
			return EntityID{};
		}

		/// [EN] The user data holds the EntityID bit for bit, as written at creation.
		/// [JP] ユーザーデータには、生成時に書いた EntityID がビットのまま入っている。
		return std::bit_cast<EntityID>(static_cast<Uint64>(lock.GetBody().GetUserData()));
	}

	/**
	* [EN]
	* Returns the identifiers of all physics bodies.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* すべての物理ボディ識別子を返す。
	*/
	DynamicArray<JPH::BodyID> Physics::BodyList()const
	{
		/// [EN] Includes suspended bodies, since they still exist in Jolt.
		/// [JP] 停止中のボディも Jolt 上には存在するので含まれる。
		JPH::BodyIDVector bodyIDs;
		joltManager_.PhysicsSystem().GetBodies(bodyIDs);
		return DynamicArray<JPH::BodyID>(bodyIDs.begin(), bodyIDs.end());
	}

	/**
	* [EN]
	* Removes a body from simulation without destroying it.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* ボディを破棄せずシミュレーションから外す。
	*/
	void Physics::SuspendBody(JPH::BodyID bodyID)
	{
		if (bodyID.IsInvalid())
		{
			return;
		}

		/// [EN] Removing keeps the body and its state; it only stops simulating and colliding.
		/// [JP] 外してもボディと状態は残り、シミュレーションと衝突だけが止まる。
		JPH::BodyInterface& bodyInterface = joltManager_.BodyInterface();
		if (bodyInterface.IsAdded(bodyID))
		{
			bodyInterface.RemoveBody(bodyID);
		}
	}

	/**
	* [EN]
	* Adds a suspended body back to simulation.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 停止中のボディをシミュレーションへ戻す。
	*/
	void Physics::ResumeBody(JPH::BodyID bodyID)
	{
		if (bodyID.IsInvalid())
		{
			return;
		}

		/// [EN] Re-adds the body awake; a body already in the simulation is left alone.
		/// [JP] ボディを起きた状態で戻す。既にシミュレーション中のボディはそのままにする。
		JPH::BodyInterface& bodyInterface = joltManager_.BodyInterface();
		if (!bodyInterface.IsAdded(bodyID))
		{
			bodyInterface.AddBody(bodyID, JPH::EActivation::Activate);
		}
	}

	/**
	* [EN]
	* Creates a hinge constraint between two bodies.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 2つのボディ間にヒンジ拘束を生成する。
	*/
	Handle<JPH::Constraint> Physics::CreateHingeJoint(JPH::BodyID bodyA, JPH::BodyID bodyB, const HingeJointDesc& desc)
	{
		JPH::BodyInterface& bodyInterface = joltManager_.BodyInterface();
		JPH::RVec3 bodyPosition = bodyInterface.GetPosition(bodyA);
		JPH::Quat bodyRotation = bodyInterface.GetRotation(bodyA);

		/// [EN] The anchor and axis are given in bodyA's local space; both go to world space.
		/// [JP] アンカーと軸は bodyA のローカル空間で与えられるので、どちらもワールド空間へ移す。
		JPH::Vec3 worldPosition(bodyPosition.GetX(), bodyPosition.GetY(), bodyPosition.GetZ());
		JPH::Vec3 worldAnchor = worldPosition + bodyRotation * JPH::Vec3(desc.anchor_.x, desc.anchor_.y, desc.anchor_.z);

		/// [EN] A zero-length axis falls back to world up.
		/// [JP] 長さ 0 の軸はワールドの上方向で代用する。
		JPH::Vec3 worldAxis = (bodyRotation * JPH::Vec3(desc.axis_.x, desc.axis_.y, desc.axis_.z)).NormalizedOr(JPH::Vec3::sAxisY());

		/// [EN] Any direction perpendicular to the axis; it marks angle 0 for the limits.
		/// [JP] 軸に垂直な任意の方向。角度制限における 0 度の基準になる。
		JPH::Vec3 worldNormal = worldAxis.GetNormalizedPerpendicular();

		/// [EN] Both bodies share the same pivot and axes, so the current pose is angle 0.
		/// [JP] 両ボディが同じ支点と軸を共有するので、現在の姿勢が 0 度になる。
		JPH::HingeConstraintSettings settings;
		settings.mSpace = JPH::EConstraintSpace::WorldSpace;
		settings.mPoint1 = settings.mPoint2 = JPH::RVec3(worldAnchor);
		settings.mHingeAxis1 = settings.mHingeAxis2 = worldAxis;
		settings.mNormalAxis1 = settings.mNormalAxis2 = worldNormal;

		/// [EN] Limits are authored in degrees; Jolt takes radians.
		/// [JP] 制限は度で指定し、Jolt にはラジアンで渡す。
		if (desc.useLimits_)
		{
			settings.mLimitsMin = ToRadians(desc.minAngle_);
			settings.mLimitsMax = ToRadians(desc.maxAngle_);
		}

		return CreateConstraint(bodyA, bodyB, settings);
	}

	/**
	* [EN]
	* Creates a fixed constraint between two bodies.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 2つのボディ間に固定拘束を生成する。
	*/
	Handle<JPH::Constraint> Physics::CreateFixedJoint(JPH::BodyID bodyA, JPH::BodyID bodyB, const FixedJointDesc&)
	{
		/// [EN] Locks the bodies in their current relative pose; the pivot is detected from their positions.
		/// [JP] 両ボディを現在の相対姿勢のまま固定する。支点は両者の位置から自動で求める。
		JPH::FixedConstraintSettings settings;
		settings.mSpace = JPH::EConstraintSpace::WorldSpace;
		settings.mAutoDetectPoint = true;

		return CreateConstraint(bodyA, bodyB, settings);
	}

	/**
	* [EN]
	* Creates a spring distance constraint between two bodies.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 2つのボディ間にばね距離拘束を生成する。
	*/
	Handle<JPH::Constraint> Physics::CreateSpringJoint(JPH::BodyID bodyA, JPH::BodyID bodyB, const SpringJointDesc& desc)
	{
		JPH::BodyInterface& bodyInterface = joltManager_.BodyInterface();
		JPH::RVec3 positionA = bodyInterface.GetPosition(bodyA);
		JPH::Quat rotationA = bodyInterface.GetRotation(bodyA);

		/// [EN] bodyA's end of the spring: the anchor, given in bodyA's local space, moved to world space.
		/// [JP] bodyA 側のばねの端: bodyA のローカル空間で与えたアンカーをワールド空間へ移したもの。
		JPH::Vec3 worldPositionA(positionA.GetX(), positionA.GetY(), positionA.GetZ());
		JPH::Vec3 anchorSelf = worldPositionA + rotationA * JPH::Vec3(desc.anchor_.x, desc.anchor_.y, desc.anchor_.z);

		/// [EN] The other end is bodyB's origin, or the anchor itself when attached to the world.
		/// [JP] もう一方の端は bodyB の原点。ワールドに繋ぐ場合はアンカーそのもの。
		JPH::Vec3 anchorOther = anchorSelf;
		if (!bodyB.IsInvalid())
		{
			JPH::RVec3 positionB = bodyInterface.GetPosition(bodyB);
			anchorOther = JPH::Vec3(positionB.GetX(), positionB.GetY(), positionB.GetZ());
		}

		/// [EN] Point1 belongs to the other body and Point2 to bodyA, matching CreateConstraint's body order.
		/// [JP] Point1 は相手側、Point2 は bodyA 側。CreateConstraint のボディ順に合わせている。
		JPH::DistanceConstraintSettings settings;
		settings.mSpace = JPH::EConstraintSpace::WorldSpace;
		settings.mPoint1 = JPH::RVec3(anchorOther);
		settings.mPoint2 = JPH::RVec3(anchorSelf);

		/// [EN] Both distances 0 means "unset": a negative value makes Jolt use the current distance.
		/// [JP] 両方 0 は未指定の扱い。負の値を渡すと、Jolt は現在の距離を使う。
		if (desc.minDistance_ == 0.0f && desc.maxDistance_ == 0.0f)
		{
			settings.mMinDistance = -1.0f;
			settings.mMaxDistance = -1.0f;
		}
		else
		{
			settings.mMinDistance = desc.minDistance_;
			settings.mMaxDistance = desc.maxDistance_;
		}

		/// [EN] The distance limits are soft, pulling back like a spring with this frequency and damping.
		/// [JP] 距離の制限は柔らかく、この振動数と減衰のばねとして引き戻す。
		settings.mLimitsSpringSettings = JPH::SpringSettings(JPH::ESpringMode::FrequencyAndDamping, desc.frequency_, desc.damping_);

		return CreateConstraint(bodyA, bodyB, settings);
	}

	/**
	* [EN]
	* Creates a slider constraint between two bodies.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 2つのボディ間にスライダー拘束を生成する。
	*/
	Handle<JPH::Constraint> Physics::CreateSliderJoint(JPH::BodyID bodyA, JPH::BodyID bodyB, const SliderJointDesc& desc)
	{
		JPH::BodyInterface& bodyInterface = joltManager_.BodyInterface();
		/// [EN] The axis is given in bodyA's local space; a zero-length axis falls back to world X.
		/// [JP] 軸は bodyA のローカル空間で与える。長さ 0 の軸はワールドの X 方向で代用する。
		JPH::Quat rotationA = bodyInterface.GetRotation(bodyA);
		JPH::Vec3 worldAxis = (rotationA * JPH::Vec3(desc.axis_.x, desc.axis_.y, desc.axis_.z)).NormalizedOr(JPH::Vec3::sAxisX());

		/// [EN] The bodies may only translate along the axis; the current offset is position 0.
		/// [JP] ボディは軸方向にだけ移動できる。現在のずれが位置 0 になる。
		JPH::SliderConstraintSettings settings;
		settings.mSpace = JPH::EConstraintSpace::WorldSpace;
		settings.mAutoDetectPoint = true;
		settings.SetSliderAxis(worldAxis);

		/// [EN] Limits on the translation along the axis, in meters.
		/// [JP] 軸方向の移動量の制限(メートル)。
		if (desc.useLimits_)
		{
			settings.mLimitsMin = desc.minDistance_;
			settings.mLimitsMax = desc.maxDistance_;
		}

		return CreateConstraint(bodyA, bodyB, settings);
	}

	/**
	* [EN]
	* Releases a joint constraint.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* ジョイント拘束を解放する。
	*/
	void Physics::DestroyJoint(Handle<JPH::Constraint> handle)
	{
		/// [EN] The pool removes the constraint from the simulation and frees its slot; stale handles are ignored.
		/// [JP] プールが拘束をシミュレーションから外してスロットを空ける。古いハンドルは無視される。
		joltManager_.ConstraintPool().Release(joltManager_.PhysicsSystem(), handle);
	}

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
	void Physics::RefleshJoint()
	{
		/// [EN] A world-attached end always counts as present.
		/// [JP] ワールドに繋がった端は、常に存在するものとして扱う。
		joltManager_.ConstraintPool().Refresh();
	}

	/**
	* [EN]
	* Creates and registers a two-body constraint.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 2ボディ拘束を生成して登録する。
	*/
	Handle<JPH::Constraint> Physics::CreateConstraint(JPH::BodyID bodyA, JPH::BodyID bodyB, const JPH::TwoBodyConstraintSettings& settings)
	{
		/// [EN] bodyA, the joint's owner, is required; bodyB is optional.
		/// [JP] ジョイントの持ち主である bodyA は必須、bodyB は省略できる。
		if (bodyA.IsInvalid())
		{
			return Handle<JPH::Constraint>::null();
		}

		JPH::PhysicsSystem& physicsSystem = joltManager_.PhysicsSystem();
		const JPH::BodyLockInterface& lockInterface = physicsSystem.GetBodyLockInterface();

		JPH::Constraint* constraint = nullptr;

		/// [EN] Without bodyB the joint attaches bodyA to the world, which takes the body1 slot.
		/// [JP] bodyB が無ければ、bodyA をワールドへ繋ぐ。ワールドが body1 の位置に入る。
		if (bodyB.IsInvalid())
		{
			JPH::BodyLockWrite lock(lockInterface, bodyA);
			if (!lock.Succeeded())
			{
				return Handle<JPH::Constraint>::null();
			}
			constraint = settings.Create(JPH::Body::sFixedToWorld, lock.GetBody());
		}
		else
		{
			/// [EN] bodyB is body1 and bodyA is body2, the same order in both cases.
			/// [JP] bodyB を body1、bodyA を body2 とし、どちらの場合も順番を揃える。
			JPH::BodyID bodyIDs[2] = { bodyB, bodyA };
			JPH::BodyLockMultiWrite lock(lockInterface, bodyIDs, 2);
			JPH::Body* body1 = lock.GetBody(0);
			JPH::Body* body2 = lock.GetBody(1);
			if (body1 == nullptr || body2 == nullptr)
			{
				return Handle<JPH::Constraint>::null();
			}
			constraint = settings.Create(*body1, *body2);
		}

		if (constraint == nullptr)
		{
			return Handle<JPH::Constraint>::null();
		}

		/// [EN] The pool adds the constraint to the simulation and hands back a generation-checked handle.
		/// [JP] プールが拘束をシミュレーションへ加え、世代付きのハンドルを返す。
		return joltManager_.ConstraintPool().Add(physicsSystem, constraint);
	}

	/**
	* [EN]
	* Finds the nearest 3D body intersected by a ray.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* レイと交差する最も近い3Dボディを検索する。
	*/
	Bool Physics::Raycast(const Vector3& origin, const Vector3& direction, Float maxDistance, RaycastHit& outHit, Uint32 layerMask)const
	{
		/// [EN] Only the direction matters; a zero vector is left as is and hits nothing.
		/// [JP] 使うのは向きだけ。ゼロベクトルはそのままにし、何にも当たらない。
		JPH::Vec3 dir(direction.x, direction.y, direction.z);
		if (dir.LengthSq() > 0.0f)
		{
			dir = dir.Normalized();
		}

		/// [EN] Jolt's ray is an origin plus a vector whose length is the reach.
		/// [JP] Jolt のレイは、始点と、長さが届く距離になるベクトルで表す。
		JPH::RRayCast ray(JPH::RVec3(origin.x, origin.y, origin.z), dir * maxDistance);

		JPH::PhysicsSystem& physicsSystem = joltManager_.PhysicsSystem();

		/// [EN] Collects every hit, since the nearest one may be filtered out below.
		/// [JP] 最も近いヒットが下で除外されることもあるので、全てのヒットを集める。
		JPH::AllHitCollisionCollector<JPH::CastRayCollector> collector;
		physicsSystem.GetNarrowPhaseQuery().CastRay(ray, JPH::RayCastSettings(), collector);

		if (!collector.HadHit())
		{
			return false;
		}

		/// [EN] Nearest first, so the first hit that passes the filters is the answer.
		/// [JP] 近い順に並べ、フィルタを通った最初のヒットを答えにする。
		collector.Sort();

		for (const JPH::RayCastResult& hit : collector.mHits)
		{
			/// [EN] 2D canvas bodies share the Jolt world but are not part of 3D queries.
			/// [JP] 2D Canvas のボディは Jolt ワールドを共有するが、3D のクエリには含めない。
			if (physicsSystem.GetBodyInterface().GetObjectLayer(hit.mBodyID) & Layers::PLANAR)
			{
				continue;
			}

			/// [EN] Skips bodies whose actor's layer bit is not in layerMask; all bits set means no filter.
			/// [JP] Actor のレイヤーのビットが layerMask に無いボディは飛ばす。全ビットが立っていればフィルタ無し。
			if (layerMask != 0xFFFFFFFF)
			{
				World* world = joltManager_.ActiveWorld();
				if (world)
				{
					Actor actor = world->GetActor(BodyEntityID(hit.mBodyID));
					if (actor && (layerMask & (1u << actor.Layer())) == 0)
					{
						continue;
					}
				}
			}

			/// [EN] The hit point lies at the hit fraction along the ray.
			/// [JP] ヒット位置は、レイ上のヒット割合の位置にある。
			JPH::RVec3 hitPosition = ray.GetPointOnRay(hit.mFraction);

			/// [EN] The surface normal needs the body's shape, so the body is read-locked briefly.
			/// [JP] 面の法線はボディの形状から求めるので、短い間だけ読み込みロックをかける。
			JPH::Vec3 normal = JPH::Vec3::sZero();
			{
				JPH::BodyLockRead lock(physicsSystem.GetBodyLockInterface(), hit.mBodyID);
				if (lock.Succeeded())
				{
					normal = lock.GetBody().GetWorldSpaceSurfaceNormal(hit.mSubShapeID2, hitPosition);
				}
			}

			/// [EN] The fraction is a 0-1 ratio of the reach, so it scales back to a distance.
			/// [JP] 割合は届く距離に対する 0～1 の比なので、距離へ戻す。
			outHit.position_ = Vector3(static_cast<Float>(hitPosition.GetX()), static_cast<Float>(hitPosition.GetY()), static_cast<Float>(hitPosition.GetZ()));
			outHit.normal_ = Vector3(normal.GetX(), normal.GetY(), normal.GetZ());
			outHit.distance_ = hit.mFraction * maxDistance;
			outHit.entityID_ = BodyEntityID(hit.mBodyID);
			return true;
		}

		return false;
	}

	/**
	* [EN]
	* Finds the nearest 3D body intersected by a swept sphere.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 移動する球と交差する最も近い3Dボディを検索する。
	*/
	Bool Physics::Spherecast(const Vector3& origin, Float radius, const Vector3& direction, Float maxDistance, RaycastHit& outHit, Uint32 layerMask)const
	{
		/// [EN] Only the direction matters; a zero vector is left as is and sweeps nowhere.
		/// [JP] 使うのは向きだけ。ゼロベクトルはそのままにし、どこへも掃引しない。
		JPH::Vec3 dir(direction.x, direction.y, direction.z);
		if (dir.LengthSq() > 0.0f)
		{
			dir = dir.Normalized();
		}

		/// [EN] A temporary sphere at origin, swept along dir for maxDistance at unit scale.
		/// [JP] origin に置いた一時的な球を、等倍のまま dir 方向へ maxDistance だけ掃引する。
		JPH::SphereShape sphereShape(radius);
		JPH::RShapeCast shapeCast(&sphereShape, JPH::Vec3::sReplicate(1.0f), JPH::RMat44::sTranslation(JPH::RVec3(origin.x, origin.y, origin.z)), dir * maxDistance);

		JPH::ShapeCastSettings settings;

		JPH::PhysicsSystem& physicsSystem = joltManager_.PhysicsSystem();

		/// [EN] Collects every hit, since the nearest one may be filtered out below.
		/// [JP] 最も近いヒットが下で除外されることもあるので、全てのヒットを集める。
		JPH::AllHitCollisionCollector<JPH::CastShapeCollector> collector;
		physicsSystem.GetNarrowPhaseQuery().CastShape(shapeCast, settings, JPH::RVec3::sZero(), collector);

		if (!collector.HadHit())
		{
			return false;
		}

		/// [EN] Nearest first, so the first hit that passes the filters is the answer.
		/// [JP] 近い順に並べ、フィルタを通った最初のヒットを答えにする。
		collector.Sort();

		for (const JPH::ShapeCastResult& hit : collector.mHits)
		{
			/// [EN] 2D canvas bodies share the Jolt world but are not part of 3D queries.
			/// [JP] 2D Canvas のボディは Jolt ワールドを共有するが、3D のクエリには含めない。
			if (physicsSystem.GetBodyInterface().GetObjectLayer(hit.mBodyID2) & Layers::PLANAR)
			{
				continue;
			}

			/// [EN] Skips bodies whose actor's layer bit is not in layerMask; all bits set means no filter.
			/// [JP] Actor のレイヤーのビットが layerMask に無いボディは飛ばす。全ビットが立っていればフィルタ無し。
			if (layerMask != 0xFFFFFFFF)
			{
				World* world = joltManager_.ActiveWorld();
				if (world)
				{
					Actor actor = world->GetActor(BodyEntityID(hit.mBodyID2));
					if (actor && (layerMask & (1u << actor.Layer())) == 0)
					{
						continue;
					}
				}
			}

			/// [EN] The penetration axis points from the sphere into the hit body, so its negation is the surface normal.
			/// [JP] めり込み軸は球から相手のボディへ向くので、その逆向きが面の法線になる。
			JPH::Vec3 normal = -hit.mPenetrationAxis.Normalized();

			/// [EN] The contact point is taken on the hit body's surface.
			/// [JP] 接触点は、当たったボディの表面上の点を使う。
			outHit.position_ = Vector3(hit.mContactPointOn2.GetX(), hit.mContactPointOn2.GetY(), hit.mContactPointOn2.GetZ());
			outHit.normal_ = Vector3(normal.GetX(), normal.GetY(), normal.GetZ());
			outHit.distance_ = hit.mFraction * maxDistance;
			outHit.entityID_ = BodyEntityID(hit.mBodyID2);
			return true;
		}

		return false;
	}

	/**
	* [EN]
	* Returns entities overlapping a 3D query shape.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 3Dクエリ形状と重なるエンティティを返す。
	*/
	DynamicArray<EntityID> Physics::Overlap(Handle<JPH::Shape> shape, const Vector3& position, const Quaternion& rotation, Uint32 layerMask)const
	{
		DynamicArray<EntityID> result;

		/// [EN] A handle that no longer points at a pooled shape overlaps nothing.
		/// [JP] プールの形状を指さなくなったハンドルなら、何とも重ならない。
		JPH::ShapeRefC queryShape = joltManager_.ShapePool().Get(shape);
		if (!queryShape)
		{
			return result;
		}

		/// [EN] Places the query shape at the given pose.
		/// [JP] クエリ形状を指定の姿勢に置く。
		JPH::RMat44 transform = JPH::RMat44::sRotationTranslation(JPH::Quat(rotation.x, rotation.y, rotation.z, rotation.w), JPH::RVec3(position.x, position.y, position.z));

		JPH::PhysicsSystem& physicsSystem = joltManager_.PhysicsSystem();

		/// [EN] Collects every overlapping body at unit scale; the order carries no meaning.
		/// [JP] 等倍で重なる全てのボディを集める。順番に意味は無い。
		JPH::AllHitCollisionCollector<JPH::CollideShapeCollector> collector;
		physicsSystem.GetNarrowPhaseQuery().CollideShape(queryShape.GetPtr(), JPH::Vec3::sReplicate(1.0f), transform, JPH::CollideShapeSettings(), JPH::RVec3::sZero(), collector);

		for (const JPH::CollideShapeResult& hit : collector.mHits)
		{
			/// [EN] 2D canvas bodies share the Jolt world but are not part of 3D queries.
			/// [JP] 2D Canvas のボディは Jolt ワールドを共有するが、3D のクエリには含めない。
			if (physicsSystem.GetBodyInterface().GetObjectLayer(hit.mBodyID2) & Layers::PLANAR)
			{
				continue;
			}

			/// [EN] Skips bodies whose actor's layer bit is not in layerMask; all bits set means no filter.
			/// [JP] Actor のレイヤーのビットが layerMask に無いボディは飛ばす。全ビットが立っていればフィルタ無し。
			if (layerMask != 0xFFFFFFFF)
			{
				World* world = joltManager_.ActiveWorld();
				if (world)
				{
					Actor actor = world->GetActor(BodyEntityID(hit.mBodyID2));
					if (actor && (layerMask & (1u << actor.Layer())) == 0)
					{
						continue;
					}
				}
			}

			result.push_back(BodyEntityID(hit.mBodyID2));
		}

		return result;
	}

	/**
	* [EN]
	* Finds the nearest planar body intersected by a 2D ray.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 2Dレイと交差する最も近い平面ボディを検索する。
	*/
	Bool Physics::Raycast2D(const Vector2& origin, const Vector2& direction, Float maxDistance, RaycastHit2D& outHit, Uint32 layerMask)const
	{
		/// [EN] Canvas Y points down and physics Y points up, so Y is negated; 2D bodies live on the z = 0 plane.
		/// [JP] Canvas の Y は下向き、物理の Y は上向きなので Y を反転する。2D ボディは z = 0 の平面上にある。
		JPH::Vec3 dir(direction.x, -direction.y, 0.0f);
		if (dir.LengthSq() > 0.0f)
		{
			dir = dir.Normalized();
		}

		/// [EN] The origin and reach are converted from pixels to meters.
		/// [JP] 始点と届く距離をピクセルからメートルへ変換する。
		JPH::RRayCast ray(JPH::RVec3(origin.x / pixelsPerMeter_, -origin.y / pixelsPerMeter_, 0.0f), dir * (maxDistance / pixelsPerMeter_));

		JPH::PhysicsSystem& physicsSystem = joltManager_.PhysicsSystem();

		/// [EN] Collects every hit, since the nearest one may be filtered out below.
		/// [JP] 最も近いヒットが下で除外されることもあるので、全てのヒットを集める。
		JPH::AllHitCollisionCollector<JPH::CastRayCollector> collector;
		physicsSystem.GetNarrowPhaseQuery().CastRay(ray, JPH::RayCastSettings(), collector);

		if (!collector.HadHit())
		{
			return false;
		}

		/// [EN] Nearest first, so the first hit that passes the filters is the answer.
		/// [JP] 近い順に並べ、フィルタを通った最初のヒットを答えにする。
		collector.Sort();

		for (const JPH::RayCastResult& hit : collector.mHits)
		{
			/// [EN] Only 2D canvas bodies take part in 2D queries.
			/// [JP] 2D のクエリには、2D Canvas のボディだけを含める。
			if (!(physicsSystem.GetBodyInterface().GetObjectLayer(hit.mBodyID) & Layers::PLANAR))
			{
				continue;
			}

			/// [EN] Skips bodies whose actor's layer bit is not in layerMask; all bits set means no filter.
			/// [JP] Actor のレイヤーのビットが layerMask に無いボディは飛ばす。全ビットが立っていればフィルタ無し。
			if (layerMask != 0xFFFFFFFF)
			{
				World* world = joltManager_.ActiveWorld();
				if (world)
				{
					Actor actor = world->GetActor(BodyEntityID(hit.mBodyID));
					if (actor && (layerMask & (1u << actor.Layer())) == 0)
					{
						continue;
					}
				}
			}

			/// [EN] The hit point lies at the hit fraction along the ray.
			/// [JP] ヒット位置は、レイ上のヒット割合の位置にある。
			JPH::RVec3 hitPosition = ray.GetPointOnRay(hit.mFraction);

			/// [EN] The surface normal needs the body's shape, so the body is read-locked briefly.
			/// [JP] 面の法線はボディの形状から求めるので、短い間だけ読み込みロックをかける。
			JPH::Vec3 normal = JPH::Vec3::sZero();
			{
				JPH::BodyLockRead lock(physicsSystem.GetBodyLockInterface(), hit.mBodyID);
				if (lock.Succeeded())
				{
					normal = lock.GetBody().GetWorldSpaceSurfaceNormal(hit.mSubShapeID2, hitPosition);
				}
			}

			/// [EN] Results go back to canvas pixels with Y down; the distance scales maxDistance, already in pixels.
			/// [JP] 結果は Y 下向きの Canvas ピクセルへ戻す。距離はピクセル単位の maxDistance に割合を掛ける。
			outHit.position_ = Vector2(static_cast<Float>(hitPosition.GetX()) * pixelsPerMeter_, -static_cast<Float>(hitPosition.GetY()) * pixelsPerMeter_);
			outHit.normal_ = Vector2(normal.GetX(), -normal.GetY());
			outHit.distance_ = hit.mFraction * maxDistance;
			outHit.entityID_ = BodyEntityID(hit.mBodyID);
			return true;
		}

		return false;
	}

	/**
	* [EN]
	* Finds the nearest planar body intersected by a swept circle.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 移動する円と交差する最も近い平面ボディを検索する。
	*/
	Bool Physics::Circlecast2D(const Vector2& origin, Float radius, const Vector2& direction, Float maxDistance, RaycastHit2D& outHit, Uint32 layerMask)const
	{
		/// [EN] Canvas Y points down and physics Y points up, so Y is negated; 2D bodies live on the z = 0 plane.
		/// [JP] Canvas の Y は下向き、物理の Y は上向きなので Y を反転する。2D ボディは z = 0 の平面上にある。
		JPH::Vec3 dir(direction.x, -direction.y, 0.0f);
		if (dir.LengthSq() > 0.0f)
		{
			dir = dir.Normalized();
		}

		/// [EN] On the z = 0 plane a sphere cuts out the circle; radius, origin and reach go from pixels to meters.
		/// [JP] z = 0 の平面上では球の断面が円になる。半径・始点・届く距離はピクセルからメートルへ変換する。
		JPH::SphereShape sphereShape(radius / pixelsPerMeter_);
		JPH::RShapeCast shapeCast(&sphereShape, JPH::Vec3::sReplicate(1.0f), JPH::RMat44::sTranslation(JPH::RVec3(origin.x / pixelsPerMeter_, -origin.y / pixelsPerMeter_, 0.0f)), dir * (maxDistance / pixelsPerMeter_));

		JPH::ShapeCastSettings settings;
		JPH::PhysicsSystem& physicsSystem = joltManager_.PhysicsSystem();

		/// [EN] Collects every hit, since the nearest one may be filtered out below.
		/// [JP] 最も近いヒットが下で除外されることもあるので、全てのヒットを集める。
		JPH::AllHitCollisionCollector<JPH::CastShapeCollector> collector;
		physicsSystem.GetNarrowPhaseQuery().CastShape(shapeCast, settings, JPH::RVec3::sZero(), collector);

		if (!collector.HadHit())
		{
			return false;
		}

		/// [EN] Nearest first, so the first hit that passes the filters is the answer.
		/// [JP] 近い順に並べ、フィルタを通った最初のヒットを答えにする。
		collector.Sort();

		for (const JPH::ShapeCastResult& hit : collector.mHits)
		{
			/// [EN] Only 2D canvas bodies take part in 2D queries.
			/// [JP] 2D のクエリには、2D Canvas のボディだけを含める。
			if (!(physicsSystem.GetBodyInterface().GetObjectLayer(hit.mBodyID2) & Layers::PLANAR))
			{
				continue;
			}

			/// [EN] Skips bodies whose actor's layer bit is not in layerMask; all bits set means no filter.
			/// [JP] Actor のレイヤーのビットが layerMask に無いボディは飛ばす。全ビットが立っていればフィルタ無し。
			if (layerMask != 0xFFFFFFFF)
			{
				World* world = joltManager_.ActiveWorld();
				if (world)
				{
					Actor actor = world->GetActor(BodyEntityID(hit.mBodyID2));
					if (actor && (layerMask & (1u << actor.Layer())) == 0)
					{
						continue;
					}
				}
			}

			/// [EN] The penetration axis points from the circle into the hit body, so its negation is the surface normal.
			/// [JP] めり込み軸は円から相手のボディへ向くので、その逆向きが面の法線になる。
			JPH::Vec3 normal = -hit.mPenetrationAxis.Normalized();

			/// [EN] Results go back to canvas pixels with Y down; the distance scales maxDistance, already in pixels.
			/// [JP] 結果は Y 下向きの Canvas ピクセルへ戻す。距離はピクセル単位の maxDistance に割合を掛ける。
			outHit.position_ = Vector2(hit.mContactPointOn2.GetX() * pixelsPerMeter_, -hit.mContactPointOn2.GetY() * pixelsPerMeter_);
			outHit.normal_ = Vector2(normal.GetX(), -normal.GetY());
			outHit.distance_ = hit.mFraction * maxDistance;
			outHit.entityID_ = BodyEntityID(hit.mBodyID2);
			return true;
		}

		return false;
	}

	/**
	* [EN]
	* Returns entities overlapping a 2D query shape.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 2Dクエリ形状と重なるエンティティを返す。
	*/
	DynamicArray<EntityID> Physics::Overlap2D(Handle<JPH::Shape> shape, const Vector2& position, Float rotation, Uint32 layerMask)const
	{
		DynamicArray<EntityID> result;

		/// [EN] A handle that no longer points at a pooled shape overlaps nothing.
		/// [JP] プールの形状を指さなくなったハンドルなら、何とも重ならない。
		JPH::ShapeRefC queryShape = joltManager_.ShapePool().Get(shape);
		if (!queryShape)
		{
			return result;
		}

		/// [EN] Rotation is in degrees about Z and negated with the flipped Y; position goes from pixels to meters.
		/// [JP] 回転は Z 軸まわりの度で、Y の反転に合わせて符号を反転する。位置はピクセルからメートルへ変換する。
		JPH::RMat44 transform = JPH::RMat44::sRotationTranslation(JPH::Quat::sRotation(JPH::Vec3::sAxisZ(), -ToRadians(rotation)), JPH::RVec3(position.x / pixelsPerMeter_, -position.y / pixelsPerMeter_, 0.0f));

		JPH::PhysicsSystem& physicsSystem = joltManager_.PhysicsSystem();

		/// [EN] Collects every overlapping body at unit scale; the order carries no meaning.
		/// [JP] 等倍で重なる全てのボディを集める。順番に意味は無い。
		JPH::AllHitCollisionCollector<JPH::CollideShapeCollector> collector;
		physicsSystem.GetNarrowPhaseQuery().CollideShape(queryShape.GetPtr(), JPH::Vec3::sReplicate(1.0f), transform, JPH::CollideShapeSettings(), JPH::RVec3::sZero(), collector);

		for (const JPH::CollideShapeResult& hit : collector.mHits)
		{
			/// [EN] Only 2D canvas bodies take part in 2D queries.
			/// [JP] 2D のクエリには、2D Canvas のボディだけを含める。
			if (!(physicsSystem.GetBodyInterface().GetObjectLayer(hit.mBodyID2) & Layers::PLANAR))
			{
				continue;
			}

			/// [EN] Skips bodies whose actor's layer bit is not in layerMask; all bits set means no filter.
			/// [JP] Actor のレイヤーのビットが layerMask に無いボディは飛ばす。全ビットが立っていればフィルタ無し。
			if (layerMask != 0xFFFFFFFF)
			{
				World* world = joltManager_.ActiveWorld();
				if (world)
				{
					Actor actor = world->GetActor(BodyEntityID(hit.mBodyID2));
					if (actor && (layerMask & (1u << actor.Layer())) == 0)
					{
						continue;
					}
				}
			}

			result.push_back(BodyEntityID(hit.mBodyID2));
		}

		return result;
	}

}
