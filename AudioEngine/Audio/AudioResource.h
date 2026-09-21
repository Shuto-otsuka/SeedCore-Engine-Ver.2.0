#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/Utility/Handle.h>
#include <FoundationEngine/Utility/FlatMap.h>
#include <FoundationEngine/Resource/Asset/Asset.h>

namespace SeedCore
{
	class Sound;
	struct LoaderSystem;
	class ResourceCache;

	/**
	* [EN]
	* Asset resource that maps audio asset IDs to loaded Sound handles and
	* coordinates their lifetime with AudioLoader.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 音声アセット ID と読み込み済み Sound ハンドルを対応付け、その寿命を
	* AudioLoader と連携して管理するアセットリソース。
	*/
	class SEEDCORE_API AudioResource :public Asset, public NonCopyable
	{
	public:
		/**
		* [EN]
		* Creates an empty audio-resource map.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 空の音声リソースマップを生成する。
		*/
		AudioResource() = default;

		/**
		* [EN]
		* Destroys the audio-resource map.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 音声リソースマップを破棄する。
		*/
		~AudioResource() = default;

		/**
		* [EN]
		* Loads an audio asset through the generic asset context.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 汎用アセットコンテキストを通じて音声アセットを読み込む。
		*/
		void Load(const AssetContext& context, Uint32 assetId)override;

		/**
		* [EN]
		* Unloads an audio asset through the generic asset context.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 汎用アセットコンテキストを通じて音声アセットを解放する。
		*/
		void Unload(const AssetContext& context, Uint32 assetId)override;

		/**
		* [EN]
		* Loads an audio asset if needed and returns its cached Sound handle.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 必要に応じて音声アセットを読み込み、キャッシュ済みの Sound ハンドルを返す。
		*/
		Handle<Sound> Load(LoaderSystem& loader, ResourceCache& cache, Uint32 assetId);

		/**
		* [EN]
		* Returns the loaded Sound handle associated with an asset ID.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* アセット ID に対応する読み込み済み Sound ハンドルを返す。
		*/
		Handle<Sound> GetHandle(Uint32 assetId)const;

		/**
		* [EN]
		* Resolves a Sound handle through the audio loader.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* オーディオローダーを通じて Sound ハンドルを解決する。
		*/
		Sound* Resolve(LoaderSystem& loader, const Handle<Sound>& handle);

		/**
		* [EN]
		* Reports whether an asset ID has a loaded Sound handle.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* アセット ID に読み込み済み Sound ハンドルがあるかを返す。
		*/
		Bool Contains(Uint32 assetId)const;

		/**
		* [EN]
		* Releases and removes the Sound associated with an asset ID.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* アセット ID に対応する Sound を解放し、対応関係を削除する。
		*/
		void Unload(LoaderSystem& loader, Uint32 assetId);

	private:
		/// [EN] Loaded Sound handle indexed by audio asset ID.
		/// [JP] 音声アセット ID をキーとする読み込み済み Sound ハンドル。
		FlatMap<Uint32, Handle<Sound>> assetHandleMap_;
	};
}
