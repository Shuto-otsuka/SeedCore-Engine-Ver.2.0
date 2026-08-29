#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/Utility/Handle.h>
#include <FoundationEngine/Pool/StablePool.h>
#include <GraphicsEngine/Model/Collision/MeshCollision.h>

namespace SeedCore
{
	struct LoaderSystem;
	class Crister;
	enum class MeshCollisionDetail;

	class SEEDCORE_API MeshCollisionLoader :public NonCopyable
	{
	public:
		MeshCollisionLoader() = default;
		~MeshCollisionLoader() = default;

		Handle<MeshCollision> Load(LoaderSystem& loader, String filePath);

		MeshCollision* Get(const Handle<MeshCollision>& handle);

		void Clear(Handle<MeshCollision>& handle)noexcept;

		/// [EN] Calls crister.BakeCollision to compute the geometry at the
		///      given detail, then writes it out as a ".collision" binary
		///      file at filePath (overwriting any existing one). Doesn't
		///      touch the pool or return a handle; the written file becomes
		///      its own AssetType::MeshCollision asset on the next scan,
		///      loaded through Load() like any other ".collision" asset. The
		///      caller (ModelResource::GenerateCollision) targets a
		///      "<model stem>.Collisions/" sibling folder.
		///      Driven by ModelResource::GenerateCollision, which is invoked
		///      from the content drawer's "コリジョン生成" asset action
		///      (models do not auto-derive collision on load). Returns false
		///      if the bake or the file write failed.
		/// [JP] crister.BakeCollision で指定した detail のジオメトリを計算し、
		///      ".collision" のバイナリファイルとして filePath へ書き出す
		///      （既存があれば上書き）。プールにも触れずハンドルも返さない
		///      — 書き出したファイルは次回スキャンで個別の
		///      AssetType::MeshCollision アセットになり、他の ".collision"
		///      アセットと同じく Load() で読み込まれる。ModelResource::
		///      GenerateCollision から駆動され、そちらはコンテンツドロワーの
		///      「コリジョン生成」アセットアクションから呼ばれる（モデルは
		///      ロード時に衝突を自動生成しない）。ベイクまたはファイル書き込みに
		///      失敗した場合は false を返す。
		Bool Bake(const Crister& crister, MeshCollisionDetail detail, String filePath);

	private:
		StablePool<MeshCollision> pool_;
	};
}
