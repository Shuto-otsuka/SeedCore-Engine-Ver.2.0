#include <FoundationEngine/ECS/System/SceneTransitionSystem.h>
#include <FoundationEngine/Resource/ResourceCache.h>
#include <FoundationEngine/ECS/World.h>
#include <FoundationEngine/JobSystem/JobExecutor.h>
#include <FoundationEngine/Math/Algorithm.h>

namespace SeedCore
{
	/**
	* [EN]
	* Synchronously loads targetScene by path into world, replacing its
	* current contents. Returns whether loading succeeded.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* パスで指定された targetScene を同期的に world へ読み込み、現在の
	* 内容を置き換える。読み込みに成功したかどうかを返す。
	*/
	Bool SceneTransitionSystem::LoadScene(World& world, ResourceCache& cache, const std::filesystem::path& targetScene)
	{
		return Scene::Load(world, cache, targetScene);
	}

	/**
	* [EN]
	* Overload of LoadScene resolving targetScene from an asset ID via cache.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* cache 経由でアセット ID から targetScene を解決する LoadScene の
	* オーバーロード。
	*/
	Bool SceneTransitionSystem::LoadScene(World& world, ResourceCache& cache, Uint32 targetScene)
	{
		return Scene::Load(world, cache, targetScene);
	}

	/**
	* [EN]
	* Begins an asynchronous transition to targetScene: immediately
	* swaps in loadingScene, starts background-loading targetScene, and
	* enters WaitingForBackgroundLoad (swapped in once ready via Update).
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* targetScene への非同期遷移を開始する: loadingScene を即座に
	* 切り替え、targetScene のバックグラウンド読み込みを開始し、
	* WaitingForBackgroundLoad 状態に入る（準備完了後 Update 経由で
	* 切り替えられる）。
	*/
	void SceneTransitionSystem::LoadScene(World& world, ResourceCache& cache, JobExecutor& executor, const std::filesystem::path& targetScene, const std::filesystem::path& loadingScene)
	{
		Scene::Load(world, cache, loadingScene);

		BeginBackgroundLoad(cache, executor, targetScene);
		state_ = State::WaitingForBackgroundLoad;
	}

	/**
	* [EN]
	* Overload of the loading-scene LoadScene resolving both scenes from
	* asset IDs via cache.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* cache 経由でアセット ID から両方のシーンを解決する、ローディング
	* シーン版 LoadScene のオーバーロード。
	*/
	void SceneTransitionSystem::LoadScene(World& world, ResourceCache& cache, JobExecutor& executor, Uint32 targetScene, Uint32 loadingScene)
	{
		Asset* targetAsset = cache.GetAsset(targetScene);
		Asset* loadingAsset = cache.GetAsset(loadingScene);
		if (!targetAsset || !loadingAsset)
		{
			return;
		}

		LoadScene(world, cache, executor, std::filesystem::path(targetAsset->fullpath_.c_str()), std::filesystem::path(loadingAsset->fullpath_.c_str()));
	}

	/**
	* [EN]
	* Begins an asynchronous transition to targetScene using a
	* fade-out/fade-in effect: starts background-loading targetScene
	* and enters FadingOut (the actual scene swap happens once the
	* fade-out completes and the load is ready, then fades back in).
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* フェードアウト/フェードインエフェクトを使用して targetScene への
	* 非同期遷移を開始する: targetScene のバックグラウンド読み込みを
	* 開始し、FadingOut 状態に入る（実際のシーン切り替えはフェード
	* アウトが完了し読み込みの準備ができた時点で行われ、その後
	* フェードインする）。
	*/
	void SceneTransitionSystem::LoadScene(World& world, ResourceCache& cache, JobExecutor& executor, const std::filesystem::path& targetScene, Float fadeOutDuration, Float fadeInDuration)
	{
		fadeOutDuration_ = fadeOutDuration;
		fadeInDuration_ = fadeInDuration;
		fadeTimer_ = 0.0f;
		fadeAlpha_ = 0.0f;

		BeginBackgroundLoad(cache, executor, targetScene);
		state_ = State::FadingOut;
	}

	/**
	* [EN]
	* Overload of the fade-effect LoadScene resolving targetScene from
	* an asset ID via cache.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* cache 経由でアセット ID から targetScene を解決する、フェード
	* エフェクト版 LoadScene のオーバーロード。
	*/
	void SceneTransitionSystem::LoadScene(World& world, ResourceCache& cache, JobExecutor& executor, Uint32 targetScene, Float fadeOutDuration, Float fadeInDuration)
	{
		Asset* targetAsset = cache.GetAsset(targetScene);
		if (!targetAsset)
		{
			return;
		}

		LoadScene(world, cache, executor, std::filesystem::path(targetAsset->fullpath_.c_str()), fadeOutDuration, fadeInDuration);
	}

