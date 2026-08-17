#include <GraphicsEngine/Renderer/ModelRenderer.h>
#include <GraphicsEngine/Profiler/ProfilerStats.h>
#include <GraphicsEngine/Model/Crister.h>
#include <GraphicsEngine/Model/ModelResource.h>
#include <GraphicsEngine/D3D12/Descriptor/BindlessHeap.h>
#include <GraphicsEngine/D3D12/PipelineState/PipelineStateObject.h>
#include <GraphicsEngine/D3D12/Context/D3D12CommandList.h>
#include <GraphicsEngine/D3D12/Buffer/FrameBuffer.h>
#include <GraphicsEngine/D3D12/Buffer/GeometryBuffer.h>
#include <GraphicsEngine/System/IndicesSystem.h>
#include <FoundationEngine/ECS/Query.h>
#include <FoundationEngine/ECS/Component/Active.h>
#include <FoundationEngine/ECS/Actor.h>
#include <FoundationEngine/ECS/World.h>
#include <GraphicsEngine/Model/Mesh.h>
#include <FoundationEngine/ECS/Component/Bounds.h>
#include <GraphicsEngine/Model/Animation/Animator.h>
#include <GraphicsEngine/Model/Animation/AnimationResource.h>
#include <GraphicsEngine/Model/IK/FullBodyIK.h>
#include <PhysicsEngine/Softbody/Softbody.h>
#include <FoundationEngine/ECS/Component/Position.h>
#include <FoundationEngine/ECS/Component/Rotation.h>
#include <FoundationEngine/ECS/Component/Scale.h>
#include <FoundationEngine/ECS/ComponentRegistry.h>
#include <cfloat>

namespace SeedCore
{
	ModelRenderer::ModelRenderer(RootSignature& rootSignature, PipelineStateObject& pipelineStateObject) : modelShader_(rootSignature, pipelineStateObject)
	{
		/// No Code
	}

	void ModelRenderer::Create(ID3D12Device* device, BindlessHeap* bindlessHeap, ShaderCache& shaderCache, IndicesSystem& indicesSystem, Uint32 width, Uint32 height)
	{
		device_ = device;
		bindlessHeap_ = bindlessHeap;
		indicesSystem_ = &indicesSystem;
		maxInstanceCount_ = 65536;
		maxBoneCount_ = 65536;
		maxMorphWeightCount_ = 65536;

		modelShader_.Create(shaderCache, device);

		/// [EN] Frame-ring buffers: the current SRV index changes every frame,
		///      so Upload re-registers them into the IndicesSystem each frame.
		/// [JP] フレームリングバッファ: 現在の SRV インデックスは毎フレーム
		///      変わるため、Upload が毎フレーム IndicesSystem に再登録する。
		instanceBuffer_ = MakePtr<ReadOnlyStructuredBuffer<ModelInstanceData>>(device, bindlessHeap, maxInstanceCount_);
		indicesSystem.SetModelInstanceIndex(instanceBuffer_->Index());

		boneBuffer_ = MakePtr<ReadOnlyStructuredBuffer<Matrix>>(device, bindlessHeap, maxBoneCount_);
		indicesSystem.SetModelBoneMatrixIndex(boneBuffer_->Index());

		morphWeightBuffer_ = MakePtr<ReadOnlyStructuredBuffer<Float>>(device, bindlessHeap, maxMorphWeightCount_);
		indicesSystem.SetModelMorphWeightIndex(morphWeightBuffer_->Index());

		oitBuffer_.Create(device, bindlessHeap, indicesSystem, width, height);
	}

	void ModelRenderer::Resize(ID3D12Device* device, BindlessHeap* bindlessHeap, IndicesSystem& indicesSystem, Uint32 width, Uint32 height)
	{
		oitBuffer_.Resize(device, bindlessHeap, indicesSystem, width, height);
	}

