#include <FoundationEngine/Resource/ResourceCache.h>
#include <FoundationEngine/Resource/LoaderSystem.h>
#include <FoundationEngine/Serialization/Binary/BinaryArchive.h>
#include <FoundationEngine/JobSystem/JobExecutor.h>
#include <FoundationEngine/JobSystem/JobTaskflow.h>
#include <GraphicsEngine/D3D12/Descriptor/BindlessHeap.h>

#include <GraphicsEngine/Texture/ImageResource.h>
#include <GraphicsEngine/Model/ModelResource.h>
#include <GraphicsEngine/Model/Animation/AnimationResource.h>
#include <GraphicsEngine/Model/Collision/MeshCollisionResource.h>
#include <GraphicsEngine/Font/FontResource.h>
#include <GraphicsEngine/Sky/SkymapResource.h>
#include <GraphicsEngine/Effect/Effekseer/EffekseerResource.h>
#include <GraphicsEngine/Movie/MovieResource.h>

namespace SeedCore
{
	namespace
	{
		/// [EN] Reads an AssetMeta from metaPath. Returns false if the file
		///      couldn't be opened or parsed (see BinaryInputArchive::Read).
		/// [JP] metaPath から AssetMeta を読み取る。ファイルを開けない、
		///      または解析できなければ false を返す（BinaryInputArchive::Read 参照）。
		Bool ReadAssetMeta(const std::filesystem::path& metaPath, AssetMeta& meta)
		{
			BinaryInputArchive archive;
			if (!archive.Read(String(metaPath.string())))
			{
				return false;
			}

			meta.Serialize(archive);
			return true;
		}

		/// [EN] Serialises meta to metaPath.
		/// [JP] meta を metaPath へシリアライズする。
		void WriteAssetMetaFile(const std::filesystem::path& metaPath, AssetMeta meta)
		{
			BinaryOutputArchive archive;
			meta.Serialize(archive);
			archive.Write(String(metaPath.string()));
		}
	}

	/**
	* [EN]
	* Constructs the cache, creating each resource manager and resolving
	* the project root path.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* キャッシュを構築し、各リソースマネージャを生成し、プロジェクト
	* ルートパスを解決する。
	*/
	ResourceCache::ResourceCache(LoaderSystem& loader, ID3D12Device* device, ID3D12CommandQueue* cmdQueue, BindlessHeap* heap) : loader_(loader), heap_(heap)
	{
		/// [EN] The project root is one level above the executable's working directory.
		/// [JP] プロジェクトルートは、実行ファイルの作業ディレクトリの1つ上にある。
		projectRootPath_ = std::filesystem::current_path().parent_path();

		imageResource_ = MakePtr<ImageResource>();
		modelResource_ = MakePtr<ModelResource>();
		animationResource_ = MakePtr<AnimationResource>();
		meshCollisionResource_ = MakePtr<MeshCollisionResource>();
		fontResource_ = MakePtr<FontResource>();
		skymapResource_ = MakePtr<SkymapResource>();
		effekseerResource_ = MakePtr<EffekseerResource>();
		movieResource_ = MakePtr<MovieResource>();
	}

	/**
	* [EN]
	* Destroys the cache, unloading every currently-loaded asset.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* キャッシュを破棄し、現在読み込まれている全アセットを解放する。
	*/
	ResourceCache::~ResourceCache()
	{
		/// [EN] Waits (via ~JobExecutor) for any outstanding StepAsync() job to
		///      finish before touching a single resource manager, so it's
		///      always safe to destroy this cache mid-load.
		/// [JP] 1つでもリソースマネージャに触れる前に、未完了の StepAsync()
		///      ジョブが終わるのを（~JobExecutor 経由で）待つ。これにより、
		///      読み込み中にこのキャッシュを破棄しても常に安全になる。
		loadExecutor_ = nullptr;

		Unload(loader_, heap_);
	}

