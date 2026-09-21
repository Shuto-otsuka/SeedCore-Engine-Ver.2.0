#include <AudioEngine/Audio/AudioResource.h>
#include <AudioEngine/Audio/AudioLoader.h>
#include <FoundationEngine/Resource/ResourceCache.h>
#include <FoundationEngine/Resource/LoaderSystem.h>

namespace SeedCore
{
	/**
	* [EN]
	* Loads an audio asset through the generic asset context.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 汎用アセットコンテキストを通じて音声アセットを読み込む。
	*/
	void AudioResource::Load(const AssetContext& context, Uint32 assetId)
	{
		Load(context.loader_, context.cache_, assetId);
	}

	/**
	* [EN]
	* Unloads an audio asset through the generic asset context.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 汎用アセットコンテキストを通じて音声アセットを解放する。
	*/
	void AudioResource::Unload(const AssetContext& context, Uint32 assetId)
	{
		Unload(context.loader_, assetId);
	}

	/**
	* [EN]
	* Loads an audio asset if needed and returns its cached Sound handle.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 必要に応じて音声アセットを読み込み、キャッシュ済みの Sound ハンドルを返す。
	*/
	Handle<Sound> AudioResource::Load(LoaderSystem& loader, ResourceCache& cache, Uint32 assetId)
	{
		/// [EN] Reuse the existing handle when this asset is already loaded.
		/// [JP] このアセットが読み込み済みなら、既存のハンドルを再利用する。
		if (assetHandleMap_.contains(assetId))
		{
			return assetHandleMap_.at(assetId);
		}

		/// [EN] Resolve the asset path and load its Sound into AudioLoader storage.
		/// [JP] アセットのパスを解決し、Sound を AudioLoader のストレージへ読み込む。
		AssetRecord* asset = cache.GetAsset(assetId);
		if (!asset)
		{
			return Handle<Sound>::null();
		}

		Handle<Sound> handle = loader.audioLoader_->Load(loader, asset->fullpath_);
		if (handle.empty())
		{
			return Handle<Sound>::null();
		}

		assetHandleMap_.insert({ assetId, handle });
		return handle;
	}

	/**
	* [EN]
	* Returns the loaded Sound handle associated with an asset ID.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* アセット ID に対応する読み込み済み Sound ハンドルを返す。
	*/
	Handle<Sound> AudioResource::GetHandle(Uint32 assetId)const
	{
		if (!assetHandleMap_.contains(assetId))
		{
			return Handle<Sound>::null();
		}

		return assetHandleMap_.at(assetId);
	}

	/**
	* [EN]
	* Resolves a Sound handle through the audio loader.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* オーディオローダーを通じて Sound ハンドルを解決する。
	*/
	Sound* AudioResource::Resolve(LoaderSystem& loader, const Handle<Sound>& handle)
	{
		return loader.audioLoader_->Get(handle);
	}

	/**
	* [EN]
	* Reports whether an asset ID has a loaded Sound handle.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* アセット ID に読み込み済み Sound ハンドルがあるかを返す。
	*/
	Bool AudioResource::Contains(Uint32 assetId)const
	{
		return assetHandleMap_.contains(assetId);
	}

	/**
	* [EN]
	* Releases and removes the Sound associated with an asset ID.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* アセット ID に対応する Sound を解放し、対応関係を削除する。
	*/
	void AudioResource::Unload(LoaderSystem& loader, Uint32 assetId)
	{
		if (!assetHandleMap_.contains(assetId))
		{
			return;
		}

		/// [EN] Release the pooled Sound before removing its asset-to-handle mapping.
		/// [JP] アセットとハンドルの対応を削除する前に、プール内の Sound を解放する。
		Handle<Sound> handle = assetHandleMap_.at(assetId);
		loader.audioLoader_->Clear(handle);
		assetHandleMap_.erase(assetId);
	}

	REGISTER_ASSET(AssetType::Audio, AudioResource);
}
