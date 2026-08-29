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
	class D3D12CommandQueue;
	enum class MeshCollisionDetail;

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
		Handle<Crister> Load(LoaderSystem& loader, ID3D12Device* device, D3D12CommandQueue* cmdQueue, BindlessHeap* heap, BC7CompressShader& bc7Shader, ResourceCache& cache, Uint32 assetId);

		/**
		* [EN]
		* Bakes a ".collision" file for the model asset assetId at the given
		* detail into a "<model stem>.Collisions/" sibling folder, overwriting
		* any existing one, and returns whether it succeeded. The model is
		* loaded first if it is not already resident. Proxy writes
		* "proxy.collision", Exact "exact.collision" -- each becomes its own
		* AssetType::MeshCollision asset on the next ResourceCache scan.
		* Invoked from the content drawer's "コリジョン生成" asset action;
		* models no longer auto-derive collision on load.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* モデルアセット assetId に対して、指定した detail の ".collision"
		* ファイルを "<モデル名>.Collisions/" 兄弟フォルダへベイクし（既存が
		* あれば上書き）、成否を返す。モデルが未常駐なら先にロードする。
		* Proxy は "proxy.collision"、Exact は "exact.collision" を書き出し、
		* それぞれ次回の ResourceCache スキャンで個別の
		* AssetType::MeshCollision アセットになる。コンテンツドロワーの
		* 「コリジョン生成」アセットアクションから呼ばれる。モデルは
		* ロード時に衝突を自動生成しなくなった。
		*/
		Bool GenerateCollision(LoaderSystem& loader, ID3D12Device* device, D3D12CommandQueue* cmdQueue, BindlessHeap* heap, BC7CompressShader& bc7Shader, ResourceCache& cache, Uint32 assetId, MeshCollisionDetail detail);

		/**
		* [EN]
		* Writes one ".material" sibling file per Crister material slot into
		* "<model dir>/<model stem>.Materials/", named after each Material's
		* name_. The model is loaded first if not already resident. When
		* overwrite is false, slots whose file already exists are left alone
		* (import-time extraction); when true every file is rewritten from the
		* current embedded material (the content drawer's "マテリアル抽出"
		* asset action). The written files become AssetType::Material assets
		* on the next ResourceCache scan. Returns false when the model has no
		* materials or a write failed.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* Crister のマテリアルスロット1つにつき1つの ".material" 兄弟ファイルを
		* "<モデルのディレクトリ>/<モデル名>.Materials/" へ、各 Material の
		* name_ を名前にして書き出す。モデルが未常駐なら先にロードする。
		* overwrite が false のとき、既にファイルが存在するスロットはそのまま
		* にする（インポート時抽出）。true のときは現在の内蔵マテリアルから
		* 全ファイルを書き直す（コンテンツドロワーの「マテリアル抽出」
		* アセットアクション）。書き出したファイルは次回の ResourceCache
		* スキャンで AssetType::Material アセットになる。モデルにマテリアルが
		* 無い、または書き込みに失敗した場合は false を返す。
		*/
		Bool GenerateMaterial(LoaderSystem& loader, ID3D12Device* device, D3D12CommandQueue* cmdQueue, BindlessHeap* heap, BC7CompressShader& bc7Shader, ResourceCache& cache, Uint32 assetId, Bool overwrite);

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
