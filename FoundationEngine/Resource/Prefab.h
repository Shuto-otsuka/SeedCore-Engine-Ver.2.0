#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/ECS/ReflectionRegistry.h>
#include <FoundationEngine/Resource/ActorSerialization.h>

namespace SeedCore
{
	class Actor;
	class World;
	class ResourceCache;

	/// [EN] Alias for the reflected field data captured/restored per component; see SerializedField.
	/// [JP] コンポーネントごとに取得/復元される、リフレクションされたフィールドデータのエイリアス。SerializedField を参照。
	using PrefabComponentField = SerializedField;

	/// [EN] Alias for a single captured/restored component; see SerializedComponent.
	/// [JP] 取得/復元される単一のコンポーネントのエイリアス。SerializedComponent を参照。
	using PrefabComponent = SerializedComponent;

	/// [EN] Alias for a single captured/restored actor node (name, components, parent linkage); see SerializedActorNode.
	/// [JP] 取得/復元される単一の actor ノード（名前、コンポーネント、親への関連付け）のエイリアス。SerializedActorNode を参照。
	using PrefabNode = SerializedActorNode;

	/**
	* [EN]
	* Serializable snapshot of an Actor subtree (root plus every
	* descendant), stored as a flat, parent-index-linked array of
	* PrefabNode so it round-trips through JSON. Capture()
	* records a live subtree; Instantiate() recreates it (as a new,
	* independent copy of actors) in a World.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* Actor のサブツリー（ルートとその全子孫）のシリアライズ可能な
	* スナップショット。JSON で往復できるよう、親インデックスで
	* 連結された PrefabNode のフラットな配列として保存される。
	* Capture() は生きたサブツリーを記録し、Instantiate() はそれを
	* World 内に（actor の新しい独立したコピーとして）再生成する。
	*/
	class SEEDCORE_API Prefab
	{
	public:
		/**
		* [EN]
		* Records root and every descendant into nodes_, replacing any
		* previously captured data.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* root とその全子孫を nodes_ へ記録し、以前に取得していたデータを
		* 置き換える。
		*/
		void Capture(Actor root);

		/**
		* [EN]
		* Recreates the captured subtree as new actors in world, parented
		* under parent (if given). Returns the root of the newly created
		* subtree, or nullptr if nothing was instantiated. sourceAssetID
		* is stamped onto the new root so later "apply to prefab" edits
		* know which .prefab asset to overwrite.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 取得済みのサブツリーを、world 内に新しい actor 群として再生成
		* する。parent が指定されていればその下へ配置する。新しく生成
		* されたサブツリーのルートを返す。何もインスタンス化されなければ
		* nullptr を返す。sourceAssetID は新しいルートへ刻印され、後の
		* 「プレハブに適用」編集がどの .prefab アセットを上書きすべきかを
		* 判断できるようにする。
		*/
		Actor Instantiate(World& world, ResourceCache& cache, Actor parent = {}, Uint32 sourceAssetID = 0)const;

		/**
		* [EN]
		* Binds the process-wide World/ResourceCache that the static
		* Spawn() overloads use. Must be called once, before any Spawn()
		* call, alongside Scene::Initialize. Exists so gameplay scripts
		* (SeedScript) can instantiate prefabs at runtime without
		* threading a World/ResourceCache reference through themselves --
		* neither is otherwise reachable from a script.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 静的な Spawn() オーバーロードが使用する、プロセス全体の
		* World/ResourceCache を束縛する。いずれかの Spawn() を呼ぶ前に、
		* Scene::Initialize と並べて一度だけ呼び出す必要がある。ゲーム
		* プレイスクリプト（SeedScript）が World/ResourceCache 参照を
		* 自前で引き回さずにランタイムでプレハブをインスタンス化できる
		* ようにするために存在する -- どちらもスクリプトからは他に
		* 到達手段が無い。
		*/
		static void Initialize(World& world, ResourceCache& cache);

		/**
		* [EN]
		* Loads the .prefab asset identified by assetID (via the bound
		* ResourceCache's PrefabPool) and instantiates it in the bound
		* World, parented under parent when given. Returns the new
		* subtree's root Actor, or nullptr if Initialize() has not run,
		* the asset could not be loaded, or nothing was instantiated. The
		* new root is stamped with assetID as its source prefab, so
		* inspector "apply to prefab" edits target the right asset.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* assetID で識別される .prefab アセットを（束縛済みの
		* ResourceCache の PrefabPool 経由で）読み込み、束縛済みの World
		* 内、parent が指定されていればその下へインスタンス化する。新しい
		* サブツリーのルート Actor を返す。Initialize() が未実行、アセットを
		* 読み込めなかった、または何もインスタンス化されなかった場合は
		* nullptr を返す。新しいルートには assetID が元プレハブとして
		* 刻印されるため、インスペクタの「プレハブに適用」編集が正しい
		* アセットを対象にできる。
		*/
		static Actor Spawn(Uint32 assetID, Actor parent = {});

