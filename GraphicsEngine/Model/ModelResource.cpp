#include <GraphicsEngine/Model/ModelResource.h>
#include <GraphicsEngine/Model/ModelLoader.h>
#include <GraphicsEngine/D3D12/Descriptor/BindlessHeap.h>
#include <FoundationEngine/Resource/ResourceCache.h>
#include <FoundationEngine/Resource/LoaderSystem.h>

namespace SeedCore
{
	Handle<Crister> ModelResource::Load(LoaderSystem& loader, ID3D12Device* device, D3D12CommandQueue* cmdQueue, BindlessHeap* heap, BC7CompressShader& bc7Shader, ResourceCache& cache, Uint32 assetId)
	{
		if (assetHandleMap_.contains(assetId))
		{
			return assetHandleMap_.at(assetId);
		}

		Asset* asset = cache.GetAsset(assetId);
		if (!asset)
		{
			return Handle<Crister>::null();
		}

		AxisConvention axisConvention = cache.ReadAxisConvention(assetId);
		Handle<Crister> handle = loader.modelLoader_->Load(loader, device, cmdQueue, heap, bc7Shader, asset->fullpath_, axisConvention);
		if (handle.empty())
		{
			return Handle<Crister>::null();
		}

		/// [EN] Auto-derive collision geometry the same way SplitClips
		///      auto-derives animation clips: write both a Proxy and a Full
		///      ".collision" cache next to the source model, each becoming
		///      its own AssetType::MeshCollision asset on the next scan.
		///      The Proxy sibling keeps the model's plain name (it's the
		///      common case); Full gets a "_full" suffix. Unlike
		///      ModelLoader's ".crister" cache, this does NOT re-derive from
		///      source every load: ".collision" is derived from data already
		///      resident in the just-loaded Crister (not an external file
		///      that can change out from under it), so once a sibling exists
		///      on disk it is trusted and left alone. Delete it manually to
		///      force a rebake.
		/// [JP] SplitClips がアニメーションクリップを自動導出するのと同じ
		///      仕組みで、衝突ジオメトリも自動導出する: ソースモデルの隣に
		///      Proxy と Full 両方の ".collision" キャッシュを書き出し、
		///      それぞれ次回スキャンで AssetType::MeshCollision アセットになる。
		///      Proxy 側はモデルと同じ素の名前（よく使う方のため）、Full 側は
		///      "_full" サフィックスを付ける。ModelLoader の ".crister"
		///      キャッシュと違い、毎回ソースから再導出はしない — ".collision"
		///      は既にロード済みの Crister（外部から書き換わりうるファイルでは
		///      ない）から導出するため、隣に既にあればそれを信頼してそのまま
		///      にする。再ベイクしたい場合は手動で削除すること。
		if (Crister* crister = loader.modelLoader_->Get(handle))
		{
			std::filesystem::path proxyPath(asset->fullpath_.c_str());
			proxyPath.replace_extension(".collision");
			if (!std::filesystem::exists(proxyPath))
			{
				loader.meshCollisionLoader_->Bake(*crister, MeshCollisionDetail::Proxy, String(proxyPath.string()));
			}

			std::filesystem::path fullPath(asset->fullpath_.c_str());
			fullPath.replace_extension("");
			fullPath += "_full.collision";
			if (!std::filesystem::exists(fullPath))
			{
				loader.meshCollisionLoader_->Bake(*crister, MeshCollisionDetail::Full, String(fullPath.string()));
			}
		}

		assetHandleMap_.insert({ assetId, handle });
		return handle;
	}

	Handle<Crister> ModelResource::GetHandle(Uint32 assetId)const
	{
		if (!assetHandleMap_.contains(assetId))
		{
			return Handle<Crister>::null();
		}
		return assetHandleMap_.at(assetId);
	}

	Crister* ModelResource::Resolve(LoaderSystem& loader, const Handle<Crister>& handle)
	{
		return loader.modelLoader_->Get(handle);
	}

	Bool ModelResource::Contains(Uint32 assetId)const
	{
		return assetHandleMap_.contains(assetId);
	}

	void ModelResource::Unload(LoaderSystem& loader, Uint32 assetId, BindlessHeap* heap)
	{
		if (!assetHandleMap_.contains(assetId))
		{
			return;
		}

		Handle<Crister> handle = assetHandleMap_.at(assetId);
		loader.modelLoader_->Clear(handle, heap);
		assetHandleMap_.erase(assetId);
	}
}
