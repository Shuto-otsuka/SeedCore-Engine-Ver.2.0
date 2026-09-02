#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/Utility/ArtMap.h>
#include <FoundationEngine/Utility/FlatMap.h>
#include <FoundationEngine/Resource/PrefabPool.h>
#include <FoundationEngine/Resource/ScenePool.h>
#include <FoundationEngine/Resource/AxisConvention.h>
#include <FoundationEngine/JobSystem/JobTaskflow.h>

namespace SeedCore
{
	struct LoaderSystem;
	class ImageResource;
	class ModelResource;
	class AnimationResource;
	class MeshCollisionResource;
	class MaterialResource;
	class SkeletonResource;
	class BindlessHeap;
	class D3D12CommandQueue;
	class BC7CompressShader;
	class FontManager;
	class JobExecutor;

	class FontResource;
	class SkymapResource;
	class EffekseerResource;
	class MovieResource;

	/// [EN] Identifies which kind of asset a given Asset entry represents, driving both file-extension classification during Scan and which resource manager handles loading/unloading it.
	/// [JP] 各 Asset エントリがどの種類のアセットを表すかを識別する。Scan 時のファイル拡張子分類と、読み込み/解放を担当するリソースマネージャの両方を決定する。
	enum class AssetType
	{
		/// [EN] Image texture (.png/.jpg/.dds/...).
		/// [JP] 画像テクスチャ（.png/.jpg/.dds/...）。
		Texture,

		/// [EN] 3D model (.gltf/.glb/.crister).
		/// [JP] 3Dモデル（.gltf/.glb/.crister）。
		Model,

		/// [EN] Particle/visual effect (.efkefc/.effekseer/.zephyr).
		/// [JP] パーティクル/ビジュアルエフェクト（.efkefc/.effekseer/.zephyr）。
		Effect,

		/// [EN] Audio (.acb/.awb/.sound).
		/// [JP] オーディオ（.acb/.awb/.sound）。
		Audio,

		/// [EN] Font (.ttf/.otf/.ttc).
		/// [JP] フォント（.ttf/.otf/.ttc）。
		Font,

		/// [EN] Movie (.mp4/.movie).
		/// [JP] ムービー（.mp4/.movie）。
		Movie,

		/// [EN] Animation clip (.animation).
		/// [JP] アニメーションクリップ（.animation）。
		Animation,

		/// [EN] Actor prefab (.prefab).
		/// [JP] Actor プレハブ（.prefab）。
		Prefab,

		/// [EN] Scene (.scene).
		/// [JP] シーン（.scene）。
		Scene,

		/// [EN] Skybox/environment map (.hdr/.skymap).
		/// [JP] スカイボックス/環境マップ（.hdr/.skymap）。
		Skymap,

		/// [EN] Baked mesh collision geometry (.collision).
		/// [JP] 焼き込み済みの衝突ジオメトリ（.collision）。
		MeshCollision,

		/// [EN] Standalone material (.material).
		/// [JP] 単体マテリアル（.material）。
		Material,

		/// [EN] Standalone skeleton rig (.skeleton): sockets + root bone.
		/// [JP] 単体スケルトンリグ（.skeleton）: ソケット + ルートボーン。
		Skeleton,

		/// [EN] Extension not recognized by Scan.
		/// [JP] Scan が認識しない拡張子。
		Unknown,
	};

	/**
	* [EN]
	* A single discovered project asset: its project-relative and
	* absolute filesystem paths, classified type, stable GUID-derived
	* ID, and whether it is currently loaded into a resource manager.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 発見された単一のプロジェクトアセット: プロジェクト相対パスと
	* 絶対ファイルシステムパス、分類された種類、GUID から導出された
	* 安定的な ID、および現在リソースマネージャへ読み込まれているか。
	*/
	struct Asset
	{
		/// [EN] Path relative to the project root, with forward slashes.
		/// [JP] プロジェクトルートからの相対パス（スラッシュ区切り）。
		String path_;

		/// [EN] Absolute filesystem path, with forward slashes.
		/// [JP] 絶対ファイルシステムパス（スラッシュ区切り）。
		String fullpath_;

