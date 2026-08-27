#include <FoundationEngine/Resource/Scene.h>
#include <FoundationEngine/Resource/ScenePool.h>
#include <FoundationEngine/Resource/ResourceCache.h>
#include <FoundationEngine/ECS/System/SceneTransitionSystem.h>
#include <FoundationEngine/ECS/Actor.h>
#include <FoundationEngine/ECS/World.h>
#include <FoundationEngine/Log/Error.h>
#include <FoundationEngine/Serialization/Json/JsonArchive.h>

namespace SeedCore
{
	World* Scene::world_ = nullptr;
	ResourceCache* Scene::resource_ = nullptr;
	JobExecutor* Scene::executor_ = nullptr;
	SceneTransitionSystem Scene::transitionSystem_;

	/**
	* [EN]
	* Records every root actor (and its descendants) in world into
	* nodes_, replacing any previously captured data.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* world 内の全ルート actor（とその子孫）を nodes_ へ記録し、以前に
	* 取得していたデータを置き換える。
	*/
	void Scene::Capture(World& world)
	{
		nodes_.clear();

		/// [EN] Only root actors are enumerated here; CaptureActorNode recurses into each root's descendants on its own.
		/// [JP] ここで列挙するのはルート actor のみ。各ルートの子孫への再帰は CaptureActorNode 自身が行う。
		for (auto& actor : world.GetActors())
		{
			if (actor->GetParent() == nullptr)
			{
				CaptureActorNode(actor.get(), -1, nodes_);
			}
		}
	}

	/**
	* [EN]
	* Discards any captured actor data, releasing the memory it held.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 取得済みの actor データを破棄し、それが保持していたメモリを解放する。
	*/
	void Scene::Clear()
	{
		nodes_.clear();
		nodes_.shrink_to_fit();
	}

	/**
	* [EN]
	* Recreates the captured scene as new actors in world, returning
	* every instantiated actor (roots and descendants alike).
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 取得済みのシーンを world 内に新しい actor 群として再生成し、
	* インスタンス化された全 actor（ルート・子孫問わず）を返す。
	*/
	DynamicArray<Actor*> Scene::Instantiate(World& world, ResourceCache& cache)const
	{
		DynamicArray<Actor*> instantiated;
		instantiated.reserve(nodes_.size());

		/// [EN] Nodes are stored in capture order, so each node's parent (identified by an earlier index) has always already been instantiated by the time we reach it.
		/// [JP] ノードは取得順に格納されているため、各ノードの親（それより前のインデックスで識別される）は、そこに到達する時点で既にインスタンス化済みである。
		for (Size index = 0; index < nodes_.size(); ++index)
		{
			const SerializedActorNode& node = nodes_[index];

			Actor* parentActor = nullptr;
			if (node.parentIndex_ >= 0 && static_cast<Size>(node.parentIndex_) < instantiated.size())
			{
				parentActor = instantiated[node.parentIndex_];
			}

			Actor* actor = InstantiateActorNode(world, cache, node, parentActor, false);
			instantiated.push_back(actor);
		}

		return instantiated;
	}

	/**
	* [EN]
	* Writes this scene's captured data to path as JSON. Returns whether
	* the file was written successfully.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* このシーンの取得済みデータを、JSON として path へ書き込む。
	* ファイルが正常に書き込まれたかどうかを返す。
	*/
	Bool Scene::Write(const std::filesystem::path& path)
	{
		JsonOutputArchive archive;
		Save(archive);
		return archive.Write(String(path.string()));
	}

	/**
	* [EN]
	* Reads captured data from path (JSON), replacing this scene's
	* current data. Returns whether reading succeeded.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* path（JSON）から取得済みデータを読み込み、このシーンの現在の
	* データを置き換える。読み込みに成功したかどうかを返す。
	*/
	Bool Scene::Read(const std::filesystem::path& path)
	{
		JsonInputArchive archive;
		if (!archive.Read(String(path.string())))
		{
			SC_LOG_ERROR("シーンの読み込みに失敗しました ({})", path.string());
			return false;
		}

		Load(archive);

		return true;
	}

