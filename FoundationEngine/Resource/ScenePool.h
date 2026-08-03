#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/Resource/Scene.h>
#include <FoundationEngine/Pool/StablePool.h>
#include <FoundationEngine/Utility/FlatMap.h>

namespace SeedCore
{
	class ResourceCache;

	/**
	* [EN]
	* Owns every currently-loaded Scene, deduplicated by asset ID so
	* requesting the same scene twice returns the same in-memory
	* instance. Also holds a single scratch Scene for callers that need
	* a temporary, pool-independent Scene (e.g. to read/build a scene
	* without registering it as a loaded asset).
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 現在読み込まれている全 Scene を所有する。アセット ID で重複排除
	* されるため、同じシーンを2回要求すると同じメモリ上のインスタンスが
	* 返される。また、一時的でプールに依存しない Scene を必要とする
	* 呼び出し側向けに（読み込み済みアセットとして登録せずにシーンを
	* 読み書き/構築するためなど）、単一の作業用 Scene も保持する。
	*/
	class ScenePool
	{
	public:
		/**
		* [EN]
		* Returns the handle for assetID's Scene, loading and parsing it
		* from disk on first request; returns a null handle if the asset
		* is unknown or fails to parse.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* assetID の Scene へのハンドルを返す。初回リクエスト時にはディスクから
		* 読み込み・解析する。アセットが不明であるか解析に失敗した場合は
		* null ハンドルを返す。
		*/
		Handle<Scene> Load(Uint32 assetID, ResourceCache& cache);

		/**
		* [EN]
		* Returns a pointer to the Scene identified by handle, or nullptr
		* if handle is invalid/stale.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* handle が識別する Scene へのポインタを返す。handle が無効/
		* 失効していれば nullptr を返す。
		*/
		Scene* Get(Handle<Scene> handle);

		/**
		* [EN]
		* Returns the pool's single scratch Scene, for temporary use
		* outside the deduplicated pool.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 重複排除されたプールの外で一時的に使用するための、プール専用の
		* 単一の作業用 Scene を返す。
		*/
		Scene& GetScratch();

	private:
		/// [EN] Stable-pooled storage of every loaded Scene, providing generation-checked handles.
		/// [JP] 読み込み済みの全 Scene の安定プール格納。世代チェック付きのハンドルを提供する。
		StablePool<Scene> pool_;

		/// [EN] Maps an asset ID to its already-loaded Scene's handle, for deduplication.
		/// [JP] アセット ID を、既に読み込み済みの Scene のハンドルへ対応付ける。重複排除のために使う。
		FlatMap<Uint32, Handle<Scene>> loaded_;

		/// [EN] Single reusable Scene instance for temporary, pool-independent use (see GetScratch).
		/// [JP] 一時的でプールに依存しない用途のための、再利用可能な単一の Scene インスタンス（GetScratch を参照）。
		Scene scratch_;
	};
}
