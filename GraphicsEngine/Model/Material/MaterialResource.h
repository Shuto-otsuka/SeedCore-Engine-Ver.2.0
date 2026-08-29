#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/Utility/Handle.h>
#include <FoundationEngine/Utility/FlatMap.h>
#include <GraphicsEngine/Model/Crister.h>

namespace SeedCore
{
	struct LoaderSystem;
	class ResourceCache;

	/**
	* [EN]
	* Manages the mapping between asset IDs and loaded ".material" handles.
	* Mirrors MeshCollisionResource: Load by asset ID, retrieve handles,
	* resolve to Surface pointers, unload when no longer needed. Also owns
	* the per-draw resolution helper (Resolve) that picks between a Material
	* component's slot assignment and the Crister's embedded seed Surface.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* アセット ID とロード済み ".material" ハンドル間のマッピングを管理する。
	* MeshCollisionResource と同型: アセット ID でロード、ハンドル取得、
	* Surface ポインタへの解決、不要時のアンロード。描画ごとの解決ヘルパー
	* (Resolve) も持ち、Material コンポーネントのスロット割り当てと Crister
	* 内蔵のシード Surface のどちらを使うかを選ぶ。
	*/
	class SEEDCORE_API MaterialResource :public NonCopyable
	{
	public:
		MaterialResource() = default;
		~MaterialResource() = default;

		Handle<Surface> Load(LoaderSystem& loader, ResourceCache& cache, Uint32 assetId);

		Handle<Surface> GetHandle(Uint32 assetId)const;

		Surface* Resolve(LoaderSystem& loader, const Handle<Surface>& handle);

		Bool Contains(Uint32 assetId)const;

		void Unload(LoaderSystem& loader, Uint32 assetId);

		/**
		* [EN]
		* Returns the effective Surface for one SubMesh: an assigned ".material"
		* is the whole truth for the slot (textures included); a slot with
		* nothing assigned falls back to the Crister's embedded seed Surface.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 1 SubMesh の実効 Surface を返す: 割り当て済みの ".material" は
		* そのスロットの全て（テクスチャ含む）。未割り当てのスロットは
		* Crister 内蔵のシード Surface へフォールバックする。
		*/
		Surface Resolve(LoaderSystem& loader, const Crister& crister, Uint32 slot, const DynamicArray<Uint32>& materialIDs);

	private:
		FlatMap<Uint32, Handle<Surface>> assetHandleMap_;
	};
}
