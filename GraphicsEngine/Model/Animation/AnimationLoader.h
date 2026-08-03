#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/Utility/Handle.h>
#include <FoundationEngine/Pool/StablePool.h>
#include <GraphicsEngine/Model/Animation/Animation.h>

namespace SeedCore
{
	struct LoaderSystem;

	class SEEDCORE_API AnimationLoader :public NonCopyable
	{
	public:
		AnimationLoader() = default;
		~AnimationLoader() = default;

		Handle<Animation> Load(LoaderSystem& loader, String filePath);

		/// [EN] Writes animation back out as a ".animation" cereal binary file
		///      at filePath (overwrite: same path as it was loaded from; save
		///      as: any other path). Used by TimelinePanel to persist editor-
		///      authored easing keyframes / notify events.
		/// [JP] animationを".animation"のcereal binaryファイルとしてfilePathへ
		///      書き出す(上書き保存: 読み込み元と同じパス、名前を付けて保存:
		///      別パス)。TimelinePanelがエディタで作成したイージングキーフレーム
		///      /通知イベントを永続化するのに使う。
		Bool Save(Animation& animation, const std::filesystem::path& filePath);

		Animation* Get(const Handle<Animation>& handle);

		void Clear(Handle<Animation>& handle)noexcept;

		/// [EN] Parses a glTF/GLB and writes one "<clip name>.animation" cache
		///      file per animation clip it contains, next to the source file.
		///      No-op if the source has no animations. Doesn't touch the pool
		///      or return a handle — the written files become their own
		///      AssetType::Animation assets on the next scan, loaded through
		///      Load() like any other ".animation" asset.
		/// [JP] glTF/GLBを解析し、含まれるアニメーションクリップごとに
		///      "<クリップ名>.animation" キャッシュをソースファイルの隣に
		///      書き出す。アニメーションが無ければ何もしない。プールにも
		///      触れずハンドルも返さない — 書き出したファイルは次回スキャンで
		///      個別の AssetType::Animation アセットになり、他の ".animation"
		///      アセットと同じく Load() で読み込まれる。
		void SplitClips(String filePath);

	private:
		void FetchAnimationClip(const tinygltf::Model& model, const tinygltf::Animation& gltfAnimation, Animation& animation);

	private:
		StablePool<Animation> pool_;
	};
}
