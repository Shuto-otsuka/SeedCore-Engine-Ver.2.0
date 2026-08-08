#pragma once
#include <FoundationEngine/Prelude.h>
#include <PhysicsEngine/JoltPhysics/JoltShapePool.h>

namespace SeedCore
{
	class JoltManager;

	struct RigidbodyDesc
	{
		ShapeHandle shape_;
		Vector3 position_ = { 0.0f, 0.0f, 0.0f };
		Quaternion rotation_ = Quaternion::Identity;
		JPH::EMotionType motionType_ = JPH::EMotionType::Dynamic;
		JPH::ObjectLayer layer_ = 0;
		Float mass_ = 1.0f;
		Float linearDamping_ = 0.05f;
		Float angularDamping_ = 0.05f;
		Float friction_ = 0.2f;
		Float restitution_ = 0.0f;
		Float gravityFactor_ = 1.0f;
		JPH::EAllowedDOFs allowedDOFs_ = JPH::EAllowedDOFs::All;
	};

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
	};

	class Physics
	{
	public:
		Physics();
		~Physics() = default;

		ShapeHandle CreateBoxShape(const Vector3& size, const Vector3& center = { 0.0f, 0.0f, 0.0f });

		ShapeHandle CreateSphereShape(Float radius);

		ShapeHandle CreateCapsuleShape(Float height, Float radius);

		ShapeHandle CreateCylinderShape(Float height, Float radius);

		ShapeHandle CreateRectShape(const Vector2& size, const Vector2& center = { 0.0f, 0.0f });

		ShapeHandle CreateCircleShape(Float radius, const Vector2& center = { 0.0f, 0.0f });

		ShapeHandle CreateMeshShape(Uint32 assetID, const DynamicArray<Vector3>& positions, const DynamicArray<Uint32>& indices);

		ShapeHandle CreateConvexShape(Uint32 assetID, const DynamicArray<Vector3>& positions);

		void ReleaseShape(ShapeHandle handle);

		JPH::BodyID CreateRigidbody(const RigidbodyDesc& desc);

		/// [EN] Pure CPU construction (SoftBodySharedSettings + its edge/
		///      shear/bend constraints, then Optimize()) — touches no
		///      JoltManager/BodyInterface state, so it is safe to call off
		///      the main thread (Softbody dispatches this onto a JobSystem
		///      task; building it synchronously on the main thread for a
		///      render-resolution mesh is what froze the editor). Static:
		///      no Physics instance needed. Returns null if desc has no
		///      usable geometry.
		/// [JP] 純粋な CPU 構築（SoftBodySharedSettings とその辺/シア/曲げ
		///      拘束、そして Optimize()）— JoltManager/BodyInterface の
		///      状態には一切触れないため、メインスレッド外から呼んでも
		///      安全（Softbody はこれを JobSystem のタスクへ投げる。
		///      レンダー解像度のメッシュをメインスレッドで同期的に構築
		///      していたのがエディタのフリーズの原因だった）。static:
		///      Physics インスタンス不要。desc に使えるジオメトリが
		///      無ければ null を返す。
		[[nodiscard]] static JPH::Ref<JPH::SoftBodySharedSettings> BuildSoftbodySettings(const SoftbodyDesc& desc);

		/// [EN] Fast path: wraps an already-built SoftBodySharedSettings
		///      (see BuildSoftbodySettings) into SoftBodyCreationSettings
		///      and calls CreateAndAddSoftBody. Must run on the thread that
		///      owns JoltManager's BodyInterface (Softbody calls this from
		///      PhysicsSystem::ResolveSoftbodies once its background build
		///      job finishes). Returns an invalid BodyID if sharedSettings
		///      is null.
		/// [JP] 高速パス: 構築済みの SoftBodySharedSettings
		///      （BuildSoftbodySettings 参照）を SoftBodyCreationSettings へ
		///      包んで CreateAndAddSoftBody を呼ぶ。JoltManager の
		///      BodyInterface を所有するスレッドで実行すること
		///      （Softbody はバックグラウンドのビルドジョブが終わった時点で
		///      PhysicsSystem::ResolveSoftbodies から呼ぶ）。sharedSettings
		///      が null なら無効な BodyID を返す。
		JPH::BodyID CreateSoftbody(const SoftbodyDesc& desc, JPH::Ref<JPH::SoftBodySharedSettings> sharedSettings);

		void SetBodyShape(JPH::BodyID bodyID, ShapeHandle shape);

		void DestroyBody(JPH::BodyID bodyID);

		void GetBodyTransform(JPH::BodyID bodyID, Vector3& outPosition, Quaternion& outRotation)const;

		void GetSoftbodyVertexPositions(JPH::BodyID bodyID, DynamicArray<Vector3>& outPositions)const;

	private:
		JoltManager& joltPhysics_;
	};
}