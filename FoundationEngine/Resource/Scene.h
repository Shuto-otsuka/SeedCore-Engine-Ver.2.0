#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/Resource/ActorSerialization.h>

namespace SeedCore
{
	class World;
	class ResourceCache;
	class JobExecutor;
	class Actor;
	class SceneTransitionSystem;

	/**
	* [EN]
	* Serializable snapshot of every root Actor (and its descendants) in
	* a World, stored as a flat, parent-index-linked array of
	* SerializedActorNode so it round-trips through Cereal JSON.
	* Capture()/Instantiate() handle a single scene's data; the static
	* Change()/Update()/Initialize() surface additionally drives the
	* process-wide SceneTransitionSystem, so callers can trigger
	* scene switches (with optional fade/loading-scene effects) from
	* anywhere without holding their own World/ResourceCache/JobExecutor references.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* World 内の全ルート Actor（とその子孫）のシリアライズ可能な
	* スナップショット。Cereal JSON で往復できるよう、親インデックスで
	* 連結された SerializedActorNode のフラットな配列として保存される。
	* Capture()/Instantiate() は単一シーンのデータを扱う。静的な
	* Change()/Update()/Initialize() のインターフェースは、加えて
	* プロセス全体の SceneTransitionSystem を駆動するため、呼び出し側は
	* 自前の World/ResourceCache/JobExecutor 参照を持たずとも、どこからでも
	* シーン切り替え（任意でフェード/ローディングシーンエフェクト付き）を
	* トリガーできる。
	*/
	class SEEDCORE_API Scene
	{
	public:
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
		void Capture(World& world);

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
		DynamicArray<Actor*> Instantiate(World& world, ResourceCache& cache)const;

		/**
		* [EN]
		* Writes this scene's captured data to path as JSON. Returns
		* whether the file was written successfully.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* このシーンの取得済みデータを、JSON として path へ書き込む。
		* ファイルが正常に書き込まれたかどうかを返す。
		*/
		Bool Write(const std::filesystem::path& path);

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
		Bool Read(const std::filesystem::path& path);

		/**
		* [EN]
		* Captures world's current state into a scratch Scene and writes
		* it to path, along with raytracingSettingsJson (an opaque blob;
		* pass an empty String if the caller has no raytracing settings to
		* persist). Returns whether saving succeeded.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* world の現在の状態を作業用 Scene へ取得し、raytracingSettingsJson
		* (不透明な blob。永続化するレイトレーシング設定が無い呼び出し側は
		* 空の String を渡す)と共に path へ書き込む。保存に成功したかどうかを
		* 返す。
		*/
		static Bool Save(World& world, ResourceCache& cache, const std::filesystem::path& path, const String& raytracingSettingsJson = String());

		/**
		* [EN]
		* Reads a scene from path into a scratch Scene, destroys every
		* actor currently in world, and instantiates the loaded scene. If
		* outRaytracingSettingsJson is non-null, the scene's raytracing
		* settings blob (possibly empty, for scenes saved before this
		* field existed) is written to it. Returns whether loading
		* succeeded.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* path からシーンを作業用 Scene へ読み込み、world 内の現在の全
		* actor を破棄した上で、読み込んだシーンをインスタンス化する。
		* outRaytracingSettingsJson が非null の場合、シーンのレイトレーシング
		* 設定 blob(このフィールドが存在する前に保存されたシーンでは
		* 空になりうる)をそこへ書き込む。読み込みに成功したかどうかを返す。
		*/
		static Bool Load(World& world, ResourceCache& cache, const std::filesystem::path& path, String* outRaytracingSettingsJson = nullptr);

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
		static Bool Load(World& world, ResourceCache& cache, Uint32 assetID, String* outRaytracingSettingsJson = nullptr);

		/**
		* [EN]
		* Binds the process-wide world/cache/executor references used by
		* the static Change()/Update() overloads. Must be called once
		* before using them.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 静的な Change()/Update() オーバーロードが使用する、プロセス
		* 全体の world/cache/executor 参照を束縛する。それらを使用する前に
		* 一度呼び出す必要がある。
		*/
		static void Initialize(World& world, ResourceCache& cache, JobExecutor& executor);

