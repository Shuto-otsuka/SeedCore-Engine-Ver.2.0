#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/World/Actor/Blueprint.h>

namespace SeedCore
{
	class World;
	class ResourceCache;
	class JobExecutor;
	class Actor;
	class SceneTransitionSystem;

	/**
	* [EN]
	* How a scene wants to be rendered, carried as the serialized form of
	* the raytracing, screen-space and rasterization contexts. The scene
	* stores them without looking inside: they are produced and read back
	* by the renderer, and only travel through here.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* その Scene をどう描くか。レイトレーシング、スクリーンスペース、
	* ラスタライゼーションの各コンテキストを文字列にした形で運ぶ。Scene は
	* 中身を見ずに保持する。作るのも読み戻すのもレンダラで、ここは通り道。
	*/
	struct SceneVisual
	{
		/// [EN] Raytracing context as text; empty when the scene never had one.
		/// [JP] レイトレーシングのコンテキストを文字列にしたもの。持っていない Scene では空。
		String raytracing_;

		/// [EN] Screen-space effect context as text; empty for scenes saved before it existed.
		/// [JP] スクリーンスペース系エフェクトのコンテキスト。この項目が存在する前に保存された Scene では空。
		String screenSpace_;

		/// [EN] Rasterization and SDF fallback context as text; empty for scenes saved before it existed.
		/// [JP] ラスタライゼーションと SDF フォールバックのコンテキスト。この項目が存在する前に保存された Scene では空。
		String rasterization_;
	};

	/**
	* [EN]
	* Serializable snapshot of every root Actor (and its descendants) in
	* a World, stored as a flat, parent-index-linked array of
	* BlueprintNode so it round-trips through JSON.
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
	* スナップショット。JSON で往復できるよう、親インデックスで
	* 連結された BlueprintNode のフラットな配列として保存される。
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
		* Discards any captured actor data, releasing the memory it held.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 取得済みの actor データを破棄し、それが保持していたメモリを解放する。
		*/
		void Clear();

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
		DynamicArray<Actor> Instantiate(World& world, ResourceCache& cache)const;

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

	public:
		/**
		* [EN]
		* Captures world's current state into a scratch Scene and writes it
		* to path, together with how the caller wants it rendered. Leaving
		* visual out saves a scene that carries nothing of its own.
		* Returns whether saving succeeded.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* world の現在の状態を作業用 Scene へ取得し、呼び出し側が指定する
		* 描き方と共に path へ書き込む。visual を省いた場合、その Scene は
		* 描き方を持たないものとして保存される。保存に成功したかどうかを
		* 返す。
		*/
		static Bool Save(World& world, ResourceCache& cache, const std::filesystem::path& path, const SceneVisual& visual = SceneVisual());

		/**
		* [EN]
		* Reads a scene from path into a scratch Scene, destroys every
		* actor currently in world, and instantiates the loaded scene.
		* When outVisual is given, how the scene wants to be rendered is
		* written there; parts a scene never carried come back empty.
		* Returns whether loading succeeded.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* path からシーンを作業用 Scene へ読み込み、world 内の現在の全
		* actor を破棄した上で、読み込んだシーンをインスタンス化する。
		* outVisual を渡した場合、その Scene をどう描くかをそこへ書き込む。
		* その Scene が元から持っていない項目は空で返る。読み込みに成功した
		* かどうかを返す。
		*/
		static Bool Load(World& world, ResourceCache& cache, const std::filesystem::path& path, SceneVisual* outVisual = nullptr);

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
		static Bool Load(World& world, ResourceCache& cache, Uint32 assetID, SceneVisual* outVisual = nullptr);

	public:
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
		* Aborts any in-progress process-wide scene transition and returns
		* it to Idle (see SceneTransitionSystem::Reset). Call when leaving
		* editor Play mode so a transition started during Play does not
		* resume afterward against stale actors.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 進行中のプロセス全体のシーン遷移を中断し、Idle へ戻す
		* （SceneTransitionSystem::Reset を参照）。エディタの Play モードを
		* 抜ける際に呼び、Play 中に開始された遷移がその後に古くなった
		* actor に対して再開しないようにする。
		*/
		static void Reset();

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
		static Float FadeAlpha();