	/**
	* [EN]
	* Captures world's current state into a scratch Scene and writes it
	* to path. Returns whether saving succeeded.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* world の現在の状態を作業用 Scene へ取得し、path へ書き込む。
	* 保存に成功したかどうかを返す。
	*/
	Bool Scene::Save(World& world, ResourceCache& cache, const std::filesystem::path& path, const String& raytracingSettingsJson)
	{
		/// [EN] Use the pool's shared scratch instance instead of allocating a new Scene, since this is a one-shot operation with no need for pooled/deduplicated storage.
		/// [JP] 新しい Scene を確保する代わりに、プールの共有作業用インスタンスを使用する。これは使い捨ての操作であり、プール化/重複排除されたストレージは不要なため。
		Scene& scene = cache.GetScenePool().GetScratch();
		scene.Capture(world);
		scene.SetRaytracingSettingsJson(raytracingSettingsJson);

		if (path.has_parent_path())
		{
			std::filesystem::create_directories(path.parent_path());
		}

		return scene.Write(path);
	}

	/**
	* [EN]
	* Reads a scene from path into a scratch Scene, destroys every actor
	* currently in world, and instantiates the loaded scene. Returns
	* whether loading succeeded. path is first looked up in cache as an
	* asset name (a bare filename like "Foo.scene" works - see
	* ResourceCache::GetAssetID) and, if found, that asset's real
	* filesystem path is used instead; only if no matching asset exists
	* is path opened literally (relative to the current working
	* directory), so a caller doesn't have to know the scene's folder
	* structure to load it by name.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* path からシーンを作業用 Scene へ読み込み、world 内の現在の全 actor
	* を破棄した上で、読み込んだシーンをインスタンス化する。読み込みに
	* 成功したかどうかを返す。path はまず cache でアセット名として検索し
	* ("Foo.scene" のような単なるファイル名でもよい - ResourceCache::
	* GetAssetID 参照)、見つかればそのアセットの実際のファイルシステム
	* パスを代わりに使う。一致するアセットが無い場合のみ、path を文字通り
	* (カレントディレクトリからの相対で)開く - 呼び出し側はシーンの
	* フォルダ構成を知らなくても名前だけで読み込める。
	*/
	Bool Scene::Load(World& world, ResourceCache& cache, const std::filesystem::path& path, String* outRaytracingSettingsJson)
	{
		std::filesystem::path resolvedPath = path;
		Uint32 assetID = cache.GetAssetID(String(path.string()));
		if (assetID != 0)
		{
			Asset* asset = cache.GetAsset(assetID);
			if (asset)
			{
				resolvedPath = std::filesystem::path(asset->fullpath_.c_str());
			}
		}

		Scene& scene = cache.GetScenePool().GetScratch();
		if (!scene.Read(resolvedPath))
		{
			return false;
		}

		world.DestroyActors();

		scene.Instantiate(world, cache);

		if (outRaytracingSettingsJson)
		{
			*outRaytracingSettingsJson = scene.GetRaytracingSettingsJson();
		}

		return true;
	}

	/**
	* [EN]
	* Overload of Load resolving the scene's path from an asset ID via cache.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* cache 経由でアセット ID からシーンのパスを解決する Load の
	* オーバーロード。
	*/
	Bool Scene::Load(World& world, ResourceCache& cache, Uint32 assetID, String* outRaytracingSettingsJson)
	{
		Asset* asset = cache.GetAsset(assetID);
		if (!asset)
		{
			return false;
		}

		return Load(world, cache, std::filesystem::path(asset->fullpath_.c_str()), outRaytracingSettingsJson);
	}

	/**
	* [EN]
	* Binds the process-wide world/cache/executor references used by
	* the static Change()/Update() overloads. Must be called once
	* before using them.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 静的な Change()/Update() オーバーロードが使用する、プロセス全体の
	* world/cache/executor 参照を束縛する。それらを使用する前に一度
	* 呼び出す必要がある。
	*/
	void Scene::Initialize(World& world, ResourceCache& cache, JobExecutor& executor)
	{
		world_ = &world;
		resource_ = &cache;
		executor_ = &executor;
	}

