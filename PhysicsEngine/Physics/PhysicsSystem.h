#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/Interop/ColliderInstance.h>
#include <PhysicsEngine/Physics/Physics.h>

namespace SeedCore
{
	class Actor;
	class Rigidbody;
	class World;
	class ResourceCache;
	struct LoaderSystem;

	class SEEDCORE_API PhysicsSystem
	{
	public:
		static ShapeHandle FindColliderShape(Actor& actor);

		static JPH::EAllowedDOFs ToAllowedDOFs(const Rigidbody& rigidbody);

		static void ApplyActorTransform(const Actor& actor, BodyDesc& desc);

		static JPH::BodyID CreateColliderBody(Actor& actor, ShapeHandle shape);

		static void DestroyColliderBody(Actor& actor, JPH::BodyID bodyID, ShapeHandle shape);

		/// [EN] Walks every Box/Sphere/Capsule/Cylinder/Rect/CircleCollider in
		///      world and builds the GPU-facing debug-draw instance list,
		///      straight from each collider's own fields + its Actor's
		///      Position/Rotation. Independent of whether any JPH::Body
		///      exists for a given collider, so it works whether or not Play
		///      is active. Deliberately lives here (not in Editor/Graphics) —
		///      calling ComponentRegistry::Register<T>'s lambda machinery
		///      from outside PhysicsEngine's own translation units would
		///      require every Collider's OnAwake/OnDestroy to be individually
		///      dllexported; keeping the whole walk inside PhysicsEngine
		///      avoids that entirely.
		/// [JP] world 内の Box/Sphere/Capsule/Cylinder/Rect/CircleCollider を
		///      すべて走査し、各コライダー自身のフィールド + その Actor の
		///      Position/Rotation から、GPU向けのデバッグ描画インスタンス
		///      一覧を組み立てる。対象コライダーに JPH::Body が存在するかに
		///      依存しないため、Play中かどうかを問わず動作する。意図的に
		///      Editor/GraphicsEngine側ではなくここに置く —
		///      ComponentRegistry::Register<T> のラムダ機構を PhysicsEngine
		///      自身の翻訳単位の外から呼ぶと、各Colliderの
		///      OnAwake/OnDestroyを個別に dllexport する必要が出てしまう
		///      ため、走査全体を PhysicsEngine 内に留めることでそれを避ける。
		static DynamicArray<ColliderInstance> GatherColliderInstances(World& world);

		/// [EN] Walks every MeshCollider still waiting on collision geometry
		///      and, for each one whose ".collision" asset is already loaded
		///      (via ResourceCache::GetMeshCollisionResource), resolves it
		///      and builds the Jolt shape/body right there. Colliders whose
		///      asset isn't loaded yet stay pending for a later call.
		///      Lives here for the same reason GatherColliderInstances does —
		///      walking Collider components from outside PhysicsEngine's own
		///      translation units would require dllexporting each one.
		/// [JP] 衝突ジオメトリ待ちの MeshCollider をすべて走査し、".collision"
		///      アセットが既にロード済み（ResourceCache::GetMeshCollisionResource
		///      経由）のものはその場で解決して Jolt の形状/ボディを構築する。
		///      アセットが未ロードのコライダーは保留のまま次回の呼び出しへ
		///      持ち越す。ここに置く理由は GatherColliderInstances と同じ —
		///      PhysicsEngine 自身の翻訳単位の外から Collider コンポーネントを
		///      走査すると、各々を dllexport する必要が出てしまうため。
		static void ResolveMeshColliders(LoaderSystem& loader, ResourceCache& cache, World& world);

	private:
		static constexpr Float defaultColliderShapeRadius = 0.5f;
	};
}
