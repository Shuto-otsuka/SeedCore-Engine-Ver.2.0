#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/Utility/Handle.h>
#include <FoundationEngine/Pool/StablePool.h>
#include <GraphicsEngine/Model/Skeleton/Skeleton.h>

namespace SeedCore
{
	struct LoaderSystem;

	/**
	* [EN]
	* Loads and saves standalone ".skeleton" assets - one serialized
	* SkeletonRig per file (source model id + root bone name + sockets).
	* Mirrors MaterialLoader: a pool of SkeletonRig, binary (de)serialization
	* through BinaryArchive, no GPU work. The rig holds only human-authored
	* data - the bone hierarchy and reference pose stay on the Crister.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 単体 ".skeleton" アセット（SkeletonRig 1 個 = ソースモデル ID +
	* ルートボーン名 + ソケット）の読み書きを行う。MaterialLoader と同型:
	* SkeletonRig のプール、BinaryArchive でのバイナリ直列化、GPU 処理なし。
	* リグが持つのは人が編集したデータだけ - ボーン階層と参照ポーズは
	* Crister に残る。
	*/
	class SEEDCORE_API SkeletonLoader :public NonCopyable
	{
	public:
		SkeletonLoader() = default;
		~SkeletonLoader() = default;

		Handle<SkeletonRig> Load(LoaderSystem& loader, String filePath);

		/**
		* [EN]
		* Fills a fresh rig for a newly-imported model: links it to
		* sourceModelID and leaves the root bone / sockets empty for the
		* user to author. Used by ModelResource's import-time extraction.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 新規インポートされたモデル用に空のリグを初期化する: sourceModelID
		* へ紐づけ、ルートボーン / ソケットはユーザーが編集するよう空のまま
		* にする。ModelResource のインポート時抽出が使う。
		*/
		void Populate(SkeletonRig& rig, Uint32 sourceModelID);

		SkeletonRig* Get(const Handle<SkeletonRig>& handle);

		void Clear(Handle<SkeletonRig>& handle)noexcept;

		/**
		* [EN]
		* Serializes rig to filePath as a ".skeleton" binary file
		* (overwriting any existing one). Returns false when the write failed.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* rig を ".skeleton" バイナリファイルとして filePath へ直列化
		* する（既存があれば上書き）。書き込みに失敗した場合は false を返す。
		*/
		Bool Save(const SkeletonRig& rig, String filePath);

	private:
		StablePool<SkeletonRig> pool_;
	};
}