	/**
	* [EN]
	* Advances the process-wide scene transition state machine by deltaTime.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* プロセス全体のシーン遷移状態機械を deltaTime だけ進める。
	*/
	void Scene::Update(Float deltaTime)
	{
		if (!world_ || !resource_)
		{
			return;
		}

		transitionSystem_.Update(*world_, *resource_, deltaTime);
	}

	/**
	* [EN]
	* Aborts any in-progress process-wide scene transition and returns it
	* to Idle (see SceneTransitionSystem::Reset).
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 進行中のプロセス全体のシーン遷移を中断し、Idle へ戻す
	* （SceneTransitionSystem::Reset を参照）。
	*/
	void Scene::Reset()
	{
		transitionSystem_.Reset();
	}

	/**
	* [EN]
	* Synchronously switches to targetScene. Returns false if not
	* initialized or a transition is already in progress.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* targetScene へ同期的に切り替える。未初期化であるか、既に遷移が
	* 進行中であれば false を返す。
	*/
	Bool Scene::Change(const std::filesystem::path& targetScene)
	{
		if (!world_ || !resource_ || transitionSystem_.IsTransitioning())
		{
			return false;
		}

		return transitionSystem_.LoadScene(*world_, *resource_, targetScene);
	}

	/**
	* [EN]
	* Overload of Change resolving targetScene from an asset ID.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* アセット ID から targetScene を解決する Change のオーバーロード。
	*/
	Bool Scene::Change(Uint32 targetScene)
	{
		if (!world_ || !resource_ || transitionSystem_.IsTransitioning())
		{
			return false;
		}

		return transitionSystem_.LoadScene(*world_, *resource_, targetScene);
	}

	/**
	* [EN]
	* Begins an asynchronous switch to targetScene, immediately showing
	* loadingScene while targetScene loads in the background. No-op if
	* not initialized or a transition is already in progress.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* targetScene への非同期切り替えを開始する。targetScene が
	* バックグラウンドで読み込まれている間、loadingScene を即座に表示する。
	* 未初期化であるか、既に遷移が進行中であれば何もしない。
	*/
	void Scene::Change(const std::filesystem::path& targetScene, const std::filesystem::path& loadingScene)
	{
		if (!world_ || !resource_ || !executor_ || transitionSystem_.IsTransitioning())
		{
			return;
		}

		transitionSystem_.LoadScene(*world_, *resource_, *executor_, targetScene, loadingScene);
	}

	/**
	* [EN]
	* Overload of the loading-scene Change resolving both scenes from asset IDs.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* アセット ID から両方のシーンを解決する、ローディングシーン版
	* Change のオーバーロード。
	*/
	void Scene::Change(Uint32 targetScene, Uint32 loadingScene)
	{
		if (!world_ || !resource_ || !executor_ || transitionSystem_.IsTransitioning())
		{
			return;
		}

		transitionSystem_.LoadScene(*world_, *resource_, *executor_, targetScene, loadingScene);
	}

	/**
	* [EN]
	* Begins an asynchronous switch to targetScene using a fade-out/
	* fade-in effect with the given durations.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 指定された長さのフェードアウト/フェードインエフェクトを使用して、
	* targetScene への非同期切り替えを開始する。
	*/
	void Scene::Change(const std::filesystem::path& targetScene, Float fadeOutDuration, Float fadeInDuration)
	{
		if (!world_ || !resource_ || !executor_ || transitionSystem_.IsTransitioning())
		{
			return;
		}

		transitionSystem_.LoadScene(*world_, *resource_, *executor_, targetScene, fadeOutDuration, fadeInDuration);
	}

