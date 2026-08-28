#include <Editor/Editor/ViewportPicking.h>
#include <FoundationEngine/ECS/World.h>
#include <FoundationEngine/ECS/Actor.h>
#include <FoundationEngine/ECS/ComponentRegistry.h>
#include <FoundationEngine/ECS/Component/Bounds.h>

namespace SeedCore
{
	/**
	* [EN]
	* Finds the actor whose Bounds the ray hits closest to rayOrigin.
	* Transforms the ray into each candidate's local space via its inverse
	* world matrix so the local-space Bounds acts as an OBB, then converts
	* the local hit back to world space to rank candidates by true
	* world-space distance (raw local ray parameters are not comparable
	* across actors with different scales).
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* レイが rayOrigin に最も近い位置で当たる Bounds を持つアクターを
	* 探す。各候補の逆ワールド行列でレイをローカル空間へ変換し、ローカル
	* 空間の Bounds を OBB として機能させ、ローカルのヒット点をワールド
	* 空間へ変換し直してから真のワールド距離で候補を順位付けする(生の
	* ローカルレイパラメータはスケールの異なるアクター間では比較できない
	* ため)。
	*/
	Actor* ViewportPicking::Pick(World& world, const Vector3& rayOrigin, const Vector3& rayDirection)
	{
		ComponentID boundsComponentID = ComponentRegistry::GetComponentID<Bounds>();
		if (!boundsComponentID)
		{
			return nullptr;
		}

		Actor* closestActor = nullptr;
		Float closestDistance = FLT_MAX;

		for (EntityID entityID : world.GetComponents<Bounds>())
		{
			Actor* actor = world.GetActor(entityID);
			if (!actor)
			{
				continue;
			}

			const Bounds* bounds = actor->GetComponent<Bounds>();
			if (!bounds)
			{
				continue;
			}

			Matrix worldMatrix = actor->GetWorldMatrix();
			Matrix inverseWorld = worldMatrix.Invert();

			Vector3 localOrigin = Vector3::Transform(rayOrigin, inverseWorld);
			Vector3 localDirection = Vector3::TransformNormal(rayDirection, inverseWorld);

			Vector3 boundsMin = bounds->center_ - bounds->extent_;
			Vector3 boundsMax = bounds->center_ + bounds->extent_;

			Float localT;
			if (!RayIntersectsLocalAABB(localOrigin, localDirection, boundsMin, boundsMax, localT))
			{
				continue;
			}

			Vector3 localHitPoint = localOrigin + localDirection * localT;
			Vector3 worldHitPoint = Vector3::Transform(localHitPoint, worldMatrix);
			Float worldDistance = Vector3::Distance(rayOrigin, worldHitPoint);

			if (worldDistance < closestDistance)
			{
				closestDistance = worldDistance;
				closestActor = actor;
			}
		}

		return closestActor;
	}

	/**
	* [EN]
	* Slab-method ray/AABB test in the box's local space, doubling as an
	* OBB test. outT is the entry point when the ray starts outside the
	* box, or the exit point when it starts inside.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 箱のローカル空間でのスラブ法レイ/AABB 判定。OBB 判定としても機能
	* する。outT はレイが箱の外から始まれば侵入点、内側から始まれば
	* 脱出点。
	*/
	Bool ViewportPicking::RayIntersectsLocalAABB(const Vector3& rayOriginLocal, const Vector3& rayDirectionLocal, const Vector3& boundsMin, const Vector3& boundsMax, Float& outT)
	{
		Float origin[3] = { rayOriginLocal.x, rayOriginLocal.y, rayOriginLocal.z };
		Float direction[3] = { rayDirectionLocal.x, rayDirectionLocal.y, rayDirectionLocal.z };
		Float boundsMinArray[3] = { boundsMin.x, boundsMin.y, boundsMin.z };
		Float boundsMaxArray[3] = { boundsMax.x, boundsMax.y, boundsMax.z };

		Float tMin = -FLT_MAX;
		Float tMax = FLT_MAX;

		for (Int axis = 0; axis < 3; axis++)
		{
			if (std::abs(direction[axis]) < 1e-8f)
			{
				if (origin[axis] < boundsMinArray[axis] || origin[axis] > boundsMaxArray[axis])
				{
					return false;
				}
				continue;
			}

			Float inverseDirection = 1.0f / direction[axis];
			Float t0 = (boundsMinArray[axis] - origin[axis]) * inverseDirection;
			Float t1 = (boundsMaxArray[axis] - origin[axis]) * inverseDirection;
			if (t0 > t1)
			{
				std::swap(t0, t1);
			}

			tMin = Max(tMin, t0);
			tMax = Min(tMax, t1);
			if (tMin > tMax)
			{
				return false;
			}
		}

		if (tMax < 0.0f)
		{
			/// [EN] Whole box is behind the ray origin.
			/// [JP] 箱全体がレイの起点より後ろにある。
			return false;
		}

		outT = (tMin >= 0.0f) ? tMin : tMax;
		return true;
	}
}
