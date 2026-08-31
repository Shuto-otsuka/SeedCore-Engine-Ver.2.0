#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/Resource/Scene.h>
#include <FoundationEngine/JobSystem/JobTaskflow.h>

namespace SeedCore
{
	class World;
	class ResourceCache;
	class JobExecutor;
	class Actor;

	/**
	* [EN]
	* Drives scene transitions: from an immediate synchronous LoadScene,
	* up through background-loaded transitions with a fade-out/fade-in
	* effect or a "loading scene" cover/reveal effect. Background loads
	* run on a JobExecutor via a small JobTaskflow (pendingFlow_) so the
	* target scene's file I/O doesn't block the calling thread; Update
	* must be called every frame to advance the transition's internal
	* state machine.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* シーン遷移を統括するクラス: 即時同期の LoadScene から、
	* バックグラウンド読み込みを伴うフェードアウト/フェードイン
	* エフェクト、または「ローディングシーン」による覆い隠し/表出
	* エフェクトまでを扱う。バックグラウンド読み込みは、呼び出し元
	* スレッドをブロックしないよう、小さな JobTaskflow（pendingFlow_）
	* 経由で JobExecutor 上で実行される。遷移の内部状態機械を進めるには、
	* Update を毎フレーム呼び出す必要がある。
	*/
	class SceneTransitionSystem
	{
	public:
		/**
		* [EN]
		* Synchronously loads targetScene by path into world, replacing
		* its current contents. Returns whether loading succeeded.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* パスで指定された targetScene を同期的に world へ読み込み、現在の
		* 内容を置き換える。読み込みに成功したかどうかを返す。
		*/
		Bool LoadScene(World& world, ResourceCache& cache, const std::filesystem::path& targetScene);

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
		Bool LoadScene(World& world, ResourceCache& cache, Uint32 targetScene);

		/**
		* [EN]
		* Begins an asynchronous transition to targetScene: immediately
		* swaps in loadingScene, starts background-loading targetScene,
		* and enters WaitingBackground (swapped in once ready via Update).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* targetScene への非同期遷移を開始する: loadingScene を即座に
		* 切り替え、targetScene のバックグラウンド読み込みを開始し、
		* WaitingBackground 状態に入る（準備完了後 Update 経由で
		* 切り替えられる）。
		*/
		void LoadScene(World& world, ResourceCache& cache, JobExecutor& executor, const std::filesystem::path& targetScene, const std::filesystem::path& loadingScene);

		/**
		* [EN]
		* Overload of the loading-scene LoadScene resolving both scenes
		* from asset IDs via cache.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* cache 経由でアセット ID から両方のシーンを解決する、
		* ローディングシーン版 LoadScene のオーバーロード。
		*/
		void LoadScene(World& world, ResourceCache& cache, JobExecutor& executor, Uint32 targetScene, Uint32 loadingScene);

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
		void LoadScene(World& world, ResourceCache& cache, JobExecutor& executor, const std::filesystem::path& targetScene, Float fadeOutDuration, Float fadeInDuration);

		/**
		* [EN]
		* Overload of the fade-effect LoadScene resolving targetScene
		* from an asset ID via cache.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* cache 経由でアセット ID から targetScene を解決する、
		* フェードエフェクト版 LoadScene のオーバーロード。
		*/
		void LoadScene(World& world, ResourceCache& cache, JobExecutor& executor, Uint32 targetScene, Float fadeOutDuration, Float fadeInDuration);

		/**
		* [EN]
		* Begins an asynchronous transition to targetScene using a
		* loading-scene cover/reveal effect: instantiates loadingScene
		* over the current scene, starts background-loading targetScene,
		* and enters CoveringLoad (the previous scene's
		* actors are destroyed once the cover finishes, the target scene
		* is instantiated once loading finishes, and the loading scene's
		* actors are destroyed once the reveal finishes).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* ローディングシーンによる覆い隠し/表出エフェクトを使用して
		* targetScene への非同期遷移を開始する: loadingScene を現在の
		* シーンの上にインスタンス化し、targetScene のバックグラウンド
		* 読み込みを開始し、CoveringLoad 状態に入る
		* （覆い隠しが完了した時点で以前のシーンの actor が破棄され、
		* 読み込みが完了した時点でターゲットシーンがインスタンス化され、
		* 表出が完了した時点でローディングシーンの actor が破棄される）。
		*/
		void LoadScene(World& world, ResourceCache& cache, JobExecutor& executor, const std::filesystem::path& targetScene, const std::filesystem::path& loadingScene, Float coverDuration, Float revealDuration);

		/**
		* [EN]
		* Overload of the loading-scene cover/reveal LoadScene resolving
		* both scenes from asset IDs via cache.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* cache 経由でアセット ID から両方のシーンを解決する、
		* ローディングシーン覆い隠し/表出版 LoadScene のオーバーロード。
		*/
		void LoadScene(World& world, ResourceCache& cache, JobExecutor& executor, Uint32 targetScene, Uint32 loadingScene, Float coverDuration, Float revealDuration);

		/**
		* [EN]
		* Advances the transition state machine by deltaTime, performing
		* the scene swap and/or destroying stale actors when the current
		* phase completes.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 遷移の状態機械を deltaTime だけ進め、現在のフェーズが完了した
		* 時点でシーンの切り替えや不要になった actor の破棄を行う。
		*/
		void Update(World& world, ResourceCache& cache, Float deltaTime);

		/**
		* [EN]
		* Returns whether a transition is currently in progress.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 現在遷移が進行中かどうかを返す。
		*/
		Bool IsTransitioning()const;