	/**
	* [EN]
	* Overload of the fade-effect Change resolving targetScene from an asset ID.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* アセット ID から targetScene を解決する、フェードエフェクト版
	* Change のオーバーロード。
	*/
	void Scene::Change(Uint32 targetScene, Float fadeOutDuration, Float fadeInDuration)
	{
		if (!world_ || !resource_ || !executor_ || transitionSystem_.IsTransitioning())
		{
			return;
		}

		transitionSystem_.LoadScene(*world_, *resource_, *executor_, targetScene, fadeOutDuration, fadeInDuration);
	}

	/**
	* [EN]
	* Begins an asynchronous switch to targetScene using a loading-scene
	* cover/reveal effect with the given durations.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 指定された長さのローディングシーンによる覆い隠し/表出エフェクトを
	* 使用して、targetScene への非同期切り替えを開始する。
	*/
	void Scene::Change(const std::filesystem::path& targetScene, const std::filesystem::path& loadingScene, Float coverDuration, Float revealDuration)
	{
		if (!world_ || !resource_ || !executor_ || transitionSystem_.IsTransitioning())
		{
			return;
		}

		transitionSystem_.LoadScene(*world_, *resource_, *executor_, targetScene, loadingScene, coverDuration, revealDuration);
	}

	/**
	* [EN]
	* Overload of the loading-scene cover/reveal Change resolving both
	* scenes from asset IDs.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* アセット ID から両方のシーンを解決する、ローディングシーン
	* 覆い隠し/表出版 Change のオーバーロード。
	*/
	void Scene::Change(Uint32 targetScene, Uint32 loadingScene, Float coverDuration, Float revealDuration)
	{
		if (!world_ || !resource_ || !executor_ || transitionSystem_.IsTransitioning())
		{
			return;
		}

		transitionSystem_.LoadScene(*world_, *resource_, *executor_, targetScene, loadingScene, coverDuration, revealDuration);
	}

	/**
	* [EN]
	* Returns the current fade overlay alpha of the process-wide scene transition.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* プロセス全体のシーン遷移における、現在のフェードオーバーレイの
	* アルファ値を返す。
	*/
	Float Scene::GetFadeAlpha()
	{
		return transitionSystem_.GetFadeAlpha();
	}

	/**
	* [EN]
	* Resolves path (project-root-relative, forward-slash) to its Asset
	* ID via the process-wide ResourceCache bound by Initialize(). Returns
	* 0 if not initialized or path is unknown. This is the sanctioned
	* entry point for gameplay code (SeedScript subclasses) to
	* dynamically look up an Asset by path at runtime;
	* Tools/Python/RuntimePackager.py statically scans source for calls
	* to this function to determine which Assets must be included in a
	* packaged build, so avoid calling it with anything other than a
	* string literal.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* path（プロジェクトルート相対、フォワードスラッシュ）を、
	* Initialize() で束縛されたプロセス全体の ResourceCache 経由で Asset ID
	* に解決する。未初期化または path が不明な場合は 0 を返す。ゲームプレイ
	* コード（SeedScript のサブクラス）が実行時にパスから Asset を動的に
	* 引くための正規の入口である。Tools/Python/RuntimePackager.py が
	* この関数への呼び出しをソースコードから静的にスキャンし、パッケージ
	* ビルドに含めるべき Asset を判定するため、文字列リテラル以外を渡すのは
	* 避けること。
	*/
	Uint32 Scene::GetAsset(const String& path)
	{
		if (!resource_)
		{
			return 0;
		}

		return resource_->GetAssetID(path);
	}

	/**
	* [EN]
	* Sets the raytracing settings blob written out alongside this
	* scene's actor data on the next Write().
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 次回の Write() でこのシーンの actor データと一緒に書き出される
	* レイトレーシング設定の blob を設定する。
	*/
	void Scene::SetRaytracingSettingsJson(const String& json)
	{
		raytracingSettingsJson_ = json;
	}

	/**
	* [EN]
	* Returns the raytracing settings blob captured by the last Read()/Load().
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 直近の Read()/Load() で取得したレイトレーシング設定の blob を返す。
	*/
	const String& Scene::GetRaytracingSettingsJson()const
	{
		return raytracingSettingsJson_;
	}
}