	/**
	* [EN]
	* Begins an asynchronous transition to targetScene using a
	* loading-scene cover/reveal effect: instantiates loadingScene over
	* the current scene, starts background-loading targetScene, and
	* enters CoveringWithLoadingScene.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* ローディングシーンによる覆い隠し/表出エフェクトを使用して
	* targetScene への非同期遷移を開始する: loadingScene を現在の
	* シーンの上にインスタンス化し、targetScene のバックグラウンド
	* 読み込みを開始し、CoveringWithLoadingScene 状態に入る。
	*/
	void SceneTransitionSystem::LoadScene(World& world, ResourceCache& cache, JobExecutor& executor, const std::filesystem::path& targetScene, const std::filesystem::path& loadingScene, Float coverDuration, Float revealDuration)
	{
		coverDuration_ = coverDuration;
		revealDuration_ = revealDuration;
		transitionTimer_ = 0.0f;

		/// [EN] Snapshot the current scene's actors so they can be destroyed once the cover phase finishes (not immediately, to avoid a visible pop).
		/// [JP] 現在のシーンの actor をスナップショットしておく。覆い隠しフェーズが完了した時点で破棄できるようにするため（見た目の飛びを避けるため即座には破棄しない）。
		previousActors_.clear();
		for (auto& actor : world.GetActors())
		{
			previousActors_.push_back(actor.get());
		}

		std::filesystem::path resolvedLoadingScene = loadingScene;
		Uint32 loadingSceneAssetID = cache.GetAssetID(String(loadingScene.string()));
		if (loadingSceneAssetID != 0)
		{
			Asset* loadingSceneAsset = cache.GetAsset(loadingSceneAssetID);
			if (loadingSceneAsset)
			{
				resolvedLoadingScene = std::filesystem::path(loadingSceneAsset->fullpath_.c_str());
			}
		}

		Scene loadingSceneObj;
		loadingSceneActors_.clear();
		if (loadingSceneObj.Read(resolvedLoadingScene))
		{
			loadingSceneActors_ = loadingSceneObj.Instantiate(world, cache);
		}

		BeginBackgroundLoad(cache, executor, targetScene);
		state_ = State::CoveringWithLoadingScene;
	}

	/**
	* [EN]
	* Overload of the loading-scene cover/reveal LoadScene resolving
	* both scenes from asset IDs via cache.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* cache 経由でアセット ID から両方のシーンを解決する、ローディング
	* シーン覆い隠し/表出版 LoadScene のオーバーロード。
	*/
	void SceneTransitionSystem::LoadScene(World& world, ResourceCache& cache, JobExecutor& executor, Uint32 targetScene, Uint32 loadingScene, Float coverDuration, Float revealDuration)
	{
		Asset* targetAsset = cache.GetAsset(targetScene);
		Asset* loadingAsset = cache.GetAsset(loadingScene);
		if (!targetAsset || !loadingAsset)
		{
			return;
		}

		LoadScene(world, cache, executor, std::filesystem::path(targetAsset->fullpath_.c_str()), std::filesystem::path(loadingAsset->fullpath_.c_str()), coverDuration, revealDuration);
	}

	/**
	* [EN]
	* Advances the transition state machine by deltaTime, performing the
	* scene swap and/or destroying stale actors when the current phase completes.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 遷移の状態機械を deltaTime だけ進め、現在のフェーズが完了した
	* 時点でシーンの切り替えや不要になった actor の破棄を行う。
	*/
	void SceneTransitionSystem::Update(World& world, ResourceCache& cache, Float deltaTime)
	{
		switch (state_)
		{
		case State::WaitingForBackgroundLoad:
			if (IsBackgroundLoadReady())
			{
				SwapToLoadedScene(world, cache);
				state_ = State::Idle;
			}
			break;

		case State::FadingOut:
			fadeTimer_ += deltaTime;
			fadeAlpha_ = fadeOutDuration_ > 0.0f ? Clamp(fadeTimer_ / fadeOutDuration_, 0.0f, 1.0f) : 1.0f;

			if (fadeAlpha_ >= 1.0f && IsBackgroundLoadReady())
			{
				SwapToLoadedScene(world, cache);
				fadeTimer_ = 0.0f;
				state_ = State::FadingIn;
			}
			break;

		case State::FadingIn:
			fadeTimer_ += deltaTime;
			fadeAlpha_ = fadeInDuration_ > 0.0f ? 1.0f - Clamp(fadeTimer_ / fadeInDuration_, 0.0f, 1.0f) : 0.0f;

			if (fadeTimer_ >= fadeInDuration_)
			{
				fadeAlpha_ = 0.0f;
				state_ = State::Idle;
			}
			break;

		case State::CoveringWithLoadingScene:
			transitionTimer_ += deltaTime;

			if (transitionTimer_ >= coverDuration_)
			{
				for (Actor* actor : previousActors_)
				{
					world.DestroyActor(actor);
				}
				previousActors_.clear();

				state_ = State::WaitingWithLoadingScene;
			}
			break;

		case State::WaitingWithLoadingScene:
			if (IsBackgroundLoadReady())
			{
				if (pendingLoadSucceeded_)
				{
					pendingScene_.Instantiate(world, cache);
				}

				transitionTimer_ = 0.0f;
				state_ = State::RevealingTarget;
			}
			break;

		case State::RevealingTarget:
			transitionTimer_ += deltaTime;

			if (transitionTimer_ >= revealDuration_)
			{
				for (Actor* actor : loadingSceneActors_)
				{
					world.DestroyActor(actor);
				}
				loadingSceneActors_.clear();

				state_ = State::Idle;
			}
			break;

		default:
			break;
		}
	}

