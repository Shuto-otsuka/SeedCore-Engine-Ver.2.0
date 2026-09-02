#include <GraphicsEngine/System/ConstraintSystem.h>
#include <GraphicsEngine/Constraint/PositionConstraint.h>
#include <GraphicsEngine/Constraint/RotationConstraint.h>
#include <GraphicsEngine/Constraint/ParentConstraint.h>
#include <GraphicsEngine/Constraint/LookAtConstraint.h>
#include <GraphicsEngine/Constraint/AttachmentConstraint.h>
#include <GraphicsEngine/Model/Skeleton/Skeleton.h>
#include <FoundationEngine/ECS/World.h>
#include <FoundationEngine/ECS/Actor.h>
#include <FoundationEngine/ECS/Component/Position.h>
#include <FoundationEngine/ECS/Component/Rotation.h>
#include <FoundationEngine/ECS/Component/Scale.h>

namespace SeedCore
{
	void ConstraintSystem::Execute(World& world)
	{
		DynamicArray<EntityID> constrainedEntities;

		for (EntityID id : world.GetComponents<PositionConstraint>())
		{
			constrainedEntities.push_back(id);
		}
		for (EntityID id : world.GetComponents<RotationConstraint>())
		{
			constrainedEntities.push_back(id);
		}
		for (EntityID id : world.GetComponents<ParentConstraint>())
		{
			constrainedEntities.push_back(id);
		}
		for (EntityID id : world.GetComponents<AttachmentConstraint>())
		{
			constrainedEntities.push_back(id);
		}
		for (EntityID id : world.GetComponents<LookAtConstraint>())
		{
			constrainedEntities.push_back(id);
		}

		if (constrainedEntities.empty())
		{
			return;
		}

		/// [EN] A constrained actor's overridden matrix must propagate to every descendant (even unconstrained ones), so the dirty set includes each constrained actor's whole subtree, not just the constrained actors themselves.
		/// [JP] コンストレイントを持つ actor の上書き済み行列は、全ての子孫(コンストレイント無しも含む)へ伝播する必要があるため、dirty集合はコンストレイントを持つ actor 自身だけでなく、その全サブツリーを含む。
		std::unordered_set<EntityID> dirty;
		for (EntityID id : constrainedEntities)
		{
			Actor actor = world.GetActor(id);
			if (actor)
			{
				MarkDirtySubtree(actor, dirty);
			}
		}

		/// [EN] Recompute starting only from each dirty actor whose real parent is not itself dirty, reusing that parent's TransformSystem-computed world matrix as the base.
		/// [JP] dirtyな actor のうち、実際の親が dirty でないものだけを起点に再計算し、その親の TransformSystem 計算済みワールド行列をベースとして再利用する。
		for (EntityID id : dirty)
		{
			Actor actor = world.GetActor(id);
			Actor parent = actor.GetParent();
			if (!parent || !dirty.contains(parent.GetEntity().GetID()))
			{
				Matrix parentMatrix = parent ? parent.GetWorldMatrix() : Matrix::Identity;
				UpdateActor(actor, parentMatrix, world, dirty);
			}
		}
	}

	void ConstraintSystem::MarkDirtySubtree(Actor actor, std::unordered_set<EntityID>& dirty)
	{
		if (!dirty.insert(actor.GetEntity().GetID()).second)
		{
			return;
		}

		for (Actor child : actor.GetChildren())
		{
			MarkDirtySubtree(child, dirty);
		}
	}

