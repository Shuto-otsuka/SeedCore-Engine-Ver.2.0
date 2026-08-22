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
		///      file at filePath, next to the source model — same
		///      shape as AnimationLoader::SplitClips. Doesn't touch the pool
		///      or return a handle; the written file becomes its own
		///      AssetType::MeshCollision asset on the next scan, loaded
		///      through Load() like any other ".collision" asset. Called
		///      once per detail level from ModelResource::Load, so a single
		///      Model ends up with both a Proxy and a Full ".collision"
		///      sibling for MeshCollider::meshID_ to pick between.
		/// [JP] crister.BakeCollision で指定した detail のジオメトリを計算し、
		///      ソースモデルの隣に ".collision" のバイナリファイルとして
		///      書き出す — AnimationLoader::SplitClips と同じ形。プールにも
		///      触れずハンドルも返さない — 書き出したファイルは次回スキャンで
		///      個別の AssetType::MeshCollision アセットになり、他の
		///      ".collision" アセットと同じく Load() で読み込まれる。
		///      ModelResource::Load から detail ごとに1回ずつ呼ばれるため、
		///      1つの Model につき Proxy と Full 両方の ".collision" が
		///      隣に揃い、MeshCollider::meshID_ はどちらか選んで参照できる。
		void Bake(const Crister& crister, MeshCollisionDetail detail, String filePath);

	private:
		StablePool<MeshCollision> pool_;
	};
}