		/**
		* [EN]
		* Advances the process-wide scene transition state machine by deltaTime.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* プロセス全体のシーン遷移状態機械を deltaTime だけ進める。
		*/
		static void Update(Float deltaTime);

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
		static Bool Change(const std::filesystem::path& targetScene);

		/**
		* [EN]
		* Overload of Change resolving targetScene from an asset ID.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* アセット ID から targetScene を解決する Change のオーバーロード。
		*/
		static Bool Change(Uint32 targetScene);

		/**
		* [EN]
		* Begins an asynchronous switch to targetScene, immediately
		* showing loadingScene while targetScene loads in the background.
		* No-op if not initialized or a transition is already in progress.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* targetScene への非同期切り替えを開始する。targetScene が
		* バックグラウンドで読み込まれている間、loadingScene を即座に
		* 表示する。未初期化であるか、既に遷移が進行中であれば何もしない。
		*/
		static void Change(const std::filesystem::path& targetScene, const std::filesystem::path& loadingScene);

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
		static void Change(Uint32 targetScene, Uint32 loadingScene);

		/**
		* [EN]
		* Begins an asynchronous switch to targetScene using a
		* fade-out/fade-in effect with the given durations.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 指定された長さのフェードアウト/フェードインエフェクトを使用して、
		* targetScene への非同期切り替えを開始する。
		*/
		static void Change(const std::filesystem::path& targetScene, Float fadeOutDuration, Float fadeInDuration);

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
		static void Change(Uint32 targetScene, Float fadeOutDuration, Float fadeInDuration);

		/**
		* [EN]
		* Begins an asynchronous switch to targetScene using a
		* loading-scene cover/reveal effect with the given durations.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 指定された長さのローディングシーンによる覆い隠し/表出
		* エフェクトを使用して、targetScene への非同期切り替えを開始する。
		*/
		static void Change(const std::filesystem::path& targetScene, const std::filesystem::path& loadingScene, Float coverDuration, Float revealDuration);

		/**
		* [EN]
		* Overload of the loading-scene cover/reveal Change resolving
		* both scenes from asset IDs.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* アセット ID から両方のシーンを解決する、ローディングシーン
		* 覆い隠し/表出版 Change のオーバーロード。
		*/
		static void Change(Uint32 targetScene, Uint32 loadingScene, Float coverDuration, Float revealDuration);

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
		static Float GetFadeAlpha();

		/**
		* [EN]
		* Resolves path (project-root-relative, forward-slash) to its
		* Asset ID via the process-wide ResourceCache bound by
		* Initialize(). Returns 0 if not initialized or path is unknown.
		* This is the sanctioned entry point for gameplay code (SeedScript
		* subclasses) to dynamically look up an Asset by path at runtime;
		* Tools/Python/RuntimePackager.py statically scans source for
		* calls to this function to determine which Assets must be
		* included in a packaged build, so avoid calling it with anything
		* other than a string literal.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* path（プロジェクトルート相対、フォワードスラッシュ）を、
		* Initialize() で束縛されたプロセス全体の ResourceCache 経由で
		* Asset ID に解決する。未初期化または path が不明な場合は 0 を返す。
		* ゲームプレイコード（SeedScript のサブクラス）が実行時にパスから
		* Asset を動的に引くための正規の入口である。
		* Tools/Python/RuntimePackager.py がこの関数への呼び出しをソース
		* コードから静的にスキャンし、パッケージビルドに含めるべき Asset を
		* 判定するため、文字列リテラル以外を渡すのは避けること。
		*/
		static Uint32 GetAsset(const String& path);

