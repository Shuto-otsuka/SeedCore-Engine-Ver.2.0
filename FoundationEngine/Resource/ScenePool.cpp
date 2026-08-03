#include <FoundationEngine/Resource/ScenePool.h>
#include <FoundationEngine/Resource/ResourceCache.h>
#include <FoundationEngine/Log/Error.h>

namespace SeedCore
{
	/**
	* [EN]
	* Returns the handle for assetID's Scene, loading and parsing it
	* from disk on first request; returns a null handle if the asset is
	* unknown or fails to parse.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* assetID の Scene へのハンドルを返す。初回リクエスト時にはディスクから
	* 読み込み・解析する。アセットが不明であるか解析に失敗した場合は
	* null ハンドルを返す。
	*/
	Handle<Scene> ScenePool::Load(Uint32 assetID, ResourceCache& cache)
	{
		auto it = loaded_.find(assetID);
		if (it != loaded_.end())
		{
			return it->second;
		}

		Asset* asset = cache.GetAsset(assetID);
		if (!asset)
		{
			return Handle<Scene>::null();
		}

		Handle<Scene> handle = pool_.Create();
		Scene* scene = pool_.Get(handle);
		if (!scene)
		{
			return Handle<Scene>::null();
		}

		/// [EN] Parsing can throw (malformed file); treat any failure as "load failed" rather than propagating the exception.
		/// [JP] 解析中に例外が投げられる可能性がある（不正な形式のファイル）。例外を伝播させるのではなく、あらゆる失敗を「読み込み失敗」として扱う。
		try
		{
			if (!scene->Read(asset->fullpath_.c_str()))
			{
				SC_LOG_ERROR("Sceneファイルを開けませんでした: {}", asset->fullpath_.str());
				return Handle<Scene>::null();
			}
		}
		catch (...)
		{
			SC_LOG_ERROR("Sceneファイルの読み込みに失敗しました: {}", asset->fullpath_.str());
			return Handle<Scene>::null();
		}

		loaded_[assetID] = handle;
		return handle;
	}

	/**
	* [EN]
	* Returns a pointer to the Scene identified by handle, or nullptr if
	* handle is invalid/stale.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* handle が識別する Scene へのポインタを返す。handle が無効/失効
	* していれば nullptr を返す。
	*/
	Scene* ScenePool::Get(Handle<Scene> handle)
	{
		return pool_.Get(handle);
	}

	/**
	* [EN]
	* Returns the pool's single scratch Scene, for temporary use outside
	* the deduplicated pool.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 重複排除されたプールの外で一時的に使用するための、プール専用の
	* 単一の作業用 Scene を返す。
	*/
	Scene& ScenePool::GetScratch()
	{
		return scratch_;
	}
}