	void ModelRenderer::Gather(LoaderSystem& loaderSystem, ModelResource& modelResource, AnimationResource& animationResource, World& world, const SceneConstantBuffer& scene, Entity selectedEntity)
	{
		streamingFrame_++;
		geometryStreamingRequests_.clear();

		opaqueInstances_.clear();
		transparentInstances_.clear();
		boneMatrices_.clear();
		animatedBoneOffsets_.clear();
		animatedMorphWeights_.clear();
		morphWeights_.clear();
		hasSkinnedOpaque_ = false;
		hasSkinnedTransparent_ = false;
		hasSelectedInstance_ = false;
		hasSelectedSkinned_ = false;
		uploaded_ = false;

		/// [EN] Bone palette base offset per Crister — palettes are bind-pose
		///      (no animation system yet), so instances sharing a Crister can
		///      share one palette within this frame.
		/// [JP] Crister ごとのボーンパレット先頭オフセット。パレットはバインドポーズ
		///      （アニメーションシステム未実装）なので、同じ Crister を共有する
		///      インスタンスはこのフレーム内で 1 つのパレットを共有できる。
		std::unordered_map<const Crister*, Uint> cristerBoneBase;

		/// [EN] Cached once per Gather() rather than looked up per instance —
		///      see the Bounds sync below (right after resolving each
		///      Mesh actor's Crister).
		/// [JP] Gather() ごとに一度だけキャッシュする — インスタンスごとに
		///      引かない（下の Bounds 同期処理参照。各 Mesh アクターの
		///      Crister 解決直後で使う）。
		ComponentID boundsComponentID = ComponentRegistry::GetComponentID<Bounds>();

		Query<Read<Active>, Read<Mesh>> query(world);
		query.ForEach([&](EntityID entityID, const Active& active, const Mesh& mesh)
			{
				if (!active.active_)
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

				/// [EN] Use the world matrix TransformSystem already computed
				///      (local * parent chain), not a re-derived local-only
				///      matrix - otherwise child actors ignore their parent.
				/// [JP] TransformSystem が既に計算したワールド行列(親子チェーン込み)
				///      を使う。ローカルだけで再構築すると子が親を無視してしまう。
				Actor* actor = world.GetActor(entityID);
				if (!actor)
				{
					return;
				}

				/// [EN] Overwrite this actor's Bounds (every actor already
				///      has one — see Bounds's class comment, added in
				///      Actor::Actor) with the resolved Crister's
				///      local-space AABB.
				/// [JP] このアクターの Bounds（全 actor が構築時から既に
				///      持っている — Bounds のクラスコメント、Actor::Actor
				///      参照）を、解決済み Crister のローカル空間 AABB で
				///      上書きする。
				if (boundsComponentID)
				{
					void* boundsRaw = world.GetComponent(entityID, boundsComponentID);
					if (boundsRaw)
					{
						Bounds* bounds = static_cast<Bounds*>(boundsRaw);
						Vector3 positionExtent = crister->PositionExtent();
						bounds->center_ = crister->PositionMin() + positionExtent * 0.5f;
						bounds->extent_ = positionExtent * 0.5f;
					}
				}

				/// [EN] Softbody-bearing actors with a live Jolt body are
				///      drawn entirely through the dedicated Softbody block
				///      below (their own SoftbodyMesh, not this Crister's
				///      cluster/LOD streaming path) - skip the normal path
				///      here so they don't draw twice. A Softbody with no
				///      body yet (not Playing, or not yet built) falls
				///      through to the normal path instead, so the actor
				///      renders its bind pose rather than nothing - and so
				///      stopping Play (which destroys the body) doesn't
				///      leave the last simulated frame's deformed shape
				///      stuck on screen forever (see Softbody::HasBody).
				/// [JP] 生きた Jolt ボディを持つ Softbody アクターは、下の
				///      Softbody 専用ブロックで完全に描画される（この
				///      Crister のクラスタ/LOD ストリーミング経路ではなく
				///      自身の SoftbodyMesh）— 二重描画にならないようここでは
				///      通常経路をスキップする。まだボディが無い Softbody
				///      （Play中でない、またはまだビルドされていない）は
				///      通常経路へフォールバックする — アクターが何も
				///      描かれないのではなくバインドポーズで描かれるように
				///      するため。これにより Play を止めて（ボディが破棄
				///      されて）も、最後にシミュレートしたフレームの変形
				///      形状が画面に残り続けない（Softbody::HasBody 参照）。
				Softbody* softbody = actor->GetComponent<Softbody>();
				if (softbody && softbody->HasBody())
				{
					return;
				}

				Matrix worldMatrix = actor->GetWorldMatrix();

				Matrix inverseTransposeWorld = worldMatrix.Invert().Transpose();

				/// [JP] 前フレームのワールド行列(速度計算用、StaticModelMS.hlsl/
				///      SkeletalModelMS.hlsl 参照)。今フレームの値で上書きする前に
				///      読み、この Gather() の最後で書き戻す。初出のエンティティは
				///      今フレームの worldMatrix にフォールバック(出現時は速度ゼロ)。
				auto previousWorldIt = previousWorldMatrices_.find(entityID);
				Matrix previousWorldMatrix = previousWorldIt != previousWorldMatrices_.end() ? previousWorldIt->second : worldMatrix;
				previousWorldMatrices_[entityID] = worldMatrix;

				const auto& subMeshes = crister->SubMeshes();
				const auto& materials = crister->Materials();
				const auto& clusters = crister->Clusters();
				const auto& skins = crister->Skins();
				const auto& nodes = crister->Nodes();

				/// [EN] If this Actor has an Animator currently in a valid state
				///      with an assigned animation, sample that animation's pose
				///      at the Animator's current time and re-derive every node's
				///      global transform for THIS instance only (root-down via
				///      children_, same S·R·T convention as
				///      ModelLoader::CumulateTransforms, but using the sampled
				///      local override where the animation drives that node).
				/// [JP] このActorにAnimatorが付いていて、有効なステートに
				///      アニメーションが割り当てられていれば、Animatorの現在時刻で
				///      そのアニメーションのポーズをサンプリングし、このインスタンス
				///      専用に全ノードのグローバルトランスフォームを再計算する
				///      (children_を辿ってルートから、ModelLoader::CumulateTransforms
				///      と同じS·R·T規約だが、アニメーションが駆動するノードは
				///      サンプリングしたローカル行列を使う)。
				DynamicArray<Matrix> poseGlobalTransforms;
				Bool hasPose = false;

				/// [EN] Also runs for a skin-less Actor when its Crister has morph
				///      targets — morph weight animation (SampleMorphWeights
				///      below) needs the sampled Animation regardless of
				///      whether this mesh is also skinned.
				/// [JP] Crister がモーフターゲットを持つなら、スキンの無い
				///      Actor でも実行する — 下の SampleMorphWeights による
				///      モーフウェイトアニメーションは、このメッシュがスキン
				///      済みかどうかに関わらずサンプリング済みの Animation を
				///      必要とするため。
				Bool hasMorphs = std::any_of(crister->SubMeshes().begin(), crister->SubMeshes().end(), [](const SubMesh& subMesh) { return !subMesh.morphs_.empty(); });
				Animator* animator = (skins.empty() && !hasMorphs) ? nullptr : actor->GetComponent<Animator>();
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
								/// [EN] Animator::CurrentTime() grows unbounded (it never
								///      wraps itself), so loop it against this animation's
								///      duration here, at the point of sampling.
								/// [JP] Animator::CurrentTime()は無制限に増え続ける
								///      (自分ではラップしない)ため、サンプリングする
								///      このタイミングでアニメーションの長さに対して
								///      ループさせる。
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

								if (std::any_of(crister->SubMeshes().begin(), crister->SubMeshes().end(), [](const SubMesh& subMesh) { return !subMesh.morphs_.empty(); }))
								{
									animation->SampleMorphWeights(sampleTime, animatedMorphWeights_[entityID]);
								}

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

								if (animatorState.useRootMotion_ && !skins.empty() && !skins[0].joints_.empty())
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

										delta.y = 0.0f;

										if (delta.x != 0.0f || delta.z != 0.0f)
										{
											Entity entity = actor->GetEntity();
											Rotation* rotationComponent = world.GetComponent<Rotation>(entity);
											Scale* scaleComponent = world.GetComponent<Scale>(entity);

											Matrix scaleMatrix = scaleComponent ? Matrix::CreateScale(scaleComponent->x, scaleComponent->y, scaleComponent->z) : Matrix::Identity;
											Matrix rotationMatrix = rotationComponent ? Matrix::CreateFromYawPitchRoll(ToRadians(rotationComponent->y), ToRadians(rotationComponent->x), ToRadians(rotationComponent->z)) : Matrix::Identity;

											Vector3 localDelta = Vector3::TransformNormal(delta, scaleMatrix * rotationMatrix);

											Position* positionComponent = world.GetComponent<Position>(entity);
											if (positionComponent)
											{
												positionComponent->x += localDelta.x;
												positionComponent->y += localDelta.y;
												positionComponent->z += localDelta.z;
											}
										}
									}

									animator->UpdateRootMotionBaseline(stateIndex, sampleTime, currentRootTranslation);

									Vector3 maskedRootTranslation = currentRootTranslation;
									maskedRootTranslation.x = 0.0f;
									maskedRootTranslation.z = 0.0f;
									translationOverrides[rootNodeIndex] = maskedRootTranslation;
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

				/// [EN] Build (or reuse) this Crister's bone palette. Palette layout:
				///      all skins appended contiguously in skin order. Each entry is
				///      inverseBindMatrix × jointGlobalTransform (row-vector order),
				///      which is identity in bind pose. Animated instances (hasPose)
				///      always get their own fresh palette, never shared/cached —
				///      each Actor can be at a different point in its animation.
				/// [JP] この Crister のボーンパレットを構築（または再利用）する。
				///      レイアウト: 全スキンをスキン順に連続で並べる。各要素は
				///      逆バインド行列 × ジョイントのグローバルトランスフォーム
				///      （行ベクトル規約の順）で、バインドポーズでは単位行列になる。
				///      アニメーション中のインスタンス(hasPose)は共有/キャッシュせず
				///      毎回専用のパレットを作る — Actorごとにアニメーションの
				///      進行度が異なりうるため。
				Uint boneBase = 0;
				Bool boneOverflow = false;
				if (!skins.empty())
				{
					if (hasPose)
					{
						Size totalJoints = 0;
						for (const Skin& skin : skins)
						{
							totalJoints += skin.joints_.size();
						}

						if (boneMatrices_.size() + totalJoints > maxBoneCount_)
						{
							boneOverflow = true;
						}
						else
						{
							boneBase = static_cast<Uint>(boneMatrices_.size());
							for (const Skin& skin : skins)
							{
								for (Size joint = 0; joint < skin.joints_.size(); joint++)
								{
									const Matrix& globalTransform = poseGlobalTransforms[skin.joints_[joint]];

									if (joint < skin.inverseBindMatrices_.size())
									{
										boneMatrices_.push_back(skin.inverseBindMatrices_[joint] * globalTransform);
									}
									else
									{
										boneMatrices_.push_back(globalTransform);
									}
								}
							}
						}
					}
					else
					{
						auto found = cristerBoneBase.find(crister);
						if (found != cristerBoneBase.end())
						{
							boneBase = found->second;
						}
						else
						{
							Size totalJoints = 0;
							for (const Skin& skin : skins)
							{
								totalJoints += skin.joints_.size();
							}

							if (boneMatrices_.size() + totalJoints > maxBoneCount_)
							{
								boneOverflow = true;
							}
							else
							{
								boneBase = static_cast<Uint>(boneMatrices_.size());
								for (const Skin& skin : skins)
								{
									for (Size joint = 0; joint < skin.joints_.size(); joint++)
									{
										const Matrix& globalTransform = nodes[skin.joints_[joint]].globalTransform_;

										/// [EN] glTF allows omitting inverseBindMatrices (implies identity).
										/// [JP] glTF は inverseBindMatrices の省略を許容する（単位行列扱い）。
										if (joint < skin.inverseBindMatrices_.size())
										{
											boneMatrices_.push_back(skin.inverseBindMatrices_[joint] * globalTransform);
										}
										else
										{
											boneMatrices_.push_back(globalTransform);
										}
									}
								}
								cristerBoneBase[crister] = boneBase;
							}
						}
					}

					if (hasPose && !boneOverflow)
					{
						animatedBoneOffsets_[entityID] = static_cast<Uint32>(boneBase);
					}
				}

				for (const auto& subMesh : subMeshes)
				{
					const Material& material = materials[subMesh.materialIndex_];

					/// [EN] Whether this SubMesh renders through the skeletal path.
					///      Skinned SubMeshes stay on LOD 0: the QEM-generated LOD
					///      vertices carry unreliable joint/weight copies.
					/// [JP] この SubMesh がスケルタルパスで描画されるかどうか。
					///      スキンド SubMesh は LOD 0 固定: QEM 生成の LOD 頂点は
					///      ジョイント/ウェイトのコピーが信頼できないため。
					Bool skinned = subMesh.skinIndex_ >= 0 && subMesh.skinIndex_ < static_cast<Int>(skins.size()) && !boneOverflow;

					if (subMesh.clusterCount_ == 0)
					{
						continue;
					}

					/// [EN] Geometry streaming + LOD selection: mirror the AS's
					///      IsLodSelected metric on the CPU to find the cluster the
					///      camera actually wants (the coarsest whose screen-space
					///      error fits 1px), request it if not yet resident, and emit
					///      ONLY that one cluster. Previously every resident cluster of
					///      every LOD was emitted and the AS discarded all but one on
					///      the GPU, so the instance buffer filled up with entries that
					///      never drew — a detailed model could push the total past the
					///      dispatch cap. The coarsest cluster is pinned at load, so
					///      there is always a resident fallback and never a hole.
					/// [JP] ジオメトリストリーミング + LOD 選択: AS の IsLodSelected と
					///      同じ式を CPU で再現してカメラが本当に必要とするクラスタ
					///      （スクリーン誤差が 1px に収まる最も粗いもの）を求め、
					///      未常駐なら要求し、その 1 つだけを発行する。以前は全 LOD の
					///      常駐クラスタを発行して AS が GPU 側で 1 つを残し他を捨てて
					///      いたため、インスタンスバッファが描画されないエントリで
					///      埋まっていた — 詳細なモデルでは合計がディスパッチ上限を
					///      超えうる。最粗クラスタはロード時にピン留めされるため、
					///      常駐のフォールバックが必ず存在し穴は開かない。
					Float worldScale = Max(Max(Vector3(worldMatrix._11, worldMatrix._12, worldMatrix._13).Length(), Vector3(worldMatrix._21, worldMatrix._22, worldMatrix._23).Length()), Vector3(worldMatrix._31, worldMatrix._32, worldMatrix._33).Length());
					Vector3 instancePosition(worldMatrix._41, worldMatrix._42, worldMatrix._43);
					Float viewDistance = Max((instancePosition - Vector3(scene.cameraPosition_.x, scene.cameraPosition_.y, scene.cameraPosition_.z)).Length(), 0.0001f);
					Float pixelsPerUnit = scene.projection_._22 * scene.screenSize_.y * 0.5f / viewDistance;

					/// [EN] Texture streaming: same worldScale/pixelsPerUnit metric
					///      as the cluster LOD selection above, applied per material
					///      texture slot (Crister::TextureDesiredMip). The request
					///      carries the desired mip and MakeTextureMipResident jumps
					///      straight to it in one upload (not one level per frame);
					///      TextureBindlessIndex always resolves to whatever is
					///      currently resident, so there is never a missing SRV.
					/// [JP] テクスチャストリーミング: 上のクラスタ LOD 選択と同じ
					///      worldScale/pixelsPerUnit 指標を、マテリアルの各テクスチャ
					///      スロットに適用する(Crister::TextureDesiredMip)。要求には
					///      目標ミップを持たせ、MakeTextureMipResident が1回の
					///      アップロードで直接そこへ到達する(1フレーム1段ずつではない)。
					///      TextureBindlessIndex は常にそのとき常駐しているものへ
					///      解決するため、SRV が欠けることはない。
					auto requestTextureMip = [&](Uint32 materialTextureIndex)
					{
						if (materialTextureIndex == 0xFFFFFFFF)
						{
							return;
						}
						Uint32 desiredMip = crister->TextureDesiredMip(materialTextureIndex, worldScale, pixelsPerUnit);
						if (crister->TextureFinestMip(materialTextureIndex) > desiredMip)
						{
							textureStreamingRequests_.push_back({ crister, materialTextureIndex, desiredMip });
						}
						crister->TouchTexture(materialTextureIndex, streamingFrame_);
					};
					requestTextureMip(material.baseColorTextureIndex_);
					requestTextureMip(material.normalTextureIndex_);
					requestTextureMip(material.metallicRoughnessTextureIndex_);
					requestTextureMip(material.emissiveTextureIndex_);

					Uint32 selectedCluster = 0xFFFFFFFF;
					if (skinned)
					{
						/// [JP] スキンドは LOD 0 固定（ロード時にピン留め済み）。
						selectedCluster = subMesh.clusterOffset_;
					}
					else
					{
						Uint32 desired = 0;
						for (Uint32 c = 0; c < subMesh.clusterCount_; ++c)
						{
							if (clusters[subMesh.clusterOffset_ + c].lodError_ * worldScale * pixelsPerUnit <= 1.0f)
							{
								desired = c;
							}
						}
						if (!crister->IsClusterResident(subMesh.clusterOffset_ + desired))
						{
							geometryStreamingRequests_.push_back({ crister, subMesh.clusterOffset_ + desired });
						}

						/// [EN] Same bracket the AS used to apply, resolved here instead:
						///      clusters are walked coarsest-error-ascending, so the last
						///      resident one still within 1px wins. If none fit (only
						///      coarse clusters are resident yet), the first resident —
						///      the finest available — is kept as the fallback.
						/// [JP] AS が行っていたのと同じ判定をここで解決する: クラスタは
						///      誤差の昇順に並ぶため、1px に収まる最後の常駐クラスタが
						///      選ばれる。どれも収まらない場合（まだ粗いクラスタしか
						///      常駐していない場合）は、最初の常駐クラスタ＝利用可能な
						///      中で最も細かいものをフォールバックとして残す。
						for (Uint32 c = 0; c < subMesh.clusterCount_; ++c)
						{
							Uint32 clusterIndex = subMesh.clusterOffset_ + c;
							if (!crister->IsClusterResident(clusterIndex))
							{
								continue;
							}
							if (selectedCluster == 0xFFFFFFFF || clusters[clusterIndex].lodError_ * worldScale * pixelsPerUnit <= 1.0f)
							{
								selectedCluster = clusterIndex;
							}
						}
					}

					if (selectedCluster == 0xFFFFFFFF)
					{
						continue;
					}

					/// [EN] Raster morph blend (see Model.hlsli's ApplyMorphBlend):
					///      only valid when the selected cluster references
					///      the shared LOD 0 pool (Crister::StandaloneVertices'
					///      comment) — an own-page (streamed-in coarser) LOD
					///      just renders its frozen bind-pose shape instead,
					///      by leaving morphTargetCount 0. Routes this
					///      SubMesh's weights the same way RaytracingRenderer
					///      does: SubMesh::meshIndex_ -> the owning Node ->
					///      animatedMorphWeights_[entityID][nodeIndex].
					/// [JP] ラスタのモーフブレンド(Model.hlsli の
					///      ApplyMorphBlend 参照): 選択クラスタが共有 LOD 0
					///      プールを参照する場合のみ有効(Crister::
					///      StandaloneVertices のコメント参照) — 自前ページ
					///      (ストリームイン済みのより粗い)LOD は
					///      morphTargetCount を 0 のままにして、代わりに
					///      凍結されたバインドポーズ形状を描画する。この
					///      SubMesh のウェイトは RaytracingRenderer と同じ
					///      経路で解決する: SubMesh::meshIndex_ → 所有 Node
					///      → animatedMorphWeights_[entityID][nodeIndex]。
					Uint32 morphDeltaBufferIndex = 0xFFFFFFFF;
					Uint32 vertexMorphSourceBufferIndex = 0xFFFFFFFF;
					Uint32 morphDeltaOffset = 0;
					Uint32 morphVertexOffset = 0;
					Uint32 morphVertexCount = 0;
					Uint32 morphTargetCount = 0;
					Uint32 morphWeightOffset = 0;

					if (!subMesh.morphs_.empty() && !crister->StandaloneVertices(selectedCluster))
					{
						Int ownerNodeIndex = -1;
						for (Size nodeIndex = 0; nodeIndex < nodes.size(); nodeIndex++)
						{
							if (nodes[nodeIndex].mesh_ == subMesh.meshIndex_)
							{
								ownerNodeIndex = static_cast<Int>(nodeIndex);
								break;
							}
						}

						auto entityWeightsIt = ownerNodeIndex >= 0 ? animatedMorphWeights_.find(entityID) : animatedMorphWeights_.end();
						if (entityWeightsIt != animatedMorphWeights_.end())
						{
							auto nodeWeightsIt = entityWeightsIt->second.find(ownerNodeIndex);
							if (nodeWeightsIt != entityWeightsIt->second.end() && !nodeWeightsIt->second.empty())
							{
								const DynamicArray<Float>& weights = nodeWeightsIt->second;
								if (morphWeights_.size() + weights.size() <= maxMorphWeightCount_)
								{
									morphWeightOffset = static_cast<Uint32>(morphWeights_.size());
									morphWeights_.insert(morphWeights_.end(), weights.begin(), weights.end());

									morphDeltaBufferIndex = crister->MorphDeltaBufferIndex();
									vertexMorphSourceBufferIndex = crister->VertexMorphSourceBufferIndex();
									morphDeltaOffset = subMesh.morphDeltaOffset_;
									morphVertexOffset = subMesh.vertexOffset_;
									morphVertexCount = subMesh.vertexCount_;
									morphTargetCount = static_cast<Uint32>(Min(weights.size(), subMesh.morphs_.size()));
								}
							}
						}
					}

					/// [EN] Keep every RESIDENT cluster of this chain warm, not just the
					///      selected one. Only the selected cluster is emitted as an
					///      instance (that is the dispatch-count win), but touching only
					///      it would let the neighbouring LODs age out and be evicted -
					///      and then any Scale/camera change that reselects one of them
					///      forces a re-upload whose MakeClusterResident does a blocking
					///      GPU wait, thrashing upload/evict every frame.
					/// [JP] このチェーンの「常駐」クラスタは、選択したものだけでなく全て
					///      warm に保つ。インスタンスとして発行するのは選択した 1 つだけ
					///      (ディスパッチ数削減の本体はそこ)だが、選択分だけを touch すると
					///      隣接 LOD が期限切れで追い出され、その後 Scale やカメラの変化で
					///      それらが再選択されるたびに再アップロードが必要になる —
					///      MakeClusterResident は GPU をブロッキング待ちするため、
					///      毎フレーム アップロード/追い出しのスラッシングになる。
					if (skinned)
					{
						crister->TouchCluster(subMesh.clusterOffset_, streamingFrame_);
					}
					else
					{
						for (Uint32 c = 0; c < subMesh.clusterCount_; ++c)
						{
							Uint32 clusterIndex = subMesh.clusterOffset_ + c;
							if (crister->IsClusterResident(clusterIndex))
							{
								crister->TouchCluster(clusterIndex, streamingFrame_);
							}
						}
					}

					{
						Uint32 clusterIndex = selectedCluster;
						const Cluster& cluster = clusters[clusterIndex];

						/// [EN] The CPU already picked this cluster, so the AS's
						///      IsLodSelected must pass unconditionally: a zero error
						///      always fits the 1px threshold and an infinite next error
						///      always exceeds it. Feeding the real error back instead
						///      would risk CPU/GPU float divergence rejecting the one
						///      cluster that was emitted, leaving the mesh invisible.
						/// [JP] このクラスタは CPU が既に選び終えているため、AS の
						///      IsLodSelected は無条件で通す必要がある: 誤差 0 は必ず
						///      1px 閾値に収まり、次の誤差が無限大なら必ず閾値を超える。
						///      実際の誤差を渡すと CPU と GPU の浮動小数の差で、唯一
						///      発行したクラスタが棄却されメッシュが消える恐れがある。
						Float lodErrorNext = FLT_MAX;

						constexpr Uint32 maxMeshletsPerDispatch = 32;
						Uint32 remaining = cluster.meshletCount_;
						Uint32 offset = cluster.meshletOffset_;

						while (remaining > 0)
						{
							Uint32 count = (remaining > maxMeshletsPerDispatch) ? maxMeshletsPerDispatch : remaining;

							ModelInstanceData instanceData{};
							instanceData.world_ = worldMatrix;
							instanceData.inverseTransposeWorld_ = inverseTransposeWorld;
							instanceData.previousWorld_ = previousWorldMatrix;

							instanceData.baseColor_ = material.baseColor_;
							instanceData.metallic_ = material.metallic_;
							instanceData.roughness_ = material.roughness_;
							/// [EN] MASK clips at the glTF alphaCutoff, BLEND at a tiny epsilon.
							///      OPAQUE must be 0 - glTF ignores the alpha channel entirely there.
							/// [JP] MASK は glTF の alphaCutoff、BLEND は微小値でクリップ。
							///      OPAQUE は 0 — glTF ではアルファを完全に無視する規定のため。
							instanceData.alphaCutoff_ = material.alphaMode_ == 1 ? material.alphaCutoff_ : (material.alphaMode_ == 2 ? 0.01f : 0.0f);
							instanceData.emissive_ = Vector3(material.emissiveFactor_[0], material.emissiveFactor_[1], material.emissiveFactor_[2]);

							/// [EN] KHR material extensions consumed by the deferred G-Buffer.
							/// [JP] deferred G-Buffer が消費する KHR マテリアル拡張。
							instanceData.ior_ = material.khr_.ior_.ior_;
							instanceData.emissiveStrength_ = material.khr_.emissiveStrength_.emissiveStrength_;
							instanceData.specularFactor_ = material.khr_.specular_.specularFactor_;
							instanceData.specularColor_ = Vector3(material.khr_.specular_.specularColorFactor_[0], material.khr_.specular_.specularColorFactor_[1], material.khr_.specular_.specularColorFactor_[2]);
							instanceData.clearCoatFactor_ = material.khr_.clearCoat_.clearCoatFactor_;
							instanceData.clearCoatRoughness_ = material.khr_.clearCoat_.clearCoatRoughnessFactor_;
							instanceData.anisotropy_ = material.khr_.anisotropy_.anisotropyStrength_;
							instanceData.transmissionFactor_ = material.khr_.transmission_.transmissionFactor_;
							instanceData.volumeThicknessFactor_ = material.khr_.volume_.thicknessFactor_;
							instanceData.volumeAttenuationDistance_ = material.khr_.volume_.attenuationDistance_;
							instanceData.volumeAttenuationColor_ = Vector3(material.khr_.volume_.attenuationColor_[0], material.khr_.volume_.attenuationColor_[1], material.khr_.volume_.attenuationColor_[2]);
							instanceData.sheenColor_ = Vector3(material.khr_.sheen_.sheenColorFactor_[0], material.khr_.sheen_.sheenColorFactor_[1], material.khr_.sheen_.sheenColorFactor_[2]);
							instanceData.sheenRoughness_ = material.khr_.sheen_.sheenRoughnessFactor_;
							instanceData.iridescenceFactor_ = material.khr_.iridescence_.iridescenceFactor_;
							instanceData.iridescenceIor_ = material.khr_.iridescence_.iridescenceIor_;
							instanceData.iridescenceThickness_ = (material.khr_.iridescence_.iridescenceThicknessMinimum_ + material.khr_.iridescence_.iridescenceThicknessMaximum_) * 0.5f;
							instanceData.unlit_ = material.khr_.unlit_.unlit_ != 0 ? 1.0f : 0.0f;

							/// [EN] Material stores glTF image indices — resolve to bindless heap indices.
							/// [JP] Material には glTF の image インデックスが入っているため、bindless ヒープインデックスに解決する。
							instanceData.baseColorTextureIndex_ = crister->TextureBindlessIndex(material.baseColorTextureIndex_);
							instanceData.normalTextureIndex_ = crister->TextureBindlessIndex(material.normalTextureIndex_);
							instanceData.metallicRoughnessTextureIndex_ = crister->TextureBindlessIndex(material.metallicRoughnessTextureIndex_);
							instanceData.emissiveTextureIndex_ = crister->TextureBindlessIndex(material.emissiveTextureIndex_);

							/// [EN] All geometry SRVs come from the cluster's resident
							///      page; meshletOffset_ is page-local (the page's
							///      meshlets were rebased at upload).
							/// [JP] ジオメトリ SRV はすべてクラスタの常駐ページから取る。
							///      meshletOffset_ はページローカル（ページの meshlet は
							///      アップロード時にリベース済み）。
							instanceData.vertexBufferIndex_ = crister->ClusterVertexBufferIndex(clusterIndex);
							instanceData.skinVertexBufferIndex_ = crister->SkinVertexBufferIndex();
							instanceData.positionMin_ = crister->PositionMin();
							instanceData.positionExtent_ = crister->PositionExtent();
							instanceData.texcoordMinU_ = crister->TexcoordMin().x;
							instanceData.texcoordMinV_ = crister->TexcoordMin().y;
							instanceData.texcoordExtent_ = crister->TexcoordExtent();
							instanceData.meshletBufferIndex_ = crister->ClusterMeshletBufferIndex(clusterIndex);
							instanceData.meshletBoundBufferIndex_ = crister->ClusterMeshletBoundBufferIndex(clusterIndex);
							instanceData.vertexIndicesBufferIndex_ = crister->ClusterVertexIndicesBufferIndex(clusterIndex);
							instanceData.primitiveIndicesBufferIndex_ = crister->ClusterPrimitiveIndicesBufferIndex(clusterIndex);

							instanceData.morphDeltaBufferIndex_ = morphDeltaBufferIndex;
							instanceData.vertexMorphSourceBufferIndex_ = vertexMorphSourceBufferIndex;
							instanceData.morphDeltaOffset_ = morphDeltaOffset;
							instanceData.morphVertexOffset_ = morphVertexOffset;
							instanceData.morphVertexCount_ = morphVertexCount;
							instanceData.morphTargetCount_ = morphTargetCount;
							instanceData.morphWeightOffset_ = morphWeightOffset;

							instanceData.meshletOffset_ = offset - cluster.meshletOffset_;
							instanceData.meshletCount_ = count;

							instanceData.lodError_ = 0.0f;
							instanceData.lodErrorNext_ = lodErrorNext;

							/// [EN] Skinned SubMesh: point at this Crister's palette slice.
							///      On palette overflow fall back to static rendering.
							/// [JP] スキンド SubMesh: この Crister のパレット領域を指す。
							///      パレットあふれ時は静的描画にフォールバックする。
							if (skinned)
							{
								Uint skinBoneOffset = boneBase;
								for (Int skinIndex = 0; skinIndex < subMesh.skinIndex_; skinIndex++)
								{
									skinBoneOffset += static_cast<Uint>(skins[skinIndex].joints_.size());
								}
								instanceData.skinIndex_ = static_cast<Uint>(subMesh.skinIndex_);
								instanceData.boneOffset_ = skinBoneOffset;
							}
							else
							{
								instanceData.skinIndex_ = 0xFFFFFFFF;
								instanceData.boneOffset_ = 0;
							}

							instanceData.doubleSided_ = material.doubleSided_ ? 1 : 0;
							instanceData.blend_ = material.alphaMode_ == 2 ? 1 : 0;

							instanceData.selected_ = (selectedEntity.Exists() && actor->GetEntity() == selectedEntity) ? 1 : 0;
							if (instanceData.selected_)
							{
								hasSelectedInstance_ = true;
								hasSelectedSkinned_ = hasSelectedSkinned_ || skinned;
							}

							/// [EN] OPAQUE(0) and MASK(1) both go through the opaque G-Buffer path
							///      (MASK is a cutout handled by clip() in the PS). Only BLEND(2)
							///      needs the OIT transparent path.
							/// [JP] OPAQUE(0) と MASK(1) は両方とも不透明 G-Buffer パスで描く
							///      (MASK は PS の clip() で処理するカットアウト)。OIT 透過パスが
							///      必要なのは BLEND(2) のみ。
							if (material.alphaMode_ != 2)
							{
								opaqueInstances_.push_back(instanceData);
								hasSkinnedOpaque_ = hasSkinnedOpaque_ || instanceData.skinIndex_ != 0xFFFFFFFF;
							}
							else
							{
								transparentInstances_.push_back(instanceData);
								hasSkinnedTransparent_ = hasSkinnedTransparent_ || instanceData.skinIndex_ != 0xFFFFFFFF;
							}

							offset += count;
							remaining -= count;
						}
					}
				}
			});

