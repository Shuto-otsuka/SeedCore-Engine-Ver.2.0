#include <GraphicsEngine/System/AnimationSystem.h>
#include <GraphicsEngine/Model/Crister.h>
#include <GraphicsEngine/Model/ModelResource.h>
#include <GraphicsEngine/Model/Animation/Animator.h>
#include <GraphicsEngine/Model/Animation/AnimationResource.h>
#include <GraphicsEngine/Model/Skeleton/Skeleton.h>
#include <GraphicsEngine/Model/IK/FullBodyIK.h>
#include <FoundationEngine/Resource/LoaderSystem.h>
#include <FoundationEngine/ECS/Query.h>
#include <FoundationEngine/ECS/World.h>
#include <FoundationEngine/ECS/Actor.h>
#include <FoundationEngine/ECS/Component/Active.h>
#include <FoundationEngine/ECS/Component/Position.h>
#include <FoundationEngine/ECS/Component/Rotation.h>
#include <FoundationEngine/ECS/Component/Scale.h>
#include <GraphicsEngine/Model/Mesh.h>

namespace SeedCore
{
	void AnimationSystem::Execute(World& world, LoaderSystem& loaderSystem, AnimationResource& animationResource, ModelResource& modelResource)
	{
		Query<Read<Active>, Read<Mesh>> query(world);
		query.ForEach([&](EntityID entityID, const Active& active, const Mesh& mesh)
			{
				if (!active.active_ || mesh.meshID_ == 0)
				{
					return;
				}

				Handle<Crister> cristerHandle = modelResource.GetHandle(mesh.meshID_);
				if (cristerHandle.empty())
				{
					return;
				}

				Crister* crister = modelResource.Resolve(loaderSystem, cristerHandle);
				if (!crister)
				{
					return;
				}

				const DynamicArray<Skin>& skins = crister->Skins();
				if (skins.empty())
				{
					return;
				}

				Actor actor = world.GetActor(entityID);
				if (!actor)
				{
					return;
				}

				Skeleton* skeleton = actor.GetComponent<Skeleton>();
				if (!skeleton)
				{
					skeleton = actor.AddComponent<Skeleton>();
				}
				if (!skeleton)
				{
					return;
				}

				const DynamicArray<Node>& nodes = crister->Nodes();

				if (skeleton->boneNames_.size() != nodes.size())
				{
					skeleton->boneNames_.clear();
					skeleton->boneNames_.reserve(nodes.size());
					for (const Node& node : nodes)
					{
						skeleton->boneNames_.push_back(String(std::string_view(node.name_)));
					}
				}

				skeleton->meshID_ = mesh.meshID_;

				DynamicArray<Matrix> poseGlobalTransforms;
				Bool hasPose = false;

				Animator* animator = actor.GetComponent<Animator>();
				if (animator)
				{
					Int stateIndex = animator->CurrentStateIndex();
					if (stateIndex >= 0 && static_cast<Size>(stateIndex) < animator->states_.size())
					{
						const AnimationState& animatorState = animator->states_[stateIndex];
						Uint32 animationAssetId = static_cast<Uint32>(animatorState.animationID_);
						if (animationAssetId != 0)
						{
							Handle<Animation> animationHandle = animationResource.GetHandle(animationAssetId);
							Animation* animation = animationHandle.empty() ? nullptr : animationResource.Resolve(loaderSystem, animationHandle);

							if (animation)
							{
								Float duration = animation->Duration();
								Float sampleTime = animator->CurrentTime();
								if (duration > 0.0f)
								{
									sampleTime = std::fmod(sampleTime, duration);
								}

								animator->UpdateCurrentClipDuration(duration);

								std::unordered_map<Int, Vector3> translationOverrides;
								std::unordered_map<Int, Quaternion> rotationOverrides;
								std::unordered_map<Int, Vector3> scaleOverrides;
								animation->SamplePose(sampleTime, translationOverrides, rotationOverrides, scaleOverrides);

								if (animator->Blending() && static_cast<Size>(animator->PreviousStateIndex()) < animator->states_.size())
								{
									Int previousStateIndex = animator->PreviousStateIndex();
									const AnimationState& previousAnimatorState = animator->states_[previousStateIndex];
									Uint32 previousAnimationAssetId = static_cast<Uint32>(previousAnimatorState.animationID_);
									Handle<Animation> previousAnimationHandle = animationResource.GetHandle(previousAnimationAssetId);
									Animation* previousAnimation = previousAnimationAssetId != 0 && !previousAnimationHandle.empty() ? animationResource.Resolve(loaderSystem, previousAnimationHandle) : nullptr;

									if (previousAnimation)
									{
										Float previousDuration = previousAnimation->Duration();
										Float previousSampleTime = animator->PreviousTime();
										if (previousDuration > 0.0f)
										{
											previousSampleTime = std::fmod(previousSampleTime, previousDuration);
										}

										std::unordered_map<Int, Vector3> previousTranslationOverrides;
										std::unordered_map<Int, Quaternion> previousRotationOverrides;
										std::unordered_map<Int, Vector3> previousScaleOverrides;
										previousAnimation->SamplePose(previousSampleTime, previousTranslationOverrides, previousRotationOverrides, previousScaleOverrides);

										Float alpha = animator->Alpha();

										std::unordered_map<Int, Vector3> blendedTranslationOverrides;
										for (const auto& [nodeIndex, currentValue] : translationOverrides)
										{
											auto previousIt = previousTranslationOverrides.find(nodeIndex);
											Vector3 previousValue = previousIt != previousTranslationOverrides.end() ? previousIt->second : nodes[nodeIndex].translation_;
											blendedTranslationOverrides[nodeIndex] = Vector3::Lerp(previousValue, currentValue, alpha);
										}
										for (const auto& [nodeIndex, previousValue] : previousTranslationOverrides)
										{
											if (!translationOverrides.contains(nodeIndex))
											{
												blendedTranslationOverrides[nodeIndex] = Vector3::Lerp(previousValue, nodes[nodeIndex].translation_, alpha);
											}
										}
										translationOverrides = std::move(blendedTranslationOverrides);

										std::unordered_map<Int, Quaternion> blendedRotationOverrides;
										for (const auto& [nodeIndex, currentValue] : rotationOverrides)
										{
											auto previousIt = previousRotationOverrides.find(nodeIndex);
											Quaternion previousValue = previousIt != previousRotationOverrides.end() ? previousIt->second : nodes[nodeIndex].rotation_;
											blendedRotationOverrides[nodeIndex] = Quaternion::Slerp(previousValue, currentValue, alpha);
										}
										for (const auto& [nodeIndex, previousValue] : previousRotationOverrides)
										{
											if (!rotationOverrides.contains(nodeIndex))
											{
												blendedRotationOverrides[nodeIndex] = Quaternion::Slerp(previousValue, nodes[nodeIndex].rotation_, alpha);
											}
										}
										rotationOverrides = std::move(blendedRotationOverrides);

										std::unordered_map<Int, Vector3> blendedScaleOverrides;
										for (const auto& [nodeIndex, currentValue] : scaleOverrides)
										{
											auto previousIt = previousScaleOverrides.find(nodeIndex);
											Vector3 previousValue = previousIt != previousScaleOverrides.end() ? previousIt->second : nodes[nodeIndex].scale_;
											blendedScaleOverrides[nodeIndex] = Vector3::Lerp(previousValue, currentValue, alpha);
										}
										for (const auto& [nodeIndex, previousValue] : previousScaleOverrides)
										{
											if (!scaleOverrides.contains(nodeIndex))
											{
												blendedScaleOverrides[nodeIndex] = Vector3::Lerp(previousValue, nodes[nodeIndex].scale_, alpha);
											}
										}
										scaleOverrides = std::move(blendedScaleOverrides);
									}
								}

								if (!skins[0].joints_.empty())
								{
									Int rootNodeIndex = skins[0].joints_[0];
									if (!animation->HasTranslationChannel(rootNodeIndex))
									{
										Int discovered = animation->FindRootMotionNode();
										if (discovered >= 0 && static_cast<Size>(discovered) < nodes.size())
										{
											rootNodeIndex = discovered;
										}
									}

									auto rootTranslationIt = translationOverrides.find(rootNodeIndex);
									Vector3 currentRootTranslation = (rootTranslationIt != translationOverrides.end()) ? rootTranslationIt->second : nodes[rootNodeIndex].translation_;

									if (animatorState.useRootMotion_)
									{
										if (animator->HasRootMotionBaseline(stateIndex))
										{
											Float previousSampleTime = animator->RootMotionBaselineSampleTime();
											Vector3 previousTranslation = animator->RootMotionBaselineTranslation();

											Vector3 delta;
											if (duration > 0.0f && sampleTime < previousSampleTime)
											{
												std::unordered_map<Int, Vector3> endTranslations, startTranslations;
												std::unordered_map<Int, Quaternion> unusedRotations;
												std::unordered_map<Int, Vector3> unusedScales;
												animation->SamplePose(duration, endTranslations, unusedRotations, unusedScales);
												animation->SamplePose(0.0f, startTranslations, unusedRotations, unusedScales);

												Vector3 endTranslation = endTranslations.count(rootNodeIndex) ? endTranslations[rootNodeIndex] : nodes[rootNodeIndex].translation_;
												Vector3 startTranslation = startTranslations.count(rootNodeIndex) ? startTranslations[rootNodeIndex] : nodes[rootNodeIndex].translation_;

												delta = (endTranslation - previousTranslation) + (currentRootTranslation - startTranslation);
											}
											else
											{
												delta = currentRootTranslation - previousTranslation;
											}

											if (delta.x != 0.0f || delta.y != 0.0f || delta.z != 0.0f)
											{
												Entity entity = actor.GetEntity();
												Rotation* rotationComponent = world.GetComponent<Rotation>(entity);
												Scale* scaleComponent = world.GetComponent<Scale>(entity);

												Matrix scaleMatrix = scaleComponent ? Matrix::CreateScale(scaleComponent->x_, scaleComponent->y_, scaleComponent->z_) : Matrix::Identity;
												Matrix rotationMatrix = rotationComponent ? Matrix::CreateFromYawPitchRoll(ToRadians(rotationComponent->y_), ToRadians(rotationComponent->x_), ToRadians(rotationComponent->z_)) : Matrix::Identity;

												Vector3 localDelta = Vector3::TransformNormal(delta, scaleMatrix * rotationMatrix);

												Position* positionComponent = world.GetComponent<Position>(entity);
												if (positionComponent)
												{
													positionComponent->x_ += localDelta.x;
													positionComponent->y_ += localDelta.y;
													positionComponent->z_ += localDelta.z;
												}
											}
										}

										animator->UpdateRootMotionBaseline(stateIndex, sampleTime, currentRootTranslation);

										translationOverrides[rootNodeIndex] = Vector3::Zero;
									}
									else
									{
										translationOverrides[rootNodeIndex] = Vector3::Zero;
									}
								}

								poseGlobalTransforms.resize(nodes.size(), Matrix::Identity);

								std::function<void(Int, const Matrix&)> traverse = [&](Int nodeIndex, const Matrix& parentGlobal)
									{
										const Node& node = nodes[nodeIndex];

										auto translationIt = translationOverrides.find(nodeIndex);
										Vector3 translation = (translationIt != translationOverrides.end()) ? translationIt->second : node.translation_;

										auto rotationIt = rotationOverrides.find(nodeIndex);
										Quaternion rotation = (rotationIt != rotationOverrides.end()) ? rotationIt->second : node.rotation_;

										auto scaleIt = scaleOverrides.find(nodeIndex);
										Vector3 scale = (scaleIt != scaleOverrides.end()) ? scaleIt->second : node.scale_;

										Matrix scaleMatrix = Matrix::CreateScale(scale.x, scale.y, scale.z);
										Matrix rotationMatrix = Matrix::CreateFromQuaternion(rotation);
										Matrix translationMatrix = Matrix::CreateTranslation(translation.x, translation.y, translation.z);
										Matrix localTransform = scaleMatrix * rotationMatrix * translationMatrix;

										Matrix global = localTransform * parentGlobal;
										poseGlobalTransforms[nodeIndex] = global;

										for (Int childIndex : node.children_)
										{
											traverse(childIndex, global);
										}
									};

								for (Int rootNodeIndex : crister->Stages()[crister->DefaultStage()].nodes_)
								{
									traverse(rootNodeIndex, Matrix::Identity);
								}

								FullBodyIK::Apply(*crister, *animator, poseGlobalTransforms);

								hasPose = true;
							}
						}
					}
				}

				if (hasPose)
				{
					skeleton->globalTransforms_ = std::move(poseGlobalTransforms);
					skeleton->animated_ = true;
				}
				else
				{
					skeleton->globalTransforms_.clear();
					skeleton->globalTransforms_.reserve(nodes.size());
					for (const Node& node : nodes)
					{
						skeleton->globalTransforms_.push_back(node.globalTransform_);
					}
					skeleton->animated_ = false;
				}

				skeleton->valid_ = true;
			});
	}
}