		/// [EN] The classified asset type.
		/// [JP] 分類済みのアセット種別。
		AssetType type_ = AssetType::Unknown;

		/// [EN] Stable ID derived from the asset's .meta GUID (or a path hash for extensions that skip .meta files).
		/// [JP] アセットの .meta GUID から導出される安定的な ID（.meta ファイルを持たない拡張子の場合はパスハッシュ）。
		Uint32 assetID_ = 0;

		/// [EN] Whether this asset is currently loaded into its resource manager.
		/// [JP] このアセットが現在、対応するリソースマネージャへ読み込まれているかどうか。
		Bool isLoaded_ = false;
	};

	/**
	* [EN]
	* Serializable sidecar metadata stored in an asset's .meta file:
	* primarily its stable GUID, which survives file moves/renames
	* (unlike a path-derived hash).
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* アセットの .meta ファイルに格納される、シリアライズ可能な
	* サイドカーメタデータ。主にその安定的な GUID を保持し、これは
	* （パス由来のハッシュとは異なり）ファイルの移動/リネームを経ても
	* 変化しない。
	*/
	struct AssetMeta
	{
		/// [EN] Meta file format version, for future migration.
		/// [JP] メタファイルフォーマットのバージョン。将来のマイグレーション用。
		Uint32 version_ = 1;

		/// [EN] Stable identifier for this asset, persisted across file moves/renames.
		/// [JP] このアセットの安定的な識別子。ファイルの移動/リネームを跨いで永続化される。
		Uint32 guid_ = 0;

		/// [EN] Axis convention applied to this Model asset. For a .gltf/.glb source,
		///      this is the convention to apply on (re-)import. For a .crister-only
		///      asset, this records the convention currently baked into the file.
		///      Unused/ignored for non-Model asset types.
		/// [JP] この Model アセットに適用する軸コンベンション。.gltf/.glb ソースの
		///      場合は(再)インポート時に適用する規約。.crister のみのアセットの
		///      場合はファイルに現在焼き込まれている規約を記録する。Model 以外の
		///      アセット種別では未使用。
		AxisConvention axisConvention_;

		/**
		* [EN]
		* Serialization hook: reads/writes version_, guid_, and axisConvention_.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* シリアライズ用フック: version_、guid_、axisConvention_ を読み書きする。
		*/
		template<class Archive>
		void Serialize(Archive& archive)
		{
			archive.Field("version", version_);
			archive.Field("guid", guid_);
			archive.Field("axis_convention", axisConvention_);
		}
	};

	/**
	* [EN]
	* Owns the project's discovered asset table and the resource
	* managers (texture/model/animation/font/movie/skymap) that load them,
	* plus the Prefab/Scene pools. Scan() walks the project directory
	* tree to (re)discover assets and reconcile their .meta-derived
	* GUIDs; Step()/Async() drive incremental/background loading with
	* progress tracking, while Reload()/Unload() perform full
	* synchronous passes.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* プロジェクトの発見済みアセットテーブルと、それらを読み込む
	* リソースマネージャ（texture/model/animation/font/movie/skymap）、および
	* Prefab/Scene プールを所有する。Scan() はプロジェクトディレクトリ
	* ツリーを走査してアセットを（再）発見し、.meta 由来の GUID を
	* 整合させる。Step()/Async() はプログレス追跡付きの逐次/バックグラウンド
	* 読み込みを駆動し、Reload()/Unload() は完全同期パスを実行する。
	*/
	class SEEDCORE_API ResourceCache
	{
	public:
		/**
		* [EN]
		* Constructs the cache, creating each resource manager and
		* resolving the project root path.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* キャッシュを構築し、各リソースマネージャを生成し、プロジェクト
		* ルートパスを解決する。
		*/
		ResourceCache(LoaderSystem& loader, ID3D12Device* device, D3D12CommandQueue* cmdQueue, BindlessHeap* heap);

		/**
		* [EN]
		* Destroys the cache, unloading every currently-loaded asset.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* キャッシュを破棄し、現在読み込まれている全アセットを解放する。
		*/
		~ResourceCache();

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
		void Reload(LoaderSystem& loader, ID3D12Device* device, D3D12CommandQueue* cmdQueue, BC7CompressShader& bc7Shader);