		/// [EN] Softbody actors: each gets exactly one ModelInstanceData
		///      sourced from its own SoftbodyMesh (built/re-quantised below),
		///      not from Crister's cluster/LOD streaming path — see
		///      SoftbodyMesh's class comment for why. Walked via
		///      World::GetComponents (SparseSet component, same as
		///      PhysicsSystem::ResolveSoftbodies), not Query<>, since Softbody
		///      is a SeedScript component like Animator/Rigidbody.
		/// [JP] Softbody アクター: それぞれ自身の SoftbodyMesh（下で構築/
		///      再量子化）から作った ModelInstanceData を1つだけ持つ —
		///      Crister のクラスタ/LOD ストリーミング経路は使わない
		///      （理由は SoftbodyMesh のクラスコメント参照）。SparseSet
		///      コンポーネントのため Query<> ではなく World::GetComponents
		///      で走査する（PhysicsSystem::ResolveSoftbodies と同じ —
		///      Softbody は Animator/Rigidbody と同じ SeedScript コンポーネント）。
		for (EntityID entityID : world.GetComponents<Softbody>())
		{
			Actor* actor = world.GetActor(entityID);
			if (!actor)
			{
				continue;
			}

			const Active* active = actor->GetComponent<Active>();
			if (active && !active->active_)
			{
				continue;
			}

			Softbody* softbody = actor->GetComponent<Softbody>();
			if (!softbody || !softbody->HasBody())
			{
				continue;
			}

			const Mesh* mesh = actor->GetComponent<Mesh>();
			if (!mesh)
			{
				continue;
			}

			Handle<Crister> cristerHandle = modelResource.GetHandle(mesh->meshID_);
			if (cristerHandle.empty())
			{
				continue;
			}

			Crister* crister = modelResource.Resolve(loaderSystem, cristerHandle);
			if (!crister)
			{
				continue;
			}

			ResourcePtr<SoftbodyMesh>& softbodyMesh = softbodyMeshes_[entityID];
			if (!softbodyMesh)
			{
				softbodyMesh = MakePtr<SoftbodyMesh>();
				if (!softbodyMesh->Create(device_, bindlessHeap_, *crister))
				{
					softbodyMeshes_.erase(entityID);
					continue;
				}
			}

			softbodyMesh->Update(softbody->GetVertexPositions());

			Matrix worldMatrix = actor->GetWorldMatrix();
			Matrix inverseTransposeWorld = worldMatrix.Invert().Transpose();

			auto previousWorldIt = previousWorldMatrices_.find(entityID);
			Matrix previousWorldMatrix = previousWorldIt != previousWorldMatrices_.end() ? previousWorldIt->second : worldMatrix;
			previousWorldMatrices_[entityID] = worldMatrix;

			const auto& materials = crister->Materials();
			Material defaultMaterial;
			const Material& material = materials.empty() ? defaultMaterial : materials[0];

			constexpr Uint32 maxMeshletsPerDispatch = 32;
			Uint32 remaining = softbodyMesh->MeshletCount();
			Uint32 offset = 0;

			while (remaining > 0)
			{
				Uint32 count = (remaining > maxMeshletsPerDispatch) ? maxMeshletsPerDispatch : remaining;

				ModelInstanceData instanceData{};
				instanceData.world_ = worldMatrix;
				instanceData.inverseTransposeWorld_ = inverseTransposeWorld;
				instanceData.previousWorld_ = previousWorldMatrix;

				instanceData.baseColor_ = material.baseColor_;
				instanceData.metallic_ = material.metallic_;
				instanceData.roughness_ = material.roughness_;
				instanceData.alphaCutoff_ = material.alphaMode_ == 1 ? material.alphaCutoff_ : (material.alphaMode_ == 2 ? 0.01f : 0.0f);
				instanceData.emissive_ = Vector3(material.emissiveFactor_[0], material.emissiveFactor_[1], material.emissiveFactor_[2]);

				instanceData.ior_ = material.khr_.ior_.ior_;
				instanceData.emissiveStrength_ = material.khr_.emissiveStrength_.emissiveStrength_;
				instanceData.specularFactor_ = material.khr_.specular_.specularFactor_;
				instanceData.specularColor_ = Vector3(material.khr_.specular_.specularColorFactor_[0], material.khr_.specular_.specularColorFactor_[1], material.khr_.specular_.specularColorFactor_[2]);
				instanceData.clearCoatFactor_ = material.khr_.clearCoat_.clearCoatFactor_;
				instanceData.clearCoatRoughness_ = material.khr_.clearCoat_.clearCoatRoughnessFactor_;
				instanceData.anisotropy_ = material.khr_.anisotropy_.anisotropyStrength_;
				instanceData.transmissionFactor_ = material.khr_.transmission_.transmissionFactor_;
				instanceData.volumeThicknessFactor_ = material.khr_.volume_.thicknessFactor_;
				instanceData.volumeAttenuationDistance_ = material.khr_.volume_.attenuationDistance_;
				instanceData.volumeAttenuationColor_ = Vector3(material.khr_.volume_.attenuationColor_[0], material.khr_.volume_.attenuationColor_[1], material.khr_.volume_.attenuationColor_[2]);
				instanceData.sheenColor_ = Vector3(material.khr_.sheen_.sheenColorFactor_[0], material.khr_.sheen_.sheenColorFactor_[1], material.khr_.sheen_.sheenColorFactor_[2]);
				instanceData.sheenRoughness_ = material.khr_.sheen_.sheenRoughnessFactor_;
				instanceData.iridescenceFactor_ = material.khr_.iridescence_.iridescenceFactor_;
				instanceData.iridescenceIor_ = material.khr_.iridescence_.iridescenceIor_;
				instanceData.iridescenceThickness_ = (material.khr_.iridescence_.iridescenceThicknessMinimum_ + material.khr_.iridescence_.iridescenceThicknessMaximum_) * 0.5f;
				instanceData.unlit_ = material.khr_.unlit_.unlit_ != 0 ? 1.0f : 0.0f;

				instanceData.baseColorTextureIndex_ = crister->TextureBindlessIndex(material.baseColorTextureIndex_);
				instanceData.normalTextureIndex_ = crister->TextureBindlessIndex(material.normalTextureIndex_);
				instanceData.metallicRoughnessTextureIndex_ = crister->TextureBindlessIndex(material.metallicRoughnessTextureIndex_);
				instanceData.emissiveTextureIndex_ = crister->TextureBindlessIndex(material.emissiveTextureIndex_);

				/// [EN] SoftbodyMesh's own buffers, not Crister's — see
				///      SoftbodyMesh's class comment.
				/// [JP] Crister のではなく SoftbodyMesh 自身のバッファ —
				///      SoftbodyMesh のクラスコメント参照。
				instanceData.vertexBufferIndex_ = softbodyMesh->VertexBufferIndex();
				instanceData.skinVertexBufferIndex_ = 0xFFFFFFFF;
				instanceData.positionMin_ = softbodyMesh->PositionMin();
				instanceData.positionExtent_ = softbodyMesh->PositionExtent();
				instanceData.texcoordMinU_ = softbodyMesh->TexcoordMin().x;
				instanceData.texcoordMinV_ = softbodyMesh->TexcoordMin().y;
				instanceData.texcoordExtent_ = softbodyMesh->TexcoordExtent();
				instanceData.meshletBufferIndex_ = softbodyMesh->MeshletBufferIndex();
				instanceData.meshletBoundBufferIndex_ = softbodyMesh->MeshletBoundBufferIndex();
				instanceData.vertexIndicesBufferIndex_ = softbodyMesh->VertexIndicesBufferIndex();
				instanceData.primitiveIndicesBufferIndex_ = softbodyMesh->PrimitiveIndicesBufferIndex();

				instanceData.meshletOffset_ = offset;
				instanceData.meshletCount_ = count;

				instanceData.lodError_ = 0.0f;
				instanceData.lodErrorNext_ = FLT_MAX;

				instanceData.skinIndex_ = 0xFFFFFFFF;
				instanceData.boneOffset_ = 0;

				instanceData.doubleSided_ = material.doubleSided_ ? 1 : 0;
				instanceData.blend_ = material.alphaMode_ == 2 ? 1 : 0;

				instanceData.selected_ = (selectedEntity.Exists() && actor->GetEntity() == selectedEntity) ? 1 : 0;
				if (instanceData.selected_)
				{
					hasSelectedInstance_ = true;
				}

				if (material.alphaMode_ != 2)
				{
					opaqueInstances_.push_back(instanceData);
				}
				else
				{
					transparentInstances_.push_back(instanceData);
				}

				offset += count;
				remaining -= count;
			}
		}

