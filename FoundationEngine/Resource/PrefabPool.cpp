#include <FoundationEngine/Resource/PrefabPool.h>
#include <FoundationEngine/Resource/ResourceCache.h>
#include <FoundationEngine/Log/Error.h>

namespace SeedCore
{
	/**
	* [EN]
	* Returns the handle for assetID's Prefab, loading and parsing it
	* from disk on first request; returns a null handle if the asset is
	* unknown or fails to parse.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* assetID の Prefab へのハンドルを返す。初回リクエスト時にはディスクから
	* 読み込み・解析する。アセットが不明であるか解析に失敗した場合は
	* null ハンドルを返す。
	*/
	Handle<Prefab> PrefabPool::Load(Uint32 assetID, ResourceCache& cache)
	{
		auto it = loaded_.find(assetID);
		if (it != loaded_.end())
		{
			return it->second;
		}

		Asset* asset = cache.GetAsset(assetID);
		if (!asset)
		{
			return Handle<Prefab>::null();
		}

		Handle<Prefab> handle = pool_.Create();
		Prefab* prefab = pool_.Get(handle);
		if (!prefab)
		{
			return Handle<Prefab>::null();
		}

		/// [EN] Parsing can throw (malformed JSON); treat any failure as "load failed" rather than propagating the exception.
		/// [JP] 解析中に例外が投げられる可能性がある（不正な JSON）。例外を伝播させるのではなく、あらゆる失敗を「読み込み失敗」として扱う。
		try
		{
			if (!prefab->Read(asset->fullpath_.c_str()))
			{
				SC_LOG_ERROR("Prefabファイルを開けませんでした: {}", asset->fullpath_.str());
				return Handle<Prefab>::null();
			}
		}
		catch (...)
		{
			SC_LOG_ERROR("Prefabファイルの読み込みに失敗しました: {}", asset->fullpath_.str());
			return Handle<Prefab>::null();
		}

		loaded_[assetID] = handle;
		return handle;
	}

	/**
	* [EN]
	* Returns a pointer to the Prefab identified by handle, or nullptr
	* if handle is invalid/stale.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* handle が識別する Prefab へのポインタを返す。handle が無効/失効
	* していれば nullptr を返す。
	*/
	Prefab* PrefabPool::Get(Handle<Prefab> handle)
	{
		return pool_.Get(handle);
	}
}