	void ConstraintSystem::UpdateActor(Actor actor, const Matrix& parentMatrix, World& world, const std::unordered_set<EntityID>& dirty)
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
			local *= Matrix::CreateFromYawPitchRoll(
				ToRadians(rotation->y_),
				ToRadians(rotation->x_),
				ToRadians(rotation->z_)
			);
		}

		if (position)
		{
			local *= Matrix::CreateTranslation(position->x_, position->y_, position->z_);
		}

		Matrix worldMatrix = local * parentMatrix;

		Vector3 finalScale;
		Quaternion finalRotation;
		Vector3 finalTranslation;
		worldMatrix.Decompose(finalScale, finalRotation, finalTranslation);

		AttachmentConstraint* attachmentConstraint = world.GetComponent<AttachmentConstraint>(entity);
		ParentConstraint* parentConstraint = world.GetComponent<ParentConstraint>(entity);
		PositionConstraint* positionConstraint = world.GetComponent<PositionConstraint>(entity);
		RotationConstraint* rotationConstraint = world.GetComponent<RotationConstraint>(entity);
		LookAtConstraint* lookAtConstraint = world.GetComponent<LookAtConstraint>(entity);

		const Char* attachmentBoneName = attachmentConstraint ? attachmentConstraint->boneName_.c_str() : nullptr;
		if (attachmentConstraint && attachmentConstraint->enabled_ && attachmentConstraint->target_ != 0 && attachmentBoneName && *attachmentBoneName != '\0')
		{
			Actor target = world.FindActor(attachmentConstraint->target_);
			Skeleton* targetSkeleton = target ? target.GetComponent<Skeleton>() : nullptr;
			if (targetSkeleton && targetSkeleton->HasBone(attachmentConstraint->boneName_))
			{
				Matrix boneWorldMatrix = targetSkeleton->BoneWorldMatrix(attachmentConstraint->boneName_);

				Matrix offsetRotationMatrix = Matrix::CreateFromYawPitchRoll(ToRadians(attachmentConstraint->rotationOffset_.y), ToRadians(attachmentConstraint->rotationOffset_.x), ToRadians(attachmentConstraint->rotationOffset_.z));
				Matrix offsetMatrix = offsetRotationMatrix * Matrix::CreateTranslation(attachmentConstraint->positionOffset_);
				Matrix attachedMatrix = offsetMatrix * boneWorldMatrix;

				Vector3 attachedScale;
				Quaternion attachedRotation;
				Vector3 attachedTranslation;
				if (attachedMatrix.Decompose(attachedScale, attachedRotation, attachedTranslation))
				{
					finalTranslation = Vector3::Lerp(finalTranslation, attachedTranslation, attachmentConstraint->weight_);
					finalRotation = Quaternion::Slerp(finalRotation, attachedRotation, attachmentConstraint->weight_);
				}
			}
		}
		else if (parentConstraint && parentConstraint->enabled_)
		{
			Actor target = (parentConstraint->target_ != 0) ? world.FindActor(parentConstraint->target_) : Actor();
			if (target)
			{
				Matrix offsetRotationMatrix = Matrix::CreateFromYawPitchRoll(ToRadians(parentConstraint->rotationOffset_.y), ToRadians(parentConstraint->rotationOffset_.x), ToRadians(parentConstraint->rotationOffset_.z));
				Matrix offsetMatrix = offsetRotationMatrix * Matrix::CreateTranslation(parentConstraint->positionOffset_);
				Matrix constrainedMatrix = offsetMatrix * target.GetWorldMatrix();

				Vector3 constrainedScale;
				Quaternion constrainedRotation;
				Vector3 constrainedTranslation;
				if (constrainedMatrix.Decompose(constrainedScale, constrainedRotation, constrainedTranslation))
				{
					finalTranslation = Vector3::Lerp(finalTranslation, constrainedTranslation, parentConstraint->weight_);
					finalRotation = Quaternion::Slerp(finalRotation, constrainedRotation, parentConstraint->weight_);
				}
			}
		}
		else
		{
			if (positionConstraint && positionConstraint->enabled_)
			{
				Actor target = (positionConstraint->target_ != 0) ? world.FindActor(positionConstraint->target_) : Actor();
				if (target)
				{
					Matrix targetWorldMatrix = target.GetWorldMatrix();
					Vector3 targetScale;
					Quaternion targetRotation;
					Vector3 targetTranslation;
					targetWorldMatrix.Decompose(targetScale, targetRotation, targetTranslation);

					Vector3 desiredTranslation = targetTranslation + positionConstraint->offset_;
					finalTranslation = Vector3::Lerp(finalTranslation, desiredTranslation, positionConstraint->weight_);
				}
			}

			if (rotationConstraint && rotationConstraint->enabled_)
			{
				Actor target = (rotationConstraint->target_ != 0) ? world.FindActor(rotationConstraint->target_) : Actor();
				if (target)
				{
					Matrix targetWorldMatrix = target.GetWorldMatrix();
					Vector3 targetScale;
					Quaternion targetRotation;
					Vector3 targetTranslation;
					targetWorldMatrix.Decompose(targetScale, targetRotation, targetTranslation);

					Quaternion offsetRotation = Quaternion::CreateFromYawPitchRoll(ToRadians(rotationConstraint->offset_.y), ToRadians(rotationConstraint->offset_.x), ToRadians(rotationConstraint->offset_.z));
					Quaternion desiredRotation = offsetRotation * targetRotation;

					finalRotation = Quaternion::Slerp(finalRotation, desiredRotation, rotationConstraint->weight_);
				}
			}
		}

		if (lookAtConstraint && lookAtConstraint->enabled_)
		{
			Actor target = (lookAtConstraint->target_ != 0) ? world.FindActor(lookAtConstraint->target_) : Actor();
			if (target)
			{
				Matrix targetWorldMatrix = target.GetWorldMatrix();
				Vector3 targetScale;
				Quaternion targetRotation;
				Vector3 targetTranslation;
				targetWorldMatrix.Decompose(targetScale, targetRotation, targetTranslation);

				Vector3 forward = targetTranslation - finalTranslation;
				if (forward.LengthSquared() > 0.0001f)
				{
					forward.Normalize();
					Matrix lookMatrix = Matrix::CreateWorld(finalTranslation, forward, lookAtConstraint->upVector_);
					Quaternion desiredRotation = Quaternion::CreateFromRotationMatrix(lookMatrix);

					finalRotation = Quaternion::Slerp(finalRotation, desiredRotation, lookAtConstraint->weight_);
				}
			}
		}

		worldMatrix = Matrix::CreateScale(finalScale) * Matrix::CreateFromQuaternion(finalRotation) * Matrix::CreateTranslation(finalTranslation);
		actor.SetWorldMatrix(worldMatrix);

		for (Actor child : actor.GetChildren())
		{
			if (dirty.contains(child.GetEntity().GetID()))
			{
				UpdateActor(child, worldMatrix, world, dirty);
			}
		}
	}
}