		/// [EN] Streaming upkeep: bring in this frame's requested pages (capped
		///      per frame so a camera cut doesn't stall a frame on uploads),
		///      then evict cold pages until the VRAM budget is met. Requested
		///      pages become visible next Gather — until then the coarser
		///      resident cluster keeps rendering, so there is never a hole.
		/// [JP] ストリーミング処理: 今フレームの要求ページをアップロードし
		///      （カメラカットでアップロード詰まりしないようフレームあたり上限）、
		///      VRAM 予算に収まるまでコールドなページを追い出す。要求ページは
		///      次の Gather から使われ、それまでは粗い常駐クラスタが描画を
		///      続けるため穴は開かない。
		constexpr Uint maxUploadsPerFrame = 4;
		Uint uploads = 0;
		for (const auto& request : geometryStreamingRequests_)
		{
			if (uploads >= maxUploadsPerFrame)
			{
				break;
			}
			if (!request.first->IsClusterResident(request.second))
			{
				request.first->MakeClusterResident(request.second);
				request.first->TouchCluster(request.second, streamingFrame_);
				uploads++;
			}
		}
		geometryStreamingRequests_.clear();

		constexpr Uint maxTextureUploadsPerFrame = 8;
		Uint textureUploads = 0;
		for (const auto& request : textureStreamingRequests_)
		{
			if (textureUploads >= maxTextureUploadsPerFrame)
			{
				break;
			}
			if (request.crister_->TextureFinestMip(request.textureIndex_) > request.desiredMip_)
			{
				request.crister_->MakeTextureMipResident(request.textureIndex_, request.desiredMip_);
				request.crister_->TouchTexture(request.textureIndex_, streamingFrame_);
				textureUploads++;
			}
		}
		textureStreamingRequests_.clear();

