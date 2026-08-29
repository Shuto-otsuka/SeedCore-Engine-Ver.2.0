#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/Utility/Handle.h>
#include <FoundationEngine/Pool/StablePool.h>
#include <GraphicsEngine/Model/Crister.h>

namespace SeedCore
{
	struct LoaderSystem;

	/**
	* [EN]
	* Loads and saves standalone ".material" assets - one serialized
	* Surface per file (PBR factors + KHR_materials_* + shading model +
	* alpha/double-sided + glTF texture-image indices). Mirrors
	* MeshCollisionLoader: a pool of Surface, binary (de)serialization
	* through BinaryArchive, no GPU work.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 単体 ".material" アセット（Surface 1 個 = PBR ファクタ +
	* KHR_materials_* + シェーディングモデル + アルファ/両面 + glTF の
	* テクスチャ image インデックス）の読み書きを行う。
	* MeshCollisionLoader と同型: Surface のプール、BinaryArchive での
	* バイナリ直列化、GPU 処理なし。
	*/
	class SEEDCORE_API MaterialLoader :public NonCopyable
	{
	public:
		MaterialLoader() = default;
		~MaterialLoader() = default;

		Handle<Surface> Load(LoaderSystem& loader, String filePath);

		Surface* Get(const Handle<Surface>& handle);

		void Clear(Handle<Surface>& handle)noexcept;

		/**
		* [EN]
		* Serializes surface to filePath as a ".material" binary file
		* (overwriting any existing one). Returns false when the write failed.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* surface を ".material" バイナリファイルとして filePath へ直列化
		* する（既存があれば上書き）。書き込みに失敗した場合は false を返す。
		*/
		Bool Save(const Surface& surface, String filePath);

	private:
		StablePool<Surface> pool_;
	};
}
