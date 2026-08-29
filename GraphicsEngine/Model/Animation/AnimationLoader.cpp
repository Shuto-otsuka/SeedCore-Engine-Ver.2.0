#include <GraphicsEngine/Model/Animation/AnimationLoader.h>
#include <FoundationEngine/Resource/LoaderSystem.h>
#include <FoundationEngine/Serialization/Binary/BinaryArchive.h>

namespace SeedCore
{
	Handle<Animation> AnimationLoader::Load(LoaderSystem& loader, String filePath)
	{
		std::filesystem::path path(filePath.c_str());
		if (path.extension() != ".animation" || !std::filesystem::exists(path))
		{
			return Handle<Animation>::null();
		}

		Handle<Animation> handle = pool_.Create();
		Animation* animation = pool_.Get(handle);
		if (!animation)
		{
			return Handle<Animation>::null();
		}

		BinaryInputArchive archive;
		if (!archive.Read(filePath))
		{
			pool_.Destroy(handle);
			return Handle<Animation>::null();
		}
		animation->Serialize(archive);

		return handle;
	}

	Bool AnimationLoader::Save(Animation& animation, const std::filesystem::path& filePath)
	{
		BinaryOutputArchive archive;
		animation.Serialize(archive);
		return archive.Write(String(filePath.string()));
	}

	Animation* AnimationLoader::Get(const Handle<Animation>& handle)
	{
		return pool_.Get(handle);
	}

	void AnimationLoader::Clear(Handle<Animation>& handle)noexcept
	{
		pool_.Destroy(handle);
	}

	void AnimationLoader::SplitClips(String filePath)
	{
		std::filesystem::path path(filePath.c_str());

		if (path.extension() != ".gltf" && path.extension() != ".glb")
		{
			return;
		}

		tinygltf::Model model;
		tinygltf::TinyGLTF tinyLoader;
		std::string error, warning;

		Bool result = false;
		std::string pathString = path.string();
		if (path.extension() == ".glb")
		{
			result = tinyLoader.LoadBinaryFromFile(&model, &error, &warning, pathString);
		}
		else
		{
			result = tinyLoader.LoadASCIIFromFile(&model, &error, &warning, pathString);
		}

		if (!result)
		{
			return;
		}

		if (model.animations.empty())
		{
			return;
		}

		std::filesystem::path directory = path.parent_path() / (path.stem().string() + ".Animations");
		std::error_code directoryError;
		std::filesystem::create_directories(directory, directoryError);

		for (Size clipIndex = 0; clipIndex < model.animations.size(); ++clipIndex)
		{
			const tinygltf::Animation& gltfAnimation = model.animations[clipIndex];

			Animation animation;
			FetchAnimationClip(model, gltfAnimation, animation);

			std::string clipName = gltfAnimation.name.empty() ? ("Animation" + std::to_string(clipIndex)) : gltfAnimation.name;
			for (Char& c : clipName)
			{
				if (c == '<' || c == '>' || c == ':' || c == '"' || c == '/' || c == '\\' || c == '|' || c == '?' || c == '*')
				{
					c = '_';
				}
			}

			std::filesystem::path clipPath = directory / (clipName + ".animation");

			BinaryOutputArchive archive;
			animation.Serialize(archive);
			archive.Write(String(clipPath.string()));
		}
	}