	/**
	* [EN]
	* Starts (or restarts) an incremental scan/load pass: rescans the
	* project directory tree, builds the pending-load queue ordered by
	* AssetType, and resets progress counters.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 逐次的なスキャン/読み込みパスを開始（または再開始）する:
	* プロジェクトディレクトリツリーを再スキャンし、AssetType 順に並べた
	* 保留読み込みキューを構築し、進捗カウンタをリセットする。
	*/
	void ResourceCache::Async()
	{
		scanComplete_.store(false, std::memory_order_release);
		pendingAssetIDs_.clear();
		pendingIndex_ = 0;
		totalAssetCount_.store(0, std::memory_order_release);
		loadedAssetCount_.store(0, std::memory_order_release);
		loadStarted_.store(false, std::memory_order_release);

		Scan(String(projectRootPath_.c_str()));

		/// [EN] Load order matters: textures/models/animations before the things that reference them (e.g. effects/audio after visuals), scenes/prefabs last since they may reference everything else.
		/// [JP] 読み込み順序が重要: テクスチャ/モデル/アニメーションを、それらを参照するもの（ビジュアルの後にエフェクト/オーディオなど）より先に読み込む。シーン/プレハブは他の全てを参照し得るため最後にする。
		static const AssetType order[] =
		{
			AssetType::Texture,
			AssetType::Model,
			AssetType::Animation,
			AssetType::MeshCollision,
			AssetType::Font,
			AssetType::Effect,
			AssetType::Audio,
			AssetType::Movie,
			AssetType::Prefab,
			AssetType::Scene,
			AssetType::Skymap,
		};

		for (AssetType type : order)
		{
			for (auto& [id, asset] : assetsMap_)
			{
				if (asset.type_ == type && !asset.isLoaded_)
				{
					pendingAssetIDs_.push_back(id);
				}
			}
		}

		totalAssetCount_.store(static_cast<Uint32>(pendingAssetIDs_.size()), std::memory_order_release);
		scanComplete_.store(true, std::memory_order_release);
	}

	/**
	* [EN]
	* Loads exactly one pending asset from the queue built by Async(),
	* if any remain. Returns whether an asset was processed this call
	* (false once the queue is drained or Async hasn't been called).
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* Async() が構築したキューから、残っていればちょうど1つの保留
	* アセットを読み込む。この呼び出しでアセットを処理したかどうかを
	* 返す（キューが尽きた、または Async が呼ばれていない場合は false）。
	*/
	Bool ResourceCache::Step(LoaderSystem& loader, ID3D12Device* device, ID3D12CommandQueue* cmdQueue, BindlessHeap* heap, BC7CompressShader& bc7Shader)
	{
		if (!scanComplete_.load(std::memory_order_acquire))
		{
			return false;
		}

		if (pendingIndex_ >= pendingAssetIDs_.size())
		{
			return false;
		}

		Uint32 assetID = pendingAssetIDs_[pendingIndex_++];

		Asset* asset = GetAsset(assetID);
		if (asset && !asset->isLoaded_)
		{
			/// [EN] Dispatch to the resource manager matching this asset's type; some types (Prefab/Scene) have no per-frame Step loader and are handled elsewhere.
			/// [JP] このアセットの種別に対応するリソースマネージャへディスパッチする。一部の種別（Prefab/Scene）はフレームごとの Step ローダーを持たず、別の場所で処理される。
			switch (asset->type_)
			{
			case AssetType::Texture:
				imageResource_->Load(loader, device, cmdQueue, heap, *this, asset->assetID_);
				break;
			case AssetType::Model:
				modelResource_->Load(loader, device, cmdQueue, heap, bc7Shader, *this, asset->assetID_);
				loader.animationLoader_->SplitClips(asset->fullpath_);
				break;
			case AssetType::Animation:
				animationResource_->Load(loader, *this, asset->assetID_);
				break;
			case AssetType::MeshCollision:
				meshCollisionResource_->Load(loader, *this, asset->assetID_);
				break;
			case AssetType::Effect:
				effekseerResource_->Load(loader, *this, asset->assetID_);
				break;
			case AssetType::Audio:
				break;
			case AssetType::Font:
				fontResource_->Load(asset->assetID_, asset->fullpath_.str(), 48.0f);
				break;
			case AssetType::Movie:
				movieResource_->Load(asset->assetID_, asset->fullpath_.str());
				break;
			case AssetType::Skymap:
				skymapResource_->Load(loader, device, cmdQueue, heap, *this, asset->assetID_);
				break;
			default:
				break;
			}
			asset->isLoaded_ = true;
		}

		loadedAssetCount_.fetch_add(1, std::memory_order_acq_rel);
		return true;
	}

