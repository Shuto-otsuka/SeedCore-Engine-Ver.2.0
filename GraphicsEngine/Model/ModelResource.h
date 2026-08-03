#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/Utility/Handle.h>
#include <FoundationEngine/Utility/FlatMap.h>

namespace SeedCore
{
	class Crister;
	struct LoaderSystem;
	class ResourceCache;
	class BindlessHeap;
	class BC7CompressShader;

	/**
	* [EN]
	* Manages the mapping between asset IDs and loaded Crister handles.
	* Mirrors the ImageResource pattern: Load by asset ID, retrieve
	* handles, resolve to Crister pointers, and unload when no longer needed.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* アセット ID とロード済み Crister ハンドル間のマッピングを管理する。
	* ImageResource パターンと同じ: アセット ID でロード、ハンドル取得、
	* Crister ポインタへの解決、不要時のアンロード。
	*/
	class SEEDCORE_API ModelResource :public NonCopyable
	{
	public:
		ModelResource() = default;
		~ModelResource() = default;

		/**
		* [EN]
		* Loads a model by asset ID. If already loaded, returns the existing handle.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* アセット ID でモデルをロードする。既にロード済みなら既存のハンドルを返す。
		*/
		Handle<Crister> Load(LoaderSystem& loader, ID3D12Device* device, ID3D12CommandQueue* cmdQueue, BindlessHeap* heap, BC7CompressShader& bc7Shader, ResourceCache& cache, Uint32 assetId);

		/**
		* [EN]
		* Returns the handle associated with an asset ID, or null if not loaded.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* アセット ID に関連付けられたハンドルを返す。未ロードなら null。
		*/
		Handle<Crister> GetHandle(Uint32 assetId)const;

		/**
		* [EN]
		* Resolves a handle to a Crister pointer.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* ハンドルを Crister ポインタに解決する。
		*/
		Crister* Resolve(LoaderSystem& loader, const Handle<Crister>& handle);

		/**
		* [EN]
		* Returns true if a model with the given asset ID is loaded.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 指定されたアセット ID のモデルがロード済みなら true を返す。
		*/
		Bool Contains(Uint32 assetId)const;

		/**
		* [EN]
		* Unloads a model by asset ID and removes it from the map.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* アセット ID でモデルをアンロードし、マップから削除する。
		*/
		void Unload(LoaderSystem& loader, Uint32 assetId, BindlessHeap* heap);

	private:
		FlatMap<Uint32, Handle<Crister>> assetHandleMap_;
	};
}
