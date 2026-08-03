#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/Resource/Prefab.h>
#include <FoundationEngine/Pool/StablePool.h>
#include <FoundationEngine/Utility/FlatMap.h>

namespace SeedCore
{
	class ResourceCache;

	/**
	* [EN]
	* Owns every currently-loaded Prefab, deduplicated by asset ID so
	* requesting the same prefab twice returns the same in-memory instance.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 現在読み込まれている全 Prefab を所有する。アセット ID で重複排除
	* されるため、同じプレハブを2回要求すると同じメモリ上のインスタンスが
	* 返される。
	*/
	class SEEDCORE_API PrefabPool
	{
	public:
		/**
		* [EN]
		* Returns the handle for assetID's Prefab, loading and parsing it
		* from disk on first request; returns a null handle if the asset
		* is unknown or fails to parse.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* assetID の Prefab へのハンドルを返す。初回リクエスト時にはディスクから
		* 読み込み・解析する。アセットが不明であるか解析に失敗した場合は
		* null ハンドルを返す。
		*/
		Handle<Prefab> Load(Uint32 assetID, ResourceCache& cache);

		/**
		* [EN]
		* Returns a pointer to the Prefab identified by handle, or
		* nullptr if handle is invalid/stale.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* handle が識別する Prefab へのポインタを返す。handle が無効/
		* 失効していれば nullptr を返す。
		*/
		Prefab* Get(Handle<Prefab> handle);

	private:
		/// [EN] Stable-pooled storage of every loaded Prefab, providing generation-checked handles.
		/// [JP] 読み込み済みの全 Prefab の安定プール格納。世代チェック付きのハンドルを提供する。
		StablePool<Prefab> pool_;

		/// [EN] Maps an asset ID to its already-loaded Prefab's handle, for deduplication.
		/// [JP] アセット ID を、既に読み込み済みの Prefab のハンドルへ対応付ける。重複排除のために使う。
		FlatMap<Uint32, Handle<Prefab>> loaded_;
	};
}