		/**
		* [EN]
		* Aborts any in-progress transition and returns the state machine
		* to Idle: waits out a pending background load, drops the retained
		* previous/loading-scene actor pointers without destroying them
		* (the caller is expected to be rebuilding the world), and clears
		* all fade/timer state. Used when leaving editor Play mode so a
		* transition started during Play does not resume afterward against
		* stale actors.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 進行中の遷移を中断し、状態機械を Idle へ戻す: 進行中の
		* バックグラウンド読み込みを待ち切り、保持していた
		* previous/loading シーンの actor ポインタを破棄せずに手放し
		* （呼び出し側が world を再構築する想定）、フェード/タイマーの
		* 状態を全てクリアする。エディタの Play モードを抜ける際に、
		* Play 中に開始された遷移がその後に古くなった actor に対して
		* 再開しないようにするために使う。
		*/
		void Reset();

		/**
		* [EN]
		* Returns the current fade overlay alpha (0 = fully visible scene, 1 = fully covered).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 現在のフェードオーバーレイのアルファ値を返す（0 = シーンが
		* 完全に見える状態、1 = 完全に覆われた状態）。
		*/
		Float GetFadeAlpha()const;

	private:
		/**
		* [EN]
		* The transition's current phase.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 遷移の現在のフェーズ。
		*/
		enum class State
		{
			/// [EN] No transition in progress.
			/// [JP] 遷移は進行していない。
			Idle,

			/// [EN] A background load has been started; waiting for it to finish before swapping scenes.
			/// [JP] バックグラウンド読み込みが開始されている。シーンを切り替える前に、その完了を待っている。
			WaitingBackground,

			/// [EN] Fading the current scene out to fully covered.
			/// [JP] 現在のシーンを、完全に覆われた状態までフェードアウトしている。
			FadingOut,

			/// [EN] Fading the newly-swapped-in scene back in to fully visible.
			/// [JP] 新しく切り替えられたシーンを、完全に見える状態までフェードインしている。
			FadingIn,

			/// [EN] Waiting for the cover duration to elapse before destroying the previous scene's actors.
			/// [JP] 以前のシーンの actor を破棄する前に、覆い隠し期間が経過するのを待っている。
			CoveringLoad,

			/// [EN] Waiting (with the loading scene shown) for the background load to finish.
			/// [JP] （ローディングシーンを表示したまま）バックグラウンド読み込みの完了を待っている。
			WaitingLoading,

			/// [EN] Waiting for the reveal duration to elapse before destroying the loading scene's actors.
			/// [JP] ローディングシーンの actor を破棄する前に、表出期間が経過するのを待っている。
			Revealing,
		};

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
		void BackgroundLoad(ResourceCache& cache, JobExecutor& executor, const std::filesystem::path& path);

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
		void SwapLoad(World& world, ResourceCache& cache);

	private:
		/// [EN] The transition's current phase.
		/// [JP] 遷移の現在のフェーズ。
		State state_ = State::Idle;

		/// [EN] The scene data being loaded in the background.
		/// [JP] バックグラウンドで読み込まれているシーンデータ。
		Scene pendingScene_;

		/// [EN] The small taskflow that performs the background scene load.
		/// [JP] バックグラウンドでのシーン読み込みを実行する、小さなタスクフロー。
		JobTaskflow pendingFlow_;

		/// [EN] Future tracking completion of the current background load.
		/// [JP] 現在のバックグラウンド読み込みの完了を追跡する future。
		JobFuture<void> pendingLoad_;

		/// [EN] Whether the most recent background load completed successfully.
		/// [JP] 直近のバックグラウンド読み込みが正常に完了したかどうか。
		Bool pendingLoadSucceeded_ = false;

		/// [EN] Current fade overlay alpha (0 = fully visible, 1 = fully covered).
		/// [JP] 現在のフェードオーバーレイのアルファ値（0 = 完全に見える、1 = 完全に覆われている）。
		Float fadeAlpha_ = 0.0f;

		/// [EN] Elapsed time within the current fade phase (FadingOut or FadingIn).
		/// [JP] 現在のフェードフェーズ（FadingOut または FadingIn）内での経過時間。
		Float fadeTimer_ = 0.0f;

		/// [EN] Configured duration of the fade-out phase.
		/// [JP] 設定されたフェードアウトフェーズの長さ。
		Float fadeOutDuration_ = 0.3f;

		/// [EN] Configured duration of the fade-in phase.
		/// [JP] 設定されたフェードインフェーズの長さ。
		Float fadeInDuration_ = 0.3f;

		/// [EN] Actors from the previous scene, kept alive until CoveringLoad finishes.
		/// [JP] 以前のシーンの actor 群。CoveringLoad が完了するまで保持される。
		DynamicArray<Actor> previousActors_;

		/// [EN] Actors instantiated from the loading scene, kept alive until Revealing finishes.
		/// [JP] ローディングシーンからインスタンス化された actor 群。Revealing が完了するまで保持される。
		DynamicArray<Actor> loadingSceneActors_;

		/// [EN] Elapsed time within the current cover/reveal phase.
		/// [JP] 現在の覆い隠し/表出フェーズ内での経過時間。
		Float transitionTimer_ = 0.0f;

		/// [EN] Configured duration of the cover phase.
		/// [JP] 設定された覆い隠しフェーズの長さ。
		Float coverDuration_ = 0.3f;

		/// [EN] Configured duration of the reveal phase.
		/// [JP] 設定された表出フェーズの長さ。
		Float revealDuration_ = 0.3f;
	};
}