		/**
		* [EN]
		* Returns the scene the process-wide transition has switched to
		* since the last call, or nullptr if none, clearing that state in
		* the same step (see SceneTransitionSystem::ConsumeSwitchedScene).
		* Lets the host that owns the main loop pick up whatever it keeps
		* per scene - the scene's own contents are already instantiated
		* into the world. Loading scenes shown during a transition are never
		* returned. The returned scene stays valid until the next transition
		* begins.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 前回の呼び出し以降にプロセス全体の遷移が切り替えた先のシーンを
		* 返す。無ければ nullptr。同時にその状態を取り下げる
		* (SceneTransitionSystem::ConsumeSwitchedScene 参照)。メインループを
		* 所有するホストが、シーンごとに持つものを受け取るために使う -
		* シーン自体の中身は既に world へインスタンス化済み。遷移中に表示
		* されるローディングシーンは返さない。返したシーンは次の遷移が
		* 始まるまで有効。
		*/
		[[nodiscard]] static const Scene* ConsumeSwitchedScene();

		/**
		* [EN]
		* Resolves path (project-root-relative, forward-slash) to its
		* AssetRecord ID via the process-wide ResourceCache bound by
		* Initialize(). Returns 0 if not initialized or path is unknown.
		* This is the sanctioned entry point for gameplay code (SeedScript
		* subclasses) to dynamically look up an AssetRecord by path at runtime;
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
		* AssetRecord ID に解決する。未初期化または path が不明な場合は 0 を返す。
		* ゲームプレイコード（SeedScript のサブクラス）が実行時にパスから
		* AssetRecord を動的に引くための正規の入口である。
		* Tools/Python/RuntimePackager.py がこの関数への呼び出しをソース
		* コードから静的にスキャンし、パッケージビルドに含めるべき AssetRecord を
		* 判定するため、文字列リテラル以外を渡すのは避けること。
		*/
		static Uint32 AssetID(const String& path);

	public:
		/**
		* [EN]
		* Returns how this scene wants to be rendered, as captured by the
		* last Read()/Load(). Scene itself knows nothing about what these
		* strings hold - FoundationEngine cannot depend on GraphicsEngine -
		* so the renderer is what produces and reads them back.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* この Scene をどう描くかを、直近の Read()/Load() で取得した内容
		* として返す。Scene 自体はこの文字列の中身を知らない
		* （FoundationEngine は GraphicsEngine に依存できない）ため、作るのも
		* 読み戻すのもレンダラ側になる。
		*/
		[[nodiscard]] const SceneVisual& Visual()const;

		/**
		* [EN]
		* Returns the captured actors of the last Read()/Capture(), in the
		* order they were laid out. Used when a scene that arrived from
		* another member has to be matched against the actors already
		* live in the world rather than instantiated from scratch.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 直近の Read()/Capture() で取得した actor を、並んでいた順のまま
		* 返す。他のメンバーから届いた Scene を、新規にインスタンス化する
		* のではなく、既に world にいる actor と突き合わせる際に使う。
		*/
		[[nodiscard]] const DynamicArray<BlueprintNode>& Nodes()const;

	public:
		/**
		* [EN]
		* Serialization hook (save side): writes nodes_ and how the scene
		* wants to be rendered.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* シリアライズ用フック(保存側): nodes_ と、その Scene をどう描くかを
		* 書き込む。
		*/
		template<class Archive>
		void Save(Archive& archive)const
		{
			archive.Field("nodes", nodes_);
			/// [EN] The keys stay as they were written before, so a scene saved by an older build still loads.
			/// [JP] 見出しは以前書かれていたものを保つ。古いビルドが保存した Scene も読めるようにするため。
			archive.Field("raytracingSettings", visual_.raytracing_);
			archive.Field("screenSpaceSettings", visual_.screenSpace_);
			archive.Field("rasterizationSettings", visual_.rasterization_);
		}

		/**
		* [EN]
		* Serialization hook (load side): reads nodes_ and the three
		* graphics-settings blobs. Each blob is optional -- scenes saved
		* before a given field existed simply leave it empty -- so a
		* missing key is swallowed rather than failing the whole load.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* シリアライズ用フック(読み込み側): nodes_ と 3 つのグラフィックス設定
		* blob を読み込む。各 blob は任意項目 -- そのフィールドが存在する前に
		* 保存されたシーンでは単に空になる -- なので、キーが無くても読み込み
		* 全体を失敗させず読み飛ばす。
		*/
		template<class Archive>
		void Load(Archive& archive)
		{
			archive.TryField("nodes", nodes_);
			archive.TryField("raytracingSettings", visual_.raytracing_);
			archive.TryField("screenSpaceSettings", visual_.screenSpace_);
			archive.TryField("rasterizationSettings", visual_.rasterization_);
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
		DynamicArray<BlueprintNode> nodes_;

		/// [EN] How this scene wants to be rendered, round-tripped by Save()/Load(). Empty parts mean the scene never carried them.
		/// [JP] この Scene をどう描くか。Save()/Load() で往復する。空の項目は、その Scene が元から持っていなかったことを表す。
		SceneVisual visual_;
	};
}