		/**
		* [EN]
		* Starts (or restarts) an incremental scan/load pass: rescans the
		* project directory tree, builds the pending-load queue ordered
		* by AssetType, and resets progress counters.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 逐次的なスキャン/読み込みパスを開始（または再開始）する:
		* プロジェクトディレクトリツリーを再スキャンし、AssetType 順に
		* 並べた保留読み込みキューを構築し、進捗カウンタをリセットする。
		*/
		void Async();

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
		Bool Step(LoaderSystem& loader, ID3D12Device* device, D3D12CommandQueue* cmdQueue, BindlessHeap* heap, BC7CompressShader& bc7Shader);

		/**
		* [EN]
		* Starts (once per Async() pass - later calls are ignored until the
		* pass finishes) a background job that repeatedly calls Step() on a
		* dedicated worker thread until the pending queue is drained. Callers
		* poll Complete()/Progress() (both atomic) from the main thread instead
		* of calling Step() themselves, so a heavy per-asset load never blocks
		* frame presentation. The destructor waits for this job to finish
		* before unloading anything, so it's always safe to tear down mid-load.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* バックグラウンドジョブを開始する（Async() パスごとに1回のみ - パスが
		* 終わるまで以降の呼び出しは無視される）。専用のワーカースレッド上で
		* キューが尽きるまで Step() を繰り返し呼ぶ。呼び出し側は自分で Step()
		* を呼ぶ代わりに、メインスレッドから Complete()/Progress()（どちらも
		* atomic）をポーリングする。これにより重いアセット単位の読み込みが
		* フレーム表示をブロックすることがなくなる。デストラクタはこの
		* ジョブの完了を待ってから解放処理に入るので、読み込み中に破棄しても
		* 常に安全。
		*/
		void StepAsync(LoaderSystem& loader, ID3D12Device* device, D3D12CommandQueue* cmdQueue, BindlessHeap* heap, BC7CompressShader& bc7Shader);

		/**
		* [EN]
		* Returns whether the current Async() pass has scanned and
		* loaded every pending asset.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 現在の Async() パスが、保留中の全アセットをスキャン・読み込み
		* 済みかどうかを返す。
		*/
		Bool Complete()const;

		/**
		* [EN]
		* Returns the current load progress of the Async() pass, from 0
		* to 1 (1 if there is nothing to load).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* Async() パスの現在の読み込み進捗を 0 から 1 の範囲で返す
		* （読み込むものが無ければ 1）。
		*/
		Float Progress()const;

		/**
		* [EN]
		* Returns the Asset registered under id, or nullptr if unknown.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* id に登録されている Asset を返す。不明であれば nullptr を返す。
		*/
		Asset* GetAsset(Uint32 id);

		/**
		* [EN]
		* Returns the asset ID whose path matches filename, or 0 if not
		* found. filename can be a full project-root-relative path (e.g.
		* "UserProject/Assets/Scene/Foo.scene", matched exactly) or just a
		* bare filename (e.g. "Foo.scene", matched against every asset's
		* own filename - the caller doesn't need to know which subfolder
		* it lives in). If more than one asset shares that bare filename,
		* the first one found wins and a warning is logged, since which
		* one is "correct" is ambiguous.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* パスが filename に一致するアセットの ID を返す。見つからなければ
		* 0 を返す。filename には、プロジェクトルート相対のフルパス(例:
		* "UserProject/Assets/Scene/Foo.scene"、完全一致)か、単なる
		* ファイル名(例: "Foo.scene"、各アセット自身のファイル名と比較 -
		* 呼び出し側はどのサブフォルダにあるか知らなくてよい)のどちらも
		* 渡せる。同じファイル名を持つアセットが複数ある場合は最初に
		* 見つかったものを使い、警告を出す(どれが「正しい」かは判別
		* できないため)。
		*/
		Uint32 GetAssetID(String filename);

		/**
		* [EN]
		* Returns every Asset whose path contains key as a substring.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* パスに key を部分文字列として含む、全ての Asset を返す。
		*/
		DynamicArray<Asset*> Search(String key);

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
		AxisConvention ReadAxisConvention(Uint32 assetId)const;