		/**
		* [EN]
		* Sets the raytracing settings blob written out alongside this
		* scene's actor data on the next Write(). Scene itself has no
		* concept of raytracing settings (FoundationEngine cannot depend
		* on GraphicsEngine), so callers in higher layers pass it in as an
		* opaque JSON string -- see GraphicsEngine's
		* SerializeRaytracingContext/DeserializeRaytracingContext.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 次回の Write() でこのシーンの actor データと一緒に書き出される
		* レイトレーシング設定の blob を設定する。Scene 自体はレイトレーシング
		* 設定について何も知らない（FoundationEngine は GraphicsEngine に
		* 依存できない）ため、上位レイヤーの呼び出し側が不透明な JSON
		* 文字列として渡す -- GraphicsEngine の
		* SerializeRaytracingContext/DeserializeRaytracingContext を参照。
		*/
		void SetRaytracingSettingsJson(const String& json);

		/**
		* [EN]
		* Returns the raytracing settings blob captured by the last Read()/Load().
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 直近の Read()/Load() で取得したレイトレーシング設定の blob を返す。
		*/
		[[nodiscard]] const String& GetRaytracingSettingsJson()const;

		/**
		* [EN]
		* Cereal serialization hook (save side): writes nodes_ and raytracingSettingsJson_.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* Cereal シリアライズ用フック(保存側): nodes_ と raytracingSettingsJson_ を書き込む。
		*/
		template<class Archive>
		void save(Archive& archive)const
		{
			archive(cereal::make_nvp("nodes", nodes_));
			archive(cereal::make_nvp("raytracingSettings", raytracingSettingsJson_));
		}

		/**
		* [EN]
		* Cereal serialization hook (load side): reads nodes_ and
		* raytracingSettingsJson_. raytracingSettingsJson_ is optional --
		* scenes saved before this field existed simply leave it empty --
		* so a missing key is swallowed rather than failing the whole load.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* Cereal シリアライズ用フック(読み込み側): nodes_ と
		* raytracingSettingsJson_ を読み込む。raytracingSettingsJson_ は
		* 任意項目 -- このフィールドが存在する前に保存されたシーンでは単に
		* 空になる -- なので、キーが無くても読み込み全体を失敗させず読み飛ばす。
		*/
		template<class Archive>
		void load(Archive& archive)
		{
			archive(cereal::make_nvp("nodes", nodes_));

			const Char* nextName = archive.getNodeName();
			if (nextName && std::strcmp(nextName, "raytracingSettings") == 0)
			{
				archive(cereal::make_nvp("raytracingSettings", raytracingSettingsJson_));
			}
			else
			{
				raytracingSettingsJson_ = String();
			}
		}

	private:
		/// [EN] Process-wide World reference bound by Initialize, used by the static Change()/Update() overloads.
		/// [JP] Initialize によって束縛される、プロセス全体の World 参照。静的な Change()/Update() オーバーロードが使用する。
		static World* world_;

		/// [EN] Process-wide ResourceCache reference bound by Initialize.
		/// [JP] Initialize によって束縛される、プロセス全体の ResourceCache 参照。
		static ResourceCache* resource_;

		/// [EN] Process-wide JobExecutor reference bound by Initialize, used for background-loaded transitions.
		/// [JP] Initialize によって束縛される、プロセス全体の JobExecutor 参照。バックグラウンド読み込みを伴う遷移に使われる。
		static JobExecutor* executor_;

		/// [EN] Process-wide scene transition state machine driving the static Change()/Update() overloads.
		/// [JP] 静的な Change()/Update() オーバーロードを駆動する、プロセス全体のシーン遷移状態機械。
		static SceneTransitionSystem transitionSystem_;

		/// [EN] Flat, parent-index-linked array of every captured root actor and its descendants.
		/// [JP] 取得済みの全ルート actor とその子孫の、親インデックスで連結されたフラットな配列。
		DynamicArray<SerializedActorNode> nodes_;

		/// [EN] Opaque JSON blob holding raytracing settings, set by SetRaytracingSettingsJson and round-tripped by save()/load(). Empty if never set.
		/// [JP] レイトレーシング設定を保持する不透明な JSON blob。SetRaytracingSettingsJson で設定され、save()/load() で往復する。未設定なら空。
		String raytracingSettingsJson_;
	};
}