		/**
		* [EN]
		* Resolves name to an asset ID via the bound ResourceCache, then
		* delegates to Spawn(Uint32, Actor). name must include the
		* ".prefab" extension; it is matched against asset filenames (a
		* bare filename is enough -- the subfolder does not matter) and is
		* not stem-matched, so "Enemy" will not find "Enemy.prefab". Pass
		* a string literal so Tools/Python/RuntimePackager.py can
		* statically detect the reference and bundle the asset into a
		* package build. Logs a warning and returns nullptr if name does
		* not resolve.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 束縛済みの ResourceCache 経由で name をアセット ID へ解決し、
		* Spawn(Uint32, Actor) へ委譲する。name は ".prefab" 拡張子を
		* 含める必要がある。アセットのファイル名と照合され（サブフォルダ
		* は問わず、ファイル名だけで良い）、stem 一致はしないため
		* "Enemy" では "Enemy.prefab" は見つからない。
		* Tools/Python/RuntimePackager.py が参照を静的に検出して
		* パッケージビルドへアセットを同梱できるよう、文字列リテラルを
		* 渡すこと。name が解決できなければ警告をログ出力して nullptr を
		* 返す。
		*/
		static Actor Spawn(const String& name, Actor parent = {});

		/**
		* [EN]
		* Writes this prefab's captured data to path as JSON. Returns
		* whether the file was written successfully.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* この prefab の取得済みデータを、JSON として path へ書き込む。
		* ファイルが正常に書き込まれたかどうかを返す。
		*/
		Bool Write(const std::filesystem::path& path);

		/**
		* [EN]
		* Reads captured data from path (JSON), replacing this prefab's
		* current data. Returns whether reading succeeded.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* path（JSON）から取得済みデータを読み込み、この prefab の現在の
		* データを置き換える。読み込みに成功したかどうかを返す。
		*/
		Bool Read(const std::filesystem::path& path);

		/**
		* [EN]
		* Captures root into a temporary Prefab and writes it to path,
		* creating parent directories as needed. Returns whether saving succeeded.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* root を一時的な Prefab へ取得し、path へ書き込む（必要なら親
		* ディレクトリを作成する）。保存に成功したかどうかを返す。
		*/
		static Bool Save(Actor root, const std::filesystem::path& path);

		/**
		* [EN]
		* Captures root and saves it into directory under a name derived
		* from root's Name component (or "Actor" if unnamed), appending a
		* numeric suffix to avoid overwriting an existing file. Returns
		* the path actually written to, or an empty path on failure.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* root を取得し、その Name コンポーネントから導出された名前
		* （名前が無ければ "Actor"）で directory 内へ保存する。既存
		* ファイルの上書きを避けるため、数値の接尾辞を付加する。実際に
		* 書き込まれたパスを返す。失敗時は空のパスを返す。
		*/
		static std::filesystem::path SaveToDirectory(Actor root, const std::filesystem::path& directory);

		/**
		* [EN]
		* Serialization hook (save side): writes nodes_ and basePrefabAssetID_.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* シリアライズ用フック(保存側): nodes_ と basePrefabAssetID_ を
		* 書き込む。
		*/
		template<class Archive>
		void Save(Archive& archive)const
		{
			archive.Field("nodes", nodes_);
			archive.Field("basePrefabAssetID", basePrefabAssetID_);
		}

		/**
		* [EN]
		* Serialization hook (load side): reads nodes_ and basePrefabAssetID_.
		* Both are optional -- a missing key is swallowed rather than
		* failing the whole load.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* シリアライズ用フック(読み込み側): nodes_ と basePrefabAssetID_ を
		* 読み込む。どちらも任意項目 -- キーが無くても読み込み全体を
		* 失敗させず読み飛ばす。
		*/
		template<class Archive>
		void Load(Archive& archive)
		{
			archive.TryField("nodes", nodes_);
			archive.TryField("basePrefabAssetID", basePrefabAssetID_);
		}

	private:
		/// [EN] Flat, parent-index-linked array of every captured actor node in the subtree (index 0 is always the root).
		/// [JP] サブツリー内の全取得済み actor ノードの、親インデックスで連結されたフラットな配列（インデックス0は常にルート）。
		DynamicArray<PrefabNode> nodes_;

		/// [EN] Asset ID of the prefab this one was originally derived from, if any (0 if none).
		/// [JP] このプレハブが元々派生した元プレハブのアセット ID（無ければ0）。
		Uint32 basePrefabAssetID_ = 0;

		/// [EN] Process-wide World bound by Initialize, used by the static Spawn() overloads. Null until Initialize() runs.
		/// [JP] Initialize によって束縛される、プロセス全体の World。静的な Spawn() オーバーロードが使用する。Initialize() 実行までは null。
		static World* world_;

		/// [EN] Process-wide ResourceCache bound by Initialize, used to resolve prefab names/IDs and load prefab assets.
		/// [JP] Initialize によって束縛される、プロセス全体の ResourceCache。プレハブ名/ID の解決とプレハブアセットの読み込みに使う。
		static ResourceCache* resource_;
	};
}