	/**
	* [EN]
	* Starts (once per Async() pass) a background job that repeatedly calls
	* Step() on a dedicated worker thread until the pending queue is
	* drained. See the header doc comment for the full contract.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* バックグラウンドジョブを開始する（Async() パスごとに1回）。専用の
	* ワーカースレッド上でキューが尽きるまで Step() を繰り返し呼ぶ。
	* 完全な契約についてはヘッダのドキュメントコメント参照。
	*/
	void ResourceCache::StepAsync(LoaderSystem& loader, ID3D12Device* device, ID3D12CommandQueue* cmdQueue, BindlessHeap* heap, BC7CompressShader& bc7Shader)
	{
		Bool expected = false;
		if (!loadStarted_.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
		{
			return;
		}

		if (!loadExecutor_)
		{
			loadExecutor_ = MakePtr<JobExecutor>(1);
		}

		/// [EN] loadTaskflow_ is a member (see its header doc comment for why)
		///      - Clear() it first since a rescan (Unload() resetting
		///      loadStarted_) could otherwise re-run this on a taskflow that
		///      still has the previous pass's already-finished task in it.
		/// [JP] loadTaskflow_ はメンバー（理由はヘッダのドキュメントコメント
		///      参照）- 再スキャン(Unload() が loadStarted_ をリセットした場合)
		///      で、前回パスの完了済みタスクが残ったままの taskflow に対して
		///      これを再実行してしまわないよう、先に Clear() する。
		loadTaskflow_.Clear();
		loadTaskflow_.emplace([this, &loader, device, cmdQueue, heap, &bc7Shader]()
		{
			while (Step(loader, device, cmdQueue, heap, bc7Shader))
			{
				/// No Code
			}
		});

		/// [EN] Passed by lvalue reference (not moved) since loadExecutor_'s
		///      JobTopology only stores a reference to this taskflow, which
		///      must stay alive as long as loadTaskflow_ - a member - already does.
		/// [JP] （ムーブではなく）左辺値参照で渡す - loadExecutor_ の
		///      JobTopology はこの taskflow への参照しか持たないため、
		///      メンバーである loadTaskflow_ と同じだけ生存させる必要がある。
		loadExecutor_->Run(loadTaskflow_);
	}

	/**
	* [EN]
	* Returns whether the current Async() pass has scanned and loaded
	* every pending asset.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 現在の Async() パスが、保留中の全アセットをスキャン・読み込み済みか
	* どうかを返す。
	*/
	Bool ResourceCache::Complete()const
	{
		return scanComplete_.load(std::memory_order_acquire) && pendingIndex_ >= pendingAssetIDs_.size();
	}

	/**
	* [EN]
	* Returns the current load progress of the Async() pass, from 0 to 1
	* (1 if there is nothing to load).
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* Async() パスの現在の読み込み進捗を 0 から 1 の範囲で返す
	* （読み込むものが無ければ 1）。
	*/
	Float ResourceCache::Progress()const
	{
		if (!scanComplete_.load(std::memory_order_acquire))
		{
			return 0.0f;
		}

		Uint32 total = totalAssetCount_.load(std::memory_order_acquire);
		if (total == 0)
		{
			return 1.0f;
		}

		return static_cast<Float>(loadedAssetCount_.load(std::memory_order_acquire)) / static_cast<Float>(total);
	}

	/**
	* [EN]
	* Returns the Asset registered under id, or nullptr if unknown.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* id に登録されている Asset を返す。不明であれば nullptr を返す。
	*/
	Asset* ResourceCache::GetAsset(Uint32 id)
	{
		auto it = assetsMap_.find(id);
		if (it != assetsMap_.end())
		{
			return &(it->second);
		}
		return nullptr;
	}

	/**
	* [EN]
	* Returns the asset ID whose path matches filename, or 0 if not found.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* パスが filename に一致するアセットの ID を返す。見つからなければ
	* 0 を返す。
	*/
	Uint32 ResourceCache::GetAssetID(String filename)
	{
		if (searchMap_.contains(filename))
		{
			return searchMap_.at(filename);
		}

		return 0;
	}

	/**
	* [EN]
	* Returns every Asset whose path contains key as a substring.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* パスに key を部分文字列として含む、全ての Asset を返す。
	*/
	DynamicArray<Asset*> ResourceCache::Search(String key)
	{
		DynamicArray<Asset*> result;

		for (auto& asset : assetsMap_ | std::ranges::views::values)
		{
			if (asset.path_.str().find(key.c_str()) != std::string::npos)
			{
				result.push_back(&asset);
			}
		}

		return result;
	}

	/**
	* [EN]
	* Reads the AxisConvention persisted in id's .meta sidecar. Returns a
	* default-constructed AxisConvention (matching the engine's existing
	* fixed RH->LH behavior) if the asset or its .meta file don't exist,
	* or if the .meta predates this field.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* id の .meta サイドカーに永続化されている AxisConvention を読み取る。
	* アセットや .meta ファイルが存在しない場合、またはこのフィールドより
	* 前の .meta の場合は、デフォルト構築の AxisConvention（エンジン既存の
	* 固定 RH->LH 挙動と一致）を返す。
	*/
	AxisConvention ResourceCache::ReadAxisConvention(Uint32 assetId)const
	{
		auto it = assetsMap_.find(assetId);
		if (it == assetsMap_.end())
		{
			return AxisConvention{};
		}

		std::filesystem::path metaPath = std::filesystem::path(it->second.fullpath_.c_str());
		metaPath += ".meta";
		if (!std::filesystem::exists(metaPath))
		{
			return AxisConvention{};
		}

		AssetMeta meta;
		if (!ReadAssetMeta(metaPath, meta))
		{
			return AxisConvention{};
		}

		return meta.axisConvention_;
	}

	/**
	* [EN]
	* Persists convention into id's .meta sidecar, preserving the
	* existing guid_. Creates the .meta file if it doesn't exist yet.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* id の .meta サイドカーへ convention を永続化する。既存の guid_ は
	* 保持する。.meta ファイルがまだ無ければ新規作成する。
	*/
	void ResourceCache::WriteAssetMeta(Uint32 assetId, const AxisConvention& convention)
	{
		auto it = assetsMap_.find(assetId);
		if (it == assetsMap_.end())
		{
			return;
		}

		std::filesystem::path metaPath = std::filesystem::path(it->second.fullpath_.c_str());
		metaPath += ".meta";

		AssetMeta meta;
		meta.guid_ = assetId;

		if (std::filesystem::exists(metaPath))
		{
			if (!ReadAssetMeta(metaPath, meta))
			{
				meta = AssetMeta{};
				meta.guid_ = assetId;
			}
		}

		meta.axisConvention_ = convention;
		WriteAssetMetaFile(metaPath, meta);
	}

	/**
	* [EN]
	* Synchronously rescans the project for asset changes and loads
	* every not-yet-loaded asset in a single pass.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* プロジェクトのアセット変更を同期的に再スキャンし、まだ読み込まれて
	* いない全アセットを一括で読み込む。
	*/
	void ResourceCache::Reload(LoaderSystem& loader, ID3D12Device* device, ID3D12CommandQueue* cmdQueue, BC7CompressShader& bc7Shader)
	{
		Rescan();

		for (auto& asset : assetsMap_ | std::ranges::views::values)
		{
			if (asset.isLoaded_)
			{
				continue;
			}

			switch (asset.type_)
			{
			case AssetType::Texture:
				imageResource_->Load(loader, device, cmdQueue, heap_, *this, asset.assetID_);
				break;
			case AssetType::Model:
				modelResource_->Load(loader, device, cmdQueue, heap_, bc7Shader, *this, asset.assetID_);
				loader.animationLoader_->SplitClips(asset.fullpath_);
				break;
			case AssetType::Animation:
				animationResource_->Load(loader, *this, asset.assetID_);
				break;
			case AssetType::MeshCollision:
				meshCollisionResource_->Load(loader, *this, asset.assetID_);
				break;
			case AssetType::Effect:
				effekseerResource_->Load(loader, *this, asset.assetID_);
				break;
			case AssetType::Audio:
				break;
			case AssetType::Font:
				fontResource_->Load(asset.assetID_, asset.fullpath_.str(), 48.0f);
				break;
			case AssetType::Movie:
				movieResource_->Load(asset.assetID_, asset.fullpath_.str());
				break;
			case AssetType::Prefab:
				break;
			case AssetType::Scene:
				break;
			case AssetType::Skymap:
				skymapResource_->Load(loader, device, cmdQueue, heap_, *this, asset.assetID_);
				break;
			case AssetType::Unknown:
				[[fallthrough]];
			default:
				continue;
			}

			asset.isLoaded_ = true;
		}
	}

	/**
	* [EN]
	* Unloads every currently-loaded asset from its owning resource
	* manager and clears the asset/search maps.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 現在読み込まれている全アセットを、それぞれの所有元リソース
	* マネージャから解放し、アセット/検索マップをクリアする。
	*/
	void ResourceCache::Unload(LoaderSystem& loader, BindlessHeap* heap)
	{
		for (auto& asset : assetsMap_ | std::ranges::views::values)
		{
			if (!asset.isLoaded_)
			{
				continue;
			}

			switch (asset.type_)
			{
			case AssetType::Texture:
				imageResource_->Unload(loader, asset.assetID_, heap);
				break;
			case AssetType::Model:
				modelResource_->Unload(loader, asset.assetID_, heap);
				break;
			case AssetType::Animation:
				animationResource_->Unload(loader, asset.assetID_);
				break;
			case AssetType::MeshCollision:
				meshCollisionResource_->Unload(loader, asset.assetID_);
				break;
			case AssetType::Effect:
				effekseerResource_->Unload(loader, asset.assetID_);
				break;
			case AssetType::Audio:
				break;
			case AssetType::Font:
				fontResource_->Unload(asset.assetID_);
				break;
			case AssetType::Movie:
				movieResource_->Unload(asset.assetID_);
				break;
			case AssetType::Skymap:
				skymapResource_->Unload(loader, asset.assetID_, heap);
				break;
			case AssetType::Unknown:
				[[fallthrough]];
			default:
				continue;
			}

			asset.isLoaded_ = false;
		}

		fontResource_->Clear();
		movieResource_->Clear();

		assetsMap_.clear();
		searchMap_.clear();
	}

	/**
	* [EN]
	* Returns the sprite/texture resource manager.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* スプライト/テクスチャリソースマネージャを返す。
	*/
	ImageResource* ResourceCache::GetImageResource()const
	{
		return imageResource_.get();
	}

	/**
	* [EN]
	* Returns the model resource manager.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* モデルリソースマネージャを返す。
	*/
	ModelResource* ResourceCache::GetModelResource()const
	{
		return modelResource_.get();
	}

	/**
	* [EN]
	* Returns the animation resource manager.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* アニメーションリソースマネージャを返す。
	*/
	AnimationResource* ResourceCache::GetAnimationResource()const
	{
		return animationResource_.get();
	}

	/**
	* [EN]
	* Returns the mesh collision resource manager.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 衝突ジオメトリリソースマネージャを返す。
	*/
	MeshCollisionResource* ResourceCache::GetMeshCollisionResource()const
	{
		return meshCollisionResource_.get();
	}

	/**
    * [EN]
    * Returns the Effekseer effect resource manager.
    *
    * ---------------------------------------------------------------------
    *
    * [JP]
    * Effekseerエフェクトリソースマネージャを返す。
    */
	EffekseerResource* ResourceCache::GetEffekseerResource()const
	{
		return effekseerResource_.get();
	}

	/**
	* [EN]
	* Returns the font resource manager.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* フォントリソースマネージャを返す。
	*/
	FontResource* ResourceCache::GetFontResource()const
	{
		return fontResource_.get();
	}

	/**
	* [EN]
	* Returns the movie resource manager.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* ムービーリソースマネージャを返す。
	*/
	MovieResource* ResourceCache::GetMovieResource()const
	{
		return movieResource_.get();
	}

	/**
	* [EN]
	* Returns the skymap resource manager.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* スカイマップリソースマネージャを返す。
	*/
	SkymapResource* ResourceCache::GetSkymapResource()const
	{
		return skymapResource_.get();
	}

	/**
	* [EN]
	* Returns the prefab pool.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* プレハブプールを返す。
	*/
	PrefabPool& ResourceCache::GetPrefabPool()
	{
		return prefabPool_;
	}

	/**
	* [EN]
	* Returns the scene pool.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* シーンプールを返す。
	*/
	ScenePool& ResourceCache::GetScenePool()
	{
		return scenePool_;
	}

	/**
	* [EN]
	* Returns the full map of discovered assets, keyed by asset ID.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 発見済みの全アセットのマップを、アセット ID をキーとして返す。
	*/
	const FlatMap<Uint32, Asset>& ResourceCache::AssetList()const
	{
		return assetsMap_;
	}

	/**
	* [EN]
	* Returns the resolved project root path.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 解決済みのプロジェクトルートパスを返す。
	*/
	const std::filesystem::path& ResourceCache::ProjectRootPath()const
	{
		return projectRootPath_;
	}

	/**
	* [EN]
	* Returns the bindless descriptor heap used for GPU resource views.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* GPU リソースビュー用のバインドレスディスクリプタヒープを返す。
	*/
	BindlessHeap* ResourceCache::Heap()const
	{
		return heap_;
	}

	/**
	* [EN]
	* Walks targetPath's directory tree, discovering asset files by
	* extension, reconciling/recovering their .meta-derived GUIDs
	* (including orphaned .meta recovery after a rename), and
	* registering new Asset entries.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* targetPath のディレクトリツリーを走査し、拡張子でアセットファイルを
	* 発見し、.meta 由来の GUID を整合・復旧（リネーム後の孤立した .meta
	* ファイルの復旧を含む）し、新しい Asset エントリを登録する。
	*/
	void ResourceCache::Scan(String targetPath)
	{
		std::filesystem::path root(targetPath.c_str());
		if (!std::filesystem::exists(root))
		{
			return;
		}

		/// [EN] Remember which paths are already registered, so the second pass below only adds genuinely new assets.
		/// [JP] 既に登録済みのパスを記憶しておく。これにより、以下の第二パスでは本当に新しいアセットのみを追加する。
		std::unordered_set<std::string> existingPaths;
		for (auto& asset : assetsMap_ | std::ranges::views::values)
		{
			existingPaths.insert(asset.path_.str());
		}

		std::unordered_map<std::string, std::filesystem::path> orphanedMetas;
		std::unordered_set<std::string> ambiguousOrphanNames;

		/// [EN] First pass: find "orphaned" .meta files (a .meta with no corresponding asset file anymore, likely because the asset was renamed) so their GUID can be recovered instead of minted fresh. A filename appearing more than once is ambiguous and excluded, since we can't tell which orphan matches which renamed file.
		/// [JP] 第一パス: 「孤立した」.meta ファイル（対応するアセットファイルがもう存在しない、おそらくアセットがリネームされたもの）を見つける。これにより、新規に発行するのではなくその GUID を復旧できるようにする。同じファイル名が複数回現れる場合は曖昧なため除外する。どの孤立ファイルがどのリネーム後ファイルに対応するか判別できないため。
		for (auto it = std::filesystem::recursive_directory_iterator(root); it != std::filesystem::recursive_directory_iterator(); ++it)
		{
			if (it->is_directory())
			{
				std::string dirName = it->path().filename().string();
				if (excludeDirectories_.contains(dirName))
				{
					it.disable_recursion_pending();
				}
				continue;
			}

			const auto& entry = *it;
			if (!entry.is_regular_file() || entry.path().extension().string() != ".meta")
			{
				continue;
			}

			std::filesystem::path targetPath = entry.path();
			targetPath.replace_extension();

			if (std::filesystem::exists(targetPath))
			{
				continue;
			}

			std::string filename = targetPath.filename().string();
			if (ambiguousOrphanNames.contains(filename))
			{
				continue;
			}

			if (orphanedMetas.contains(filename))
			{
				orphanedMetas.erase(filename);
				ambiguousOrphanNames.insert(filename);
			}
			else
			{
				orphanedMetas.insert({ filename, entry.path() });
			}
		}

		/// [EN] Second pass: discover actual asset files by extension and register any not already known.
		/// [JP] 第二パス: 拡張子で実際のアセットファイルを発見し、まだ知られていないものを登録する。
		for (auto it = std::filesystem::recursive_directory_iterator(root); it != std::filesystem::recursive_directory_iterator(); ++it)
		{
			if (it->is_directory())
			{
				std::string dirName = it->path().filename().string();
				if (excludeDirectories_.contains(dirName))
				{
					it.disable_recursion_pending();
					continue;
				}
				continue;
			}

			const auto& entry = *it;

			if (!entry.is_regular_file())
			{
				continue;
			}

			std::string extention = entry.path().extension().string();

			if (!includeExtensions_.contains(extention))
			{
				continue;
			}

			std::string rootPath = std::filesystem::relative(entry.path(), root).string();
			std::replace(rootPath.begin(), rootPath.end(), '\\', '/');

			if (existingPaths.contains(rootPath))
			{
				continue;
			}

			Asset asset;

			std::string fullPath = std::filesystem::absolute(entry.path()).string();
			std::replace(fullPath.begin(), fullPath.end(), '\\', '/');
			asset.fullpath_ = String(fullPath);

			asset.path_ = String(rootPath);

			/// [EN] Classify the asset's type from its extension.
			/// [JP] 拡張子からアセットの種別を分類する。
			{
				if (extention == ".png" || extention == ".jpg" || extention == ".jpeg" || extention == ".dds" || extention == ".texture")
				{
					asset.type_ = AssetType::Texture;
				}
				else if (extention == ".gltf" || extention == ".glb" || extention == ".crister")
				{
					asset.type_ = AssetType::Model;
				}
				else if (extention == ".animation")
				{
					asset.type_ = AssetType::Animation;
				}
				else if (extention == ".collision")
				{
					asset.type_ = AssetType::MeshCollision;
				}
				else if (extention == ".efkefc" || extention == ".effekseer" || extention == ".zephyr")
				{
					asset.type_ = AssetType::Effect;
				}
				else if (extention == ".mp3" || extention == ".wav" || extention == ".acb" || extention == ".awb" || extention == ".sound")
				{
					asset.type_ = AssetType::Audio;
				}
				else if (extention == ".ttf" || extention == ".otf" || extention == ".ttc")
				{
					asset.type_ = AssetType::Font;
				}
				else if (extention == ".mp4" || extention == ".movie")
				{
					asset.type_ = AssetType::Movie;
				}
				else if (extention == ".prefab")
				{
					asset.type_ = AssetType::Prefab;
				}
				else if (extention == ".scene")
				{
					asset.type_ = AssetType::Scene;
				}
				else if (extention == ".hdr" || extention == ".skymap")
				{
					asset.type_ = AssetType::Skymap;
				}
				else
				{
					asset.type_ = AssetType::Unknown;
				}
			}

			AssetMeta meta;
			Bool skipMeta = noMetaExtensions_.contains(extention);

			if (skipMeta)
			{
				/// [EN] This extension never gets a .meta file: derive a GUID from the path hash instead, bumping on collision.
				/// [JP] この拡張子は .meta ファイルを持たない: 代わりにパスハッシュから GUID を導出し、衝突時はインクリメントする。
				meta.guid_ = static_cast<Uint32>(std::hash<std::string>{}(asset.path_.c_str()));
				while (assetsMap_.count(meta.guid_))
				{
					meta.guid_++;
				}
			}
			else
			{
				std::filesystem::path metaPath = entry.path();
				metaPath += ".meta";
				if (std::filesystem::exists(metaPath))
				{
					/// [EN] A .meta file already sits next to this asset: read its GUID directly.
					///      Rewrite the file if it couldn't be read and a fresh GUID had to
					///      be minted, so the repair doesn't repeat next scan.
					/// [JP] このアセットの隣に既に .meta ファイルが存在する: その GUID を直接読み取る。
					///      読み込みに失敗し新規 GUID の発行が必要だった場合はファイルを
					///      書き直し、次回スキャンで同じ修復を繰り返さないようにする。
					if (!ReadAssetMeta(metaPath, meta))
					{
						meta = AssetMeta{};
						meta.guid_ = static_cast<Uint32>(std::hash<std::string>{}(asset.path_.c_str() + std::to_string(std::time(nullptr))));
						while (assetsMap_.count(meta.guid_))
						{
							meta.guid_++;
						}
						WriteAssetMetaFile(metaPath, meta);
					}
				}
				else
				{
					/// [EN] No .meta file yet: check whether an orphaned .meta from a same-named file (i.e. this file was likely renamed/moved) can be recovered and relocated here, preserving its GUID.
					/// [JP] まだ .meta ファイルが無い: 同名のファイルから孤立した .meta（すなわちこのファイルがリネーム/移動された可能性がある）を復旧し、この場所へ再配置してその GUID を維持できるか確認する。
					auto orphanIt = orphanedMetas.find(entry.path().filename().string());
					Bool recovered = false;

					if (orphanIt != orphanedMetas.end())
					{
						/// [EN] Uses the same read as the main .meta path so a renamed
						///      asset's orphaned .meta still recovers its original GUID.
						/// [JP] メインの .meta 読み取りと同じ経路を使い、リネームされた
						///      アセットの孤立した .meta でも元の GUID を復旧できるようにする。
						if (ReadAssetMeta(orphanIt->second, meta))
						{
							/// [EN] Move the recovered .meta file to sit alongside the renamed asset; fall back to copy+delete if the rename fails (e.g. across volumes).
							/// [JP] 復旧した .meta ファイルを、リネームされたアセットの隣へ移動する。リネームが失敗した場合（異なるボリューム間など）はコピー＋削除にフォールバックする。
							std::error_code errorCode;
							std::filesystem::rename(orphanIt->second, metaPath, errorCode);
							if (errorCode)
							{
								std::filesystem::copy_file(orphanIt->second, metaPath, std::filesystem::copy_options::overwrite_existing, errorCode);
								std::filesystem::remove(orphanIt->second, errorCode);
							}

							recovered = true;
						}

						orphanedMetas.erase(orphanIt);
					}

					if (!recovered)
					{
						/// [EN] Genuinely new asset (or recovery failed): mint a fresh GUID and persist a new .meta file for it.
						/// [JP] 本当に新規のアセット（または復旧に失敗した場合）: 新しい GUID を発行し、そのための新しい .meta ファイルを永続化する。
						meta.guid_ = static_cast<Uint32>(std::hash<std::string>{}(asset.path_.c_str() + std::to_string(std::time(nullptr))));
						while (assetsMap_.count(meta.guid_))
						{
							meta.guid_++;
						}

						WriteAssetMetaFile(metaPath, meta);
					}
				}
			}

			asset.assetID_ = meta.guid_;

			assetsMap_.insert({ asset.assetID_, asset });
			searchMap_.insert(asset.path_, asset.assetID_);

			std::string directory = std::filesystem::path(asset.path_.c_str()).parent_path().string();
			std::replace(directory.begin(), directory.end(), '\\', '/');
		}
	}

	/**
	* [EN]
	* Removes Asset entries whose backing file no longer exists
	* (unloading them first if loaded), then re-runs Scan to pick up any
	* new/changed files.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 裏付けとなるファイルがもう存在しない Asset エントリを削除し
	* （読み込み済みであれば先に解放する）、その後 Scan を再実行して
	* 新規/変更されたファイルを取り込む。
	*/
	void ResourceCache::Rescan()
	{
		for (auto it = assetsMap_.begin(); it != assetsMap_.end();)
		{
			if (!std::filesystem::exists(it->second.fullpath_.c_str()))
			{
				if (it->second.isLoaded_)
				{
					switch (it->second.type_)
					{
					case AssetType::Texture:
						imageResource_->Unload(loader_, it->second.assetID_, heap_);
						break;
					case AssetType::Model:
						modelResource_->Unload(loader_, it->second.assetID_, heap_);
						break;
					case AssetType::Animation:
						animationResource_->Unload(loader_, it->second.assetID_);
						break;
					case AssetType::MeshCollision:
						meshCollisionResource_->Unload(loader_, it->second.assetID_);
						break;
					case AssetType::Effect:
						effekseerResource_->Unload(loader_, it->second.assetID_);
						break;
					case AssetType::Audio:
						break;
					case AssetType::Font:
						fontResource_->Unload(it->second.assetID_);
						break;
					case AssetType::Movie:
						movieResource_->Unload(it->second.assetID_);
						break;
					case AssetType::Skymap:
						skymapResource_->Unload(loader_, it->second.assetID_, heap_);
						break;
					default:
						break;
					}
				}
				it = assetsMap_.erase(it);
			}
			else
			{
				++it;
			}
		}

		/// [EN] Rebuild the search index from the surviving assets before re-scanning for new ones.
		/// [JP] 新規アセットを再スキャンする前に、生き残ったアセットから検索インデックスを再構築する。
		searchMap_.clear();
		for (auto& asset : assetsMap_ | std::ranges::views::values)
		{
			searchMap_.insert(asset.path_, asset.assetID_);
		}

		Scan(String(projectRootPath_.c_str()));
	}
}