	/**
	* [EN]
	* Returns whether a transition is currently in progress.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 現在遷移が進行中かどうかを返す。
	*/
	Bool SceneTransitionSystem::IsTransitioning()const
	{
		return state_ != State::Idle;
	}

	/**
	* [EN]
	* Aborts any in-progress transition and returns the state machine to
	* Idle: waits out a pending background load, drops the retained
	* previous/loading-scene actor pointers without destroying them (the
	* caller is expected to be rebuilding the world), and clears all
	* fade/timer state.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 進行中の遷移を中断し、状態機械を Idle へ戻す: 進行中の
	* バックグラウンド読み込みを待ち切り、保持していた
	* previous/loading シーンの actor ポインタを破棄せずに手放し
	* （呼び出し側が world を再構築する想定）、フェード/タイマーの
	* 状態を全てクリアする。
	*/
	void SceneTransitionSystem::Reset()
	{
		if (pendingLoad_.valid())
		{
			pendingLoad_.wait();
		}
		pendingFlow_.Clear();
		pendingLoadSucceeded_ = false;

		previousActors_.clear();
		loadingSceneActors_.clear();

		fadeAlpha_ = 0.0f;
		fadeTimer_ = 0.0f;
		transitionTimer_ = 0.0f;

		state_ = State::Idle;
	}

	/**
	* [EN]
	* Returns the current fade overlay alpha (0 = fully visible scene, 1 = fully covered).
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 現在のフェードオーバーレイのアルファ値を返す（0 = シーンが完全に
	* 見える状態、1 = 完全に覆われた状態）。
	*/
	Float SceneTransitionSystem::GetFadeAlpha()const
	{
		return fadeAlpha_;
	}

	/**
	* [EN]
	* Starts loading path in the background on executor via
	* pendingFlow_, resetting pendingLoadSucceeded_ and storing the
	* resulting future.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* pendingFlow_ 経由で executor 上において path のバックグラウンド
	* 読み込みを開始し、pendingLoadSucceeded_ をリセットして、結果の
	* future を保存する。path はまず cache でアセット名として解決される
	* （"Foo.scene" のような単なるファイル名でもよい）。
	*/
	void SceneTransitionSystem::BeginBackgroundLoad(ResourceCache& cache, JobExecutor& executor, const std::filesystem::path& path)
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

		pendingLoadSucceeded_ = false;

		pendingFlow_.Clear();
		pendingFlow_.emplace([this, resolvedPath]()
			{
				pendingLoadSucceeded_ = pendingScene_.Read(resolvedPath);
			});

		pendingLoad_ = executor.Run(pendingFlow_);
	}

	/**
	* [EN]
	* Returns whether the current background load (if any) has finished.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 現在のバックグラウンド読み込み（あれば）が完了しているかどうかを
	* 返す。
	*/
	Bool SceneTransitionSystem::IsBackgroundLoadReady()const
	{
		return pendingLoad_.valid() && pendingLoad_.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
	}

	/**
	* [EN]
	* If the background load succeeded, destroys every current actor
	* and instantiates pendingScene_ into world.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* バックグラウンド読み込みが成功していれば、現在の全 actor を
	* 破棄し、pendingScene_ を world へインスタンス化する。
	*/
	void SceneTransitionSystem::SwapToLoadedScene(World& world, ResourceCache& cache)
	{
		if (!pendingLoadSucceeded_)
		{
			return;
		}

		world.DestroyActors();
		pendingScene_.Instantiate(world, cache);
	}
}