		/**
		* [EN]
		* Persists convention into id's .meta sidecar, preserving the
		* existing guid_. Creates the .meta file if it doesn't exist yet.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* id の .meta サイドカーへ convention を永続化する。既存の guid_ は
		* 保持する。.meta ファイがまだ無ければ新規作成する。
		*/
		void WriteAssetMeta(Uint32 assetId, const AxisConvention& convention);

		/**
		* [EN]
		* Returns the sprite/texture resource manager.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* スプライト/テクスチャリソースマネージャを返す。
		*/
		ImageResource* GetImageResource()const;

		/**
		* [EN]
		* Returns the model resource manager.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* モデルリソースマネージャを返す。
		*/
		ModelResource* GetModelResource()const;

		/**
		* [EN]
		* Returns the animation resource manager.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* アニメーションリソースマネージャを返す。
		*/
		AnimationResource* GetAnimationResource()const;

		/**
		* [EN]
		* Returns the mesh collision resource manager.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 衝突ジオメトリリソースマネージャを返す。
		*/
		MeshCollisionResource* GetMeshCollisionResource()const;

		/**
		* [EN]
		* Returns the material resource manager.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* マテリアルリソースマネージャを返す。
		*/
		MaterialResource* GetMaterialResource()const;

		/**
		* [EN]
		* Returns the skeleton rig resource manager.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* スケルトンリグリソースマネージャを返す。
		*/
		SkeletonResource* GetSkeletonResource()const;

		/**
		* [EN]
		* Returns the Effekseer effect resource manager.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* Effekseerエフェクトリソースマネージャを返す。
		*/
		EffekseerResource* GetEffekseerResource()const;

		/**
		* [EN]
		* Returns the font resource manager.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* フォントリソースマネージャを返す。
		*/
		FontResource* GetFontResource()const;

		/**
		* [EN]
		* Returns the movie resource manager.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* ムービーリソースマネージャを返す。
		*/
		MovieResource* GetMovieResource()const;

		/**
		* [EN]
		* Returns the skymap resource manager.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* スカイマップリソースマネージャを返す。
		*/
		SkymapResource* GetSkymapResource()const;

		/**
		* [EN]
		* Returns the prefab pool.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* プレハブプールを返す。
		*/
		PrefabPool& GetPrefabPool();

		/**
		* [EN]
		* Returns the scene pool.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* シーンプールを返す。
		*/
		ScenePool& GetScenePool();

		/**
		* [EN]
		* Returns the full map of discovered assets, keyed by asset ID.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 発見済みの全アセットのマップを、アセット ID をキーとして返す。
		*/
		const FlatMap<Uint32, Asset>& AssetList()const;

		/**
		* [EN]
		* Returns the resolved project root path.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 解決済みのプロジェクトルートパスを返す。
		*/
		const std::filesystem::path& ProjectRootPath()const;

		/**
		* [EN]
		* Returns the bindless descriptor heap used for GPU resource views.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* GPU リソースビュー用のバインドレスディスクリプタヒープを返す。
		*/
		BindlessHeap* Heap()const;

	private:
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
		void Unload(LoaderSystem& loader, BindlessHeap* heap);

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
		* targetPath のディレクトリツリーを走査し、拡張子でアセット
		* ファイルを発見し、.meta 由来の GUID を整合・復旧（リネーム後の
		* 孤立した .meta ファイルの復旧を含む）し、新しい Asset エントリを
		* 登録する。
		*/
		void Scan(String targetPath);

		/**
		* [EN]
		* Removes Asset entries whose backing file no longer exists
		* (unloading them first if loaded), then re-runs Scan to pick up
		* any new/changed files.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 裏付けとなるファイルがもう存在しない Asset エントリを削除し
		* （読み込み済みであれば先に解放する）、その後 Scan を再実行して
		* 新規/変更されたファイルを取り込む。
		*/
		void Rescan();

	private:
		/// [EN] Every discovered asset, keyed by asset ID.
		/// [JP] 発見済みの全アセット。アセット ID をキーとする。
		FlatMap<Uint32, Asset> assetsMap_;

