#include <FoundationEngine/ECS/System/TransformSystem.h>
#include <FoundationEngine/ECS/World.h>
#include <FoundationEngine/ECS/Actor.h>
#include <FoundationEngine/ECS/Component/Position.h>
#include <FoundationEngine/ECS/Component/Rotation.h>
#include <FoundationEngine/ECS/Component/Scale.h>

namespace SeedCore
{
	/**
	* [EN]
	* Recomputes world matrices for every actor in world, starting from
	* each root (parentless) actor.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* world 内の全 actor のワールド行列を、各ルート（親を持たない）
	* actor から開始して再計算する。
	*/
	void TransformSystem::Execute(World& world)
	{
		for (Actor actor : world.GetActors())
		{
			if (!actor.GetParent())
			{
				UpdateActor(actor, Matrix::Identity, world);
			}
		}
	}

	/**
	* [EN]
	* Computes actor's local transform (from its Position/Rotation/Scale
	* components, if present) combined with parentMatrix, stores it as
	* actor's world matrix, then recurses into actor's children.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* actor のローカルトランスフォーム（存在すれば Position/Rotation/
	* Scale コンポーネントから）を parentMatrix と組み合わせて計算し、
	* actor のワールド行列として保存した後、actor の子へ再帰する。
	*/
	void TransformSystem::UpdateActor(Actor actor, const Matrix& parentMatrix, World& world)
	{
		Entity entity = actor.GetEntity();

		Position* position = world.GetComponent<Position>(entity);
		Rotation* rotation = world.GetComponent<Rotation>(entity);
		Scale* scale = world.GetComponent<Scale>(entity);

		Matrix local = Matrix::Identity;

		if (scale)
		{
			local *= Matrix::CreateScale(scale->x_, scale->y_, scale->z_);
		}

		if (rotation)
		{
			local *= Matrix::CreateFromYawPitchRoll(ToRadians(rotation->y_), ToRadians(rotation->x_), ToRadians(rotation->z_));
		}

		if (position)
		{
			local *= Matrix::CreateTranslation(position->x_, position->y_, position->z_);
		}

		Matrix worldMatrix = local * parentMatrix;
		actor.SetWorldMatrix(worldMatrix);

		for (Actor child : actor.GetChildren())
		{
			UpdateActor(child, worldMatrix, world);
		}
	}
}