		Crister::EvictClusterBudget(streamingFrame_);
		Crister::EvictTextureBudget(streamingFrame_);
	}

	void ModelRenderer::Upload()
	{
		/// [EN] Upload is invoked once per view (editor / game) but the gathered
		///      data is view-independent — copy the tens of megabytes only once
		///      per Gather.
		/// [JP] Upload はビューごと（エディタ / ゲーム）に呼ばれるが、収集済み
		///      データはビュー非依存 — 数十 MB のコピーは Gather ごとに 1 回だけにする。
		if (uploaded_)
		{
			return;
		}
		uploaded_ = true;

		/// [EN] Frame-ring buffers: re-register the current frame's SRV indices.
		/// [JP] フレームリングバッファ: 現在フレームの SRV インデックスを再登録する。
		indicesSystem_->SetModelInstanceIndex(instanceBuffer_->Index());
		indicesSystem_->SetModelBoneMatrixIndex(boneBuffer_->Index());
		indicesSystem_->SetModelMorphWeightIndex(morphWeightBuffer_->Index());

		if (!opaqueInstances_.empty() || !transparentInstances_.empty())
		{
			DynamicArray<ModelInstanceData> allInstances;
			allInstances.reserve(opaqueInstances_.size() + transparentInstances_.size());
			allInstances.insert(allInstances.end(), opaqueInstances_.begin(), opaqueInstances_.end());
			allInstances.insert(allInstances.end(), transparentInstances_.begin(), transparentInstances_.end());

			instanceBuffer_->Update(allInstances.data(), static_cast<Uint>(allInstances.size()));
		}

		if (!boneMatrices_.empty())
		{
			boneBuffer_->Update(boneMatrices_.data(), static_cast<Uint>(boneMatrices_.size()));
		}

		if (!morphWeights_.empty())
		{
			morphWeightBuffer_->Update(morphWeights_.data(), static_cast<Uint>(morphWeights_.size()));
		}
	}

	D3D12_GPU_VIRTUAL_ADDRESS ModelRenderer::BoneMatrixBufferGPUAddress()const
	{
		return boneBuffer_->GPUVirtualAddress();
	}

	Bool ModelRenderer::TryGetAnimatedBoneOffset(EntityID entityID, Uint32& outBoneOffset)const
	{
		auto found = animatedBoneOffsets_.find(entityID);
		if (found == animatedBoneOffsets_.end())
		{
			return false;
		}
		outBoneOffset = found->second;
		return true;
	}

	Bool ModelRenderer::TryGetAnimatedMorphWeights(EntityID entityID, Int nodeIndex, DynamicArray<Float>& outWeights)const
	{
		auto entityIt = animatedMorphWeights_.find(entityID);
		if (entityIt == animatedMorphWeights_.end())
		{
			return false;
		}
		auto nodeIt = entityIt->second.find(nodeIndex);
		if (nodeIt == entityIt->second.end())
		{
			return false;
		}
		outWeights = nodeIt->second;
		return true;
	}

	void ModelRenderer::DrawDepthPrepass(D3D12CommandList* cmdList, ID3D12DescriptorHeap* heap, D3D12_GPU_VIRTUAL_ADDRESS constantIndex, D3D12_GPU_VIRTUAL_ADDRESS structuredIndex)
	{
		if (opaqueInstances_.empty())
		{
			return;
		}

		auto* cmd = cmdList->Get();

		ID3D12DescriptorHeap* heaps[] = { heap };
		cmd->SetDescriptorHeaps(_countof(heaps), heaps);
		cmd->SetGraphicsRootSignature(modelShader_.GetRootSignature());
		cmd->SetGraphicsRootConstantBufferView(2, constantIndex);
		cmd->SetGraphicsRootConstantBufferView(3, structuredIndex);
		cmd->SetGraphicsRootDescriptorTable(0, bindlessHeap_->GPUHandle(0));

		/// [EN] All passes dispatch the full instance list; the AS entry skips
		///      instances that belong to the other pass (blend_ filter).
		/// [JP] 全パスが全インスタンスをディスパッチし、AS エントリが他方の
		///      パスに属するインスタンスをスキップする（blend_ フィルタ）。
		cmd->SetPipelineState(modelShader_.GetPipelineStateDepthPrepass());
		cmd->DispatchMesh(static_cast<Uint>(opaqueInstances_.size() + transparentInstances_.size()), 1, 1);
		ProfilerStats::AddDrawCall();
	}

	void ModelRenderer::DrawOpaque(D3D12CommandList* cmdList, ID3D12DescriptorHeap* heap, D3D12_GPU_VIRTUAL_ADDRESS constantIndex, D3D12_GPU_VIRTUAL_ADDRESS structuredIndex)
	{
		if (opaqueInstances_.empty())
		{
			return;
		}

		auto* cmd = cmdList->Get();

		ID3D12DescriptorHeap* heaps[] = { heap };
		cmd->SetDescriptorHeaps(_countof(heaps), heaps);
		cmd->SetGraphicsRootSignature(modelShader_.GetRootSignature());
		cmd->SetGraphicsRootConstantBufferView(2, constantIndex);
		cmd->SetGraphicsRootConstantBufferView(3, structuredIndex);
		cmd->SetGraphicsRootDescriptorTable(0, bindlessHeap_->GPUHandle(0));

		cmd->SetPipelineState(modelShader_.GetPipelineStateStatic());
		cmd->DispatchMesh(static_cast<Uint>(opaqueInstances_.size() + transparentInstances_.size()), 1, 1);
		ProfilerStats::AddDrawCall();

		/// [EN] Skinned instances are skipped by StaticModelMS and drawn here by
		///      the skeletal pipeline (SkeletalModelMS filters the inverse set).
		/// [JP] スキンインスタンスは StaticModelMS でスキップされ、ここでスケルタル
		///      パイプラインが描画する（SkeletalModelMS が逆の集合をフィルタする）。
		if (hasSkinnedOpaque_)
		{
			cmd->SetPipelineState(modelShader_.GetPipelineStateSkeletal());
			cmd->DispatchMesh(static_cast<Uint>(opaqueInstances_.size() + transparentInstances_.size()), 1, 1);
			ProfilerStats::AddDrawCall();
		}
	}

	void ModelRenderer::Compose(D3D12CommandList* cmdList, FrameBuffer* frameBuffer, ID3D12DescriptorHeap* heap, D3D12_GPU_VIRTUAL_ADDRESS constantIndex, D3D12_GPU_VIRTUAL_ADDRESS structuredIndex)
	{
		frameBuffer->Rebind(cmdList);

		auto* cmd = cmdList->Get();

		ID3D12DescriptorHeap* heaps[] = { heap };
		cmd->SetDescriptorHeaps(_countof(heaps), heaps);
		cmd->SetGraphicsRootSignature(modelShader_.GetRootSignature());
		cmd->SetGraphicsRootConstantBufferView(2, constantIndex);
		cmd->SetGraphicsRootConstantBufferView(3, structuredIndex);
		cmd->SetGraphicsRootDescriptorTable(0, bindlessHeap_->GPUHandle(0));

		cmd->SetPipelineState(modelShader_.GetPipelineStateComposite());
		cmd->DispatchMesh(1, 1, 1);
		ProfilerStats::AddDrawCall();
	}

	void ModelRenderer::DrawWireframe(D3D12CommandList* cmdList, FrameBuffer* frameBuffer, GeometryBuffer* geometryBuffer, ID3D12DescriptorHeap* heap, D3D12_GPU_VIRTUAL_ADDRESS constantIndex, D3D12_GPU_VIRTUAL_ADDRESS structuredIndex)
	{
		if (opaqueInstances_.empty())
		{
			return;
		}

		auto* cmd = cmdList->Get();

		/// [JP] エディタフレームバッファの色 ＋ ジオメトリ深度（読み取りのみ）を bind。
		D3D12_CPU_DESCRIPTOR_HANDLE renderTargetViewHandle = frameBuffer->RenderTargetViewHandle();
		D3D12_CPU_DESCRIPTOR_HANDLE depthStencilViewHandle = geometryBuffer->DepthStencilViewHandle();
		cmd->OMSetRenderTargets(1, &renderTargetViewHandle, FALSE, &depthStencilViewHandle);

		D3D12_VIEWPORT viewport = frameBuffer->GetViewport();
		cmd->RSSetViewports(1, &viewport);
		D3D12_RECT scissorRect = { 0, 0, static_cast<LONG>(viewport.Width), static_cast<LONG>(viewport.Height) };
		cmd->RSSetScissorRects(1, &scissorRect);

		ID3D12DescriptorHeap* heaps[] = { heap };
		cmd->SetDescriptorHeaps(_countof(heaps), heaps);
		cmd->SetGraphicsRootSignature(modelShader_.GetRootSignature());
		cmd->SetGraphicsRootConstantBufferView(2, constantIndex);
		cmd->SetGraphicsRootConstantBufferView(3, structuredIndex);
		cmd->SetGraphicsRootDescriptorTable(0, bindlessHeap_->GPUHandle(0));

		cmd->SetPipelineState(modelShader_.GetPipelineStateWireframeStatic());
		cmd->DispatchMesh(static_cast<Uint>(opaqueInstances_.size() + transparentInstances_.size()), 1, 1);
		ProfilerStats::AddDrawCall();

		if (hasSkinnedOpaque_)
		{
			cmd->SetPipelineState(modelShader_.GetPipelineStateWireframeSkeletal());
			cmd->DispatchMesh(static_cast<Uint>(opaqueInstances_.size() + transparentInstances_.size()), 1, 1);
			ProfilerStats::AddDrawCall();
		}
	}

	void ModelRenderer::DrawMeshlet(D3D12CommandList* cmdList, FrameBuffer* frameBuffer, GeometryBuffer* geometryBuffer, ID3D12DescriptorHeap* heap, D3D12_GPU_VIRTUAL_ADDRESS constantIndex, D3D12_GPU_VIRTUAL_ADDRESS structuredIndex)
	{
		if (opaqueInstances_.empty())
		{
			return;
		}

		auto* cmd = cmdList->Get();

		D3D12_CPU_DESCRIPTOR_HANDLE renderTargetViewHandle = frameBuffer->RenderTargetViewHandle();
		D3D12_CPU_DESCRIPTOR_HANDLE depthStencilViewHandle = geometryBuffer->DepthStencilViewHandle();
		cmd->OMSetRenderTargets(1, &renderTargetViewHandle, FALSE, &depthStencilViewHandle);

		D3D12_VIEWPORT viewport = frameBuffer->GetViewport();
		cmd->RSSetViewports(1, &viewport);
		D3D12_RECT scissorRect = { 0, 0, static_cast<LONG>(viewport.Width), static_cast<LONG>(viewport.Height) };
		cmd->RSSetScissorRects(1, &scissorRect);

		ID3D12DescriptorHeap* heaps[] = { heap };
		cmd->SetDescriptorHeaps(_countof(heaps), heaps);
		cmd->SetGraphicsRootSignature(modelShader_.GetRootSignature());
		cmd->SetGraphicsRootConstantBufferView(2, constantIndex);
		cmd->SetGraphicsRootConstantBufferView(3, structuredIndex);
		cmd->SetGraphicsRootDescriptorTable(0, bindlessHeap_->GPUHandle(0));

		cmd->SetPipelineState(modelShader_.GetPipelineStateMeshletStatic());
		cmd->DispatchMesh(static_cast<Uint>(opaqueInstances_.size() + transparentInstances_.size()), 1, 1);
		ProfilerStats::AddDrawCall();

		if (hasSkinnedOpaque_)
		{
			cmd->SetPipelineState(modelShader_.GetPipelineStateMeshletSkeletal());
			cmd->DispatchMesh(static_cast<Uint>(opaqueInstances_.size() + transparentInstances_.size()), 1, 1);
			ProfilerStats::AddDrawCall();
		}
	}

	void ModelRenderer::DrawTransparent(D3D12CommandList* cmdList, FrameBuffer* frameBuffer, GeometryBuffer* geometryBuffer, ID3D12DescriptorHeap* heap, D3D12_GPU_VIRTUAL_ADDRESS constantIndex, D3D12_GPU_VIRTUAL_ADDRESS structuredIndex)
	{
		if (transparentInstances_.empty())
		{
			return;
		}

		auto* cmd = cmdList->Get();

		oitBuffer_.Clear(cmd);
		oitBuffer_.Barrier(cmd);

		geometryBuffer->BeginDepthOnly(cmdList);

		ID3D12DescriptorHeap* heaps[] = { heap };
		cmd->SetDescriptorHeaps(_countof(heaps), heaps);
		cmd->SetGraphicsRootSignature(modelShader_.GetRootSignature());
		cmd->SetGraphicsRootConstantBufferView(2, constantIndex);
		cmd->SetGraphicsRootConstantBufferView(3, structuredIndex);
		cmd->SetGraphicsRootDescriptorTable(0, bindlessHeap_->GPUHandle(0));

		cmd->SetPipelineState(modelShader_.GetPipelineStateStaticTransparent());
		cmd->DispatchMesh(static_cast<Uint>(opaqueInstances_.size() + transparentInstances_.size()), 1, 1);
		ProfilerStats::AddDrawCall();

		if (hasSkinnedTransparent_)
		{
			oitBuffer_.Barrier(cmd);

			cmd->SetPipelineState(modelShader_.GetPipelineStateSkeletalTransparent());
			cmd->DispatchMesh(static_cast<Uint>(opaqueInstances_.size() + transparentInstances_.size()), 1, 1);
			ProfilerStats::AddDrawCall();
		}

		oitBuffer_.Barrier(cmd);

		geometryBuffer->EndDepth(cmdList);

		frameBuffer->Rebind(cmdList);

		cmd->SetPipelineState(modelShader_.GetPipelineStateResolve());
		cmd->DispatchMesh(1, 1, 1);
		ProfilerStats::AddDrawCall();
	}

	void ModelRenderer::DrawSelectionMask(D3D12CommandList* cmdList, ID3D12DescriptorHeap* heap, D3D12_GPU_VIRTUAL_ADDRESS constantIndex, D3D12_GPU_VIRTUAL_ADDRESS structuredIndex)
	{
		if (!hasSelectedInstance_)
		{
			return;
		}

		auto* cmd = cmdList->Get();

		/// [JP] 深度なしで選択メッシュのシルエット全体を描く。手前の未選択オブジェクト
		///      に遮蔽されても穴を開けない（理由は SelectionMask PSO のコメント参照）。
		///      マスクの Begin/Clear/End は呼び出し側（Renderer）が一括で行う
		///      （Model/Sprite/Billboard/Font が同じ 1 枚のマスクを共有するため）。
		ID3D12DescriptorHeap* heaps[] = { heap };
		cmd->SetDescriptorHeaps(_countof(heaps), heaps);
		cmd->SetGraphicsRootSignature(modelShader_.GetRootSignature());
		cmd->SetGraphicsRootConstantBufferView(2, constantIndex);
		cmd->SetGraphicsRootConstantBufferView(3, structuredIndex);
		cmd->SetGraphicsRootDescriptorTable(0, bindlessHeap_->GPUHandle(0));

		cmd->SetPipelineState(modelShader_.GetPipelineStateSelectionMaskStatic());
		cmd->DispatchMesh(static_cast<Uint>(opaqueInstances_.size() + transparentInstances_.size()), 1, 1);
		ProfilerStats::AddDrawCall();

		if (hasSelectedSkinned_)
		{
			cmd->SetPipelineState(modelShader_.GetPipelineStateSelectionMaskSkeletal());
			cmd->DispatchMesh(static_cast<Uint>(opaqueInstances_.size() + transparentInstances_.size()), 1, 1);
			ProfilerStats::AddDrawCall();
		}
	}

}