		/// [EN] Adaptive radix tree mapping an asset's path to its asset ID, for fast prefix/substring lookups.
		/// [JP] アセットのパスをそのアセット ID へ対応付ける、高速な前方一致/部分一致検索のためのアダプティブ基数木。
		ArtMap<String, Uint32> searchMap_;

		/// [EN] Resolved absolute path of the project root.
		/// [JP] 解決済みの、プロジェクトルートの絶対パス。
		std::filesystem::path projectRootPath_;

		/// [EN] The loader system used to perform actual asset I/O.
		/// [JP] 実際のアセット I/O を実行するために使用されるローダーシステム。
		LoaderSystem& loader_;

		/// [EN] Bindless descriptor heap used when loading GPU-visible resources.
		/// [JP] GPU から参照可能なリソースを読み込む際に使用される、バインドレスディスクリプタヒープ。
		BindlessHeap* heap_ = nullptr;

		/// [EN] Manager owning loaded sprite/texture resources.
		/// [JP] 読み込み済みのスプライト/テクスチャリソースを所有するマネージャ。
		ResourcePtr<ImageResource> imageResource_;

		/// [EN] Manager owning loaded model resources.
		/// [JP] 読み込み済みのモデルリソースを所有するマネージャ。
		ResourcePtr<ModelResource> modelResource_;

		/// [EN] Manager owning loaded animation resources.
		/// [JP] 読み込み済みのアニメーションリソースを所有するマネージャ。
		ResourcePtr<AnimationResource> animationResource_;

		/// [EN] Manager owning loaded mesh collision resources.
		/// [JP] 読み込み済みの衝突ジオメトリリソースを所有するマネージャ。
		ResourcePtr<MeshCollisionResource> meshCollisionResource_;

		/// [EN] Manager owning loaded standalone material (.material) resources.
		/// [JP] 読み込み済みの単体マテリアル(.material)リソースを所有するマネージャ。
		ResourcePtr<MaterialResource> materialResource_;

		/// [EN] Manager owning loaded standalone skeleton rig (.skeleton) resources.
		/// [JP] 読み込み済みの単体スケルトンリグ(.skeleton)リソースを所有するマネージャ。
		ResourcePtr<SkeletonResource> skeletonResource_;

		/// [EN] Manager owning loaded Effekseer effect resources.
		/// [JP] 読み込み済みのEffekseerエフェクトリソースを所有するマネージャ。
		ResourcePtr<EffekseerResource> effekseerResource_;

		/// [EN] Manager owning loaded font resources.
		/// [JP] 読み込み済みのフォントリソースを所有するマネージャ。
		ResourcePtr<FontResource> fontResource_;

		/// [EN] Manager owning loaded movie resources.
		/// [JP] 読み込み済みのムービーリソースを所有するマネージャ。
		ResourcePtr<MovieResource> movieResource_;

		/// [EN] Manager owning loaded skymap resources.
		/// [JP] 読み込み済みのスカイマップリソースを所有するマネージャ。
		ResourcePtr<SkymapResource> skymapResource_;

		/// [EN] Pool of loaded Prefab assets.
		/// [JP] 読み込み済みの Prefab アセットのプール。
		PrefabPool prefabPool_;

		/// [EN] Pool of loaded Scene assets.
		/// [JP] 読み込み済みの Scene アセットのプール。
		ScenePool scenePool_;

		/// [EN] Asset IDs awaiting load in the current Async() pass, ordered by AssetType.
		/// [JP] 現在の Async() パスで読み込みを待っている、AssetType 順に並んだアセット ID 群。
		DynamicArray<Uint32> pendingAssetIDs_;

		/// [EN] Index of the next pending asset Step() will process. Atomic
		///      since StepAsync() reads/writes it from a background worker
		///      while Complete() may be read concurrently from the main thread.
		/// [JP] 次に Step() が処理する、保留アセットのインデックス。
		///      StepAsync() がバックグラウンドワーカーから読み書きする一方、
		///      Complete() がメインスレッドから同時に読まれうるため atomic。
		std::atomic<Size> pendingIndex_{ 0 };