	void AnimationLoader::FetchAnimationClip(const tinygltf::Model& model, const tinygltf::Animation& gltfAnimation, Animation& animation)
	{
		animation.name_ = gltfAnimation.name;

		for (const tinygltf::AnimationSampler& gltfSampler : gltfAnimation.samplers)
		{
			AnimationSampler& sampler = animation.samplers_.emplace_back();
			sampler.input_ = gltfSampler.input;
			sampler.output_ = gltfSampler.output;
			sampler.interpolation_ = gltfSampler.interpolation;
		}

		for (const tinygltf::AnimationChannel& gltfChannel : gltfAnimation.channels)
		{
			AnimationChannel& channel = animation.channels_.emplace_back();
			channel.sampler_ = gltfChannel.sampler;
			channel.targetNode_ = gltfChannel.target_node;
			channel.targetPath_ = gltfChannel.target_path;

			const tinygltf::AnimationSampler& gltfSampler = gltfAnimation.samplers.at(gltfChannel.sampler);

			/// [EN] Keyed by target NODE index (not the sampler's accessor
			///      index) so Animation::SamplePose can look values up by
			///      node while walking the Crister's node hierarchy.
			/// [JP] サンプラーのaccessorインデックスではなく、対象ノード番号を
			///      キーにする — Animation::SamplePoseがCristerのノード階層を
			///      辿りながらノード番号で値を引けるようにするため。
			if (!animation.timelines_.contains(gltfChannel.target_node))
			{
				const tinygltf::Accessor& timeAccessor = model.accessors.at(gltfSampler.input);
				const tinygltf::BufferView& timeBufferView = model.bufferViews.at(timeAccessor.bufferView);
				DynamicArray<Float>& times = animation.timelines_[gltfChannel.target_node];
				times.resize(timeAccessor.count);
				memcpy(times.data(), model.buffers.at(timeBufferView.buffer).data.data() + timeBufferView.byteOffset + timeAccessor.byteOffset, timeAccessor.count * sizeof(Float));
			}

			const tinygltf::Accessor& accessor = model.accessors.at(gltfSampler.output);
			const tinygltf::BufferView& bufferView = model.bufferViews.at(accessor.bufferView);
			const Uchar* data = model.buffers.at(bufferView.buffer).data.data() + bufferView.byteOffset + accessor.byteOffset;

			if (gltfChannel.target_path == "rotation" && !animation.rotations_.contains(gltfChannel.target_node))
			{
				DynamicArray<Quaternion>& rotations = animation.rotations_[gltfChannel.target_node];
				rotations.resize(accessor.count);
				memcpy(rotations.data(), data, accessor.count * sizeof(Quaternion));

				/// [EN] Same right-handed to left-handed conversion ModelLoader
				///      applies to Node::rotation_ — otherwise animated nodes
				///      end up in a different handedness than static ones.
				/// [JP] ModelLoaderがNode::rotation_に適用しているのと同じ
				///      右手系→左手系変換。これをしないとアニメーションで
				///      動くノードだけ静的ノードと座標系がズレる。
				for (Quaternion& rotation : rotations)
				{
					rotation.y = -rotation.y;
					rotation.z = -rotation.z;
				}
			}
			else if (gltfChannel.target_path == "scale" && !animation.scales_.contains(gltfChannel.target_node))
			{
				DynamicArray<Vector3>& scales = animation.scales_[gltfChannel.target_node];
				scales.resize(accessor.count);
				memcpy(scales.data(), data, accessor.count * sizeof(Vector3));
			}
			else if (gltfChannel.target_path == "translation" && !animation.translations_.contains(gltfChannel.target_node))
			{
				DynamicArray<Vector3>& translations = animation.translations_[gltfChannel.target_node];
				translations.resize(accessor.count);
				memcpy(translations.data(), data, accessor.count * sizeof(Vector3));

				/// [EN] Same right-handed to left-handed conversion ModelLoader
				///      applies to Node::translation_.
				/// [JP] ModelLoaderがNode::translation_に適用しているのと
				///      同じ右手系→左手系変換。
				for (Vector3& translation : translations)
				{
					translation.x = -translation.x;
				}
			}
			else if (gltfChannel.target_path == "weights" && !animation.weights_.contains(gltfChannel.target_node))
			{
				/// [EN] glTF packs a "weights" channel's output accessor as
				///      one flat SCALAR run of keyframeCount * targetCount
				///      floats (all targets for keyframe 0, then all targets
				///      for keyframe 1, ...) rather than nesting it per
				///      keyframe like the vector types above. targetCount
				///      comes from the target node's own mesh — its default
				///      weights (Node::weights) if authored, otherwise its
				///      first primitive's morph target count — since nothing
				///      on the sampler itself records it.
				/// [JP] glTF は "weights" チャンネルの出力アクセサを、上の
				///      ベクター型のようにキーフレームごとにネストせず、
				///      keyframeCount * targetCount 個の float の単一フラット
				///      SCALAR 列として詰める(キーフレーム0の全ターゲット、
				///      続いてキーフレーム1の全ターゲット、...)。targetCount
				///      はサンプラー自体には記録されていないため、対象ノード
				///      自身のメッシュ(記述があればそのデフォルトウェイト
				///      Node::weights、無ければ最初のプリミティブのモーフ
				///      ターゲット数)から取得する。
				const tinygltf::Node& targetGltfNode = model.nodes.at(gltfChannel.target_node);
				Size targetCount = targetGltfNode.weights.size();
				if (targetCount == 0 && targetGltfNode.mesh >= 0)
				{
					const tinygltf::Mesh& targetGltfMesh = model.meshes.at(targetGltfNode.mesh);
					if (!targetGltfMesh.primitives.empty())
					{
						targetCount = targetGltfMesh.primitives.front().targets.size();
					}
				}

				if (targetCount > 0 && accessor.count % targetCount == 0)
				{
					Size keyframeCount = accessor.count / targetCount;
					const Float* flatWeights = reinterpret_cast<const Float*>(data);

					DynamicArray<DynamicArray<Float>>& weights = animation.weights_[gltfChannel.target_node];
					weights.resize(keyframeCount);
					for (Size keyframeIndex = 0; keyframeIndex < keyframeCount; ++keyframeIndex)
					{
						weights[keyframeIndex].assign(flatWeights + keyframeIndex * targetCount, flatWeights + (keyframeIndex + 1) * targetCount);
					}
				}
			}
		}

		/// [EN] Some export pipelines keep each clip's keyframe times as
		///      absolute offsets into a shared master timeline instead of
		///      rebasing them to start at 0 (e.g. a clip that was originally
		///      frames 16s-24s on a combined timeline keeps those literal
		///      timestamps even once split into its own file). Rebase every
		///      timeline so this clip's own earliest keyframe becomes time 0 —
		///      otherwise SamplePose would just hold the first keyframe's pose
		///      for however many seconds it originally sat at on the shared
		///      timeline before doing anything.
		/// [JP] エクスポートツールによっては、クリップのキーフレーム時刻を
		///      切り出し元の共有タイムライン上の絶対時刻のまま(0秒に
		///      リベースせず)保持する(例: 元が結合タイムライン上の16秒〜24秒
		///      だったクリップは、個別ファイルに分割されてもその生の時刻の
		///      ままになる)。このクリップの最も早いキーフレームが時刻0に
		///      なるよう、全タイムラインをリベースする — そうしないと
		///      SamplePoseは、共有タイムライン上で最初のキーフレームが
		///      本来あった秒数だけ、そのポーズを保持し続けてしまう。
		Float minTime = FLT_MAX;
		for (const auto& times : animation.timelines_ | std::ranges::views::values)
		{
			if (!times.empty())
			{
				minTime = Min(minTime, times.front());
			}
		}

		if (minTime != FLT_MAX && minTime != 0.0f)
		{
			for (auto& times : animation.timelines_ | std::ranges::views::values)
			{
				for (Float& t : times)
				{
					t -= minTime;
				}
			}
		}

		Float duration = 0.0f;
		for (const auto& times : animation.timelines_ | std::ranges::views::values)
		{
			if (!times.empty())
			{
				duration = Max(duration, times.back());
			}
		}
		animation.duration_ = duration;
	}
}