		/// [EN] Total number of assets queued in the current Async() pass.
		/// [JP] 現在の Async() パスでキューに入れられたアセットの総数。
		std::atomic<Uint32> totalAssetCount_{ 0 };

		/// [EN] Number of assets loaded so far in the current Async() pass.
		/// [JP] 現在の Async() パスで、これまでに読み込まれたアセット数。
		std::atomic<Uint32> loadedAssetCount_{ 0 };

		/// [EN] Whether the directory scan for the current Async() pass has finished.
		/// [JP] 現在の Async() パスにおけるディレクトリスキャンが完了しているかどうか。
		std::atomic<Bool> scanComplete_{ false };

		/// [EN] Backing JobTaskflow for StepAsync()'s background job. Must be a
		///      member (not a StepAsync()-local variable) because JobTopology
		///      only ever holds a reference to the JobTaskflow it runs
		///      (JobTopology::taskflow_) - a local would be destroyed the
		///      moment StepAsync() returns, while the job is still running on
		///      loadExecutor_'s worker thread, leaving that reference dangling.
		/// [JP] StepAsync() のバックグラウンドジョブを支える JobTaskflow。
		///      StepAsync() のローカル変数にしてはいけない - JobTopology は
		///      実行対象の JobTaskflow への参照(JobTopology::taskflow_)しか
		///      持たないため、ローカル変数だと StepAsync() が return した瞬間に
		///      破棄されてしまい、loadExecutor_ のワーカースレッド上ではまだ
		///      ジョブが実行中なのに、その参照がダングリングになる。
		JobTaskflow loadTaskflow_;

		/// [EN] Dedicated single-worker executor backing StepAsync() - lazily
		///      created on first use, destroyed (waiting for outstanding work)
		///      at the start of ~ResourceCache() before anything is unloaded.
		/// [JP] StepAsync() を支える専用のシングルワーカーエグゼキュータ -
		///      初回使用時に遅延生成され、~ResourceCache() の先頭で（未完了の
		///      作業を待ってから）何かを解放するより前に破棄される。
		ResourcePtr<JobExecutor> loadExecutor_;

		/// [EN] Guards against StepAsync() starting a second background job while one from the current Async() pass is still running.
		/// [JP] 現在の Async() パスのバックグラウンドジョブがまだ実行中の間に、StepAsync() が2つ目のジョブを開始しないようにする。
		std::atomic<Bool> loadStarted_{ false };

	private:
		/// [EN] File extensions Scan recognizes as candidate assets.
		/// [JP] Scan がアセット候補として認識するファイル拡張子。
		std::set<std::string_view> includeExtensions_ =
		{
			".png", ".jpg", ".jpeg", ".texture", ".dds",
			".gltf", ".glb", ".crister",
			".animation",
			".collision",
			".material",
			".skeleton",
			".navmesh",
			".efkefc", ".effekseer", ".zephyr",
			".mp3", ".wav", ".acb", ".awb", ".audio",
			".hdr", ".skymap",
			".ttf", ".otf", ".ttc",
			".mp4", ".movie",
			".scene", ".prefab",
			".h", ".cpp", ".hlsli", ".hlsl",
		};

		/// [EN] Directory names Scan skips entirely (engine/tooling directories, not user assets).
		/// [JP] Scan が完全にスキップするディレクトリ名（ユーザーアセットではなく、エンジン/ツール用ディレクトリ）。
		std::set<std::string_view> excludeDirectories_ =
		{
			"AIEngine",
			"AudioEngine",
			"CompiledShaderObject",
			"Editor",
			"External",
			"FoundationEngine",
			"GraphicsEngine",
			"Launcher",
			"Logs",
			"Package",
			"PhysicsEngine",
			"Runtime",
			"SeedCore",
			"Tools",
			".vs",
			"x64",
			".git",
		};

		/// [EN] File extensions that don't get a companion .meta file (their GUID is derived from a path hash instead).
		/// [JP] 対応する .meta ファイルを持たないファイル拡張子（その GUID は代わりにパスハッシュから導出される）。
		std::set<std::string_view> noMetaExtensions_ =
		{
			".h", ".cpp", ".cs", ".hlsli", ".hlsl"
		};
	};
}
