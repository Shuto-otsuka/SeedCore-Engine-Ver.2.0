#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/Resource/ActorSerialization.h>
#include <FoundationEngine/ECS/EcsID.h>

namespace SeedCore
{
	class World;
	class Actor;

	/**
	* [EN]
	* One hot-reloadable gameplay DLL (a "plugin"): a module that exports
	* SC_OnGameLoad / SC_OnGameUnload (and optionally SC_SetImGuiContext).
	* It is loaded from a shadow copy so the original file on disk stays
	* writable for a rebuild, and can be reloaded in place while the editor
	* runs.
	*
	* Across a reload, every ComponentBase-derived component whose code
	* lives in this module has its reflected field values captured, its
	* instance destroyed on its Actor, and - once no instance of that type
	* remains - its sparse-set storage container destroyed, so no vtable
	* pointer into the soon-to-be-unloaded DLL survives. After the new
	* module loads and re-registers those types the captured components are
	* re-added to the same Actors and their field values restored, much
	* like Unity's domain reload - provided reflected field layout is
	* compatible across the rebuild.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* ホットリロード可能なゲームプレイ DLL 1個（「プラグイン」）:
	* SC_OnGameLoad / SC_OnGameUnload（および任意で SC_SetImGuiContext）を
	* エクスポートするモジュール。リビルドのために元ファイルを書き込み
	* 可能なまま保つため、シャドウコピーからロードされ、エディタ実行中に
	* その場でリロードできる。
	*
	* リロードをまたいで、このモジュール内にコードがある全ての
	* ComponentBase 派生コンポーネントは: リフレクションフィールド値を
	* 取得し、その Actor 上のインスタンスを破棄し、その型のインスタンスが
	* 無くなった時点でスパースセットストレージコンテナも破棄する
	* — アンロードされる DLL 内へのポインタが一切残らないようにする。
	* 新しいモジュールがロードされ型を再登録した後、取得済みの
	* コンポーネントは同じ Actor へ再追加され、フィールド値が復元される
	* — Unity のドメインリロードに近い（リビルドをまたいでリフレクション
	* フィールドのレイアウトに互換性がある場合に限る）。
	*/
	class SEEDCORE_API PluginModule :public NonTransferable
	{
	public:
		/**
		* [EN]
		* Constructs a module bound to sourcePath (the real DLL on disk),
		* not yet loaded.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* sourcePath（ディスク上の実 DLL）に紐づくモジュールを構築する。
		* この時点ではまだロードされていない。
		*/
		explicit PluginModule(const std::filesystem::path& sourcePath);

		~PluginModule();

		/**
		* [EN]
		* Shadow-copies the source DLL, loads it, resolves its entry
		* points, forwards imguiContext to it (if it exports
		* SC_SetImGuiContext), calls SC_OnGameLoad, then restores any
		* components captured by a prior Unload. Returns whether the load
		* succeeded.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 元 DLL をシャドウコピーしてロードし、エントリポイントを解決し、
		* imguiContext を（SC_SetImGuiContext をエクスポートしていれば）
		* そのモジュールへ渡し、SC_OnGameLoad を呼び、直前の Unload で
		* 取得したコンポーネントがあれば復元する。ロードに成功したか
		* どうかを返す。
		*/
		Bool Load(World& world, ImGuiContext* imguiContext);

		/**
		* [EN]
		* Captures and destroys every component this module owns, calls
		* SC_OnGameUnload, unregisters the module's reflection/payload
		* entries, frees the module, and deletes its shadow copy. The
		* captured components are kept so the next Load can restore them.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* このモジュールが所有する全コンポーネントを取得・破棄し、
		* SC_OnGameUnload を呼び、モジュールのリフレクション/ペイロード
		* エントリを登録解除し、モジュールを解放してシャドウコピーを
		* 削除する。次の Load が復元できるよう、取得済みコンポーネントは
		* 保持したままにする。
		*/
		void Unload(World& world);

		/**
		* [EN]
		* Unload followed by Load (component captures carry across).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* Unload の後に Load を行う（コンポーネントの取得内容は引き継がれる）。
		*/
		Bool Reload(World& world, ImGuiContext* imguiContext);

		/**
		* [EN]
		* Returns whether the source DLL's last-write time differs from the
		* one seen at the last Load - i.e. it was rebuilt.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 元 DLL の最終更新時刻が直近の Load 時点と異なるか
		* — つまりリビルドされたか — を返す。
		*/
		[[nodiscard]] Bool SourceChanged()const;

		/**
		* [EN]
		* The real DLL path on disk this module is bound to.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* このモジュールが紐づく、ディスク上の実 DLL パス。
		*/
		[[nodiscard]] const std::filesystem::path& SourcePath()const;

		/**
		* [EN]
		* Whether the module is currently loaded.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* モジュールが現在ロードされているか。
		*/
		[[nodiscard]] Bool IsLoaded()const;

		/**
		* [EN]
		* Best-effort deletion of leftover shadow DLLs / hot-reload PDBs
		* for this module's stem in its directory (from earlier reloads or
		* a session that ended without a clean Unload). The currently
		* loaded shadow copy is skipped.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* このモジュールの stem に対応する、ディレクトリ内に残った
		* シャドウ DLL / ホットリロード用 PDB を可能な範囲で削除する
		* （過去のリロードや、正常な Unload を経ずに終了したセッションの
		* 残骸）。現在ロード中のシャドウコピーは対象外。
		*/
		void CleanupStaleArtifacts()const;

		/**
		* [EN]
		* Returns the source DLL's current last-write time (0 if it cannot
		* be read).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 元 DLL の現在の最終更新時刻を返す（読み取れない場合は 0）。
		*/
		[[nodiscard]] static Uint64 GetLastWriteTime(const std::filesystem::path& path);

	private:
		/**
		* [EN]
		* Returns every key currently in registry - a snapshot of its
		* contents taken before a module load.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* registry に現在ある全てのキーを返す - モジュールをロードする前に
		* 取るその内容のスナップショット。
		*/
		template<typename Map>
		static DynamicArray<String> CollectRegistryKeys(const Map& registry)
		{
			DynamicArray<String> keys;
			std::ranges::copy(registry | std::views::keys, std::back_inserter(keys));
			return keys;
		}

		/**
		* [EN]
		* Returns the keys present in registry but absent from previousKeys
		* - i.e. those a just-loaded module's static initializers added.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* registry には存在するが previousKeys には無いキー - つまりロード
		* されたばかりのモジュールの静的初期化子が追加したキーを返す。
		*/
		template<typename Map>
		static DynamicArray<String> FindAddedRegistryKeys(const Map& registry, const DynamicArray<String>& previousKeys)
		{
			DynamicArray<String> added;
			std::ranges::copy_if(registry | std::views::keys, std::back_inserter(added), [&previousKeys](const String& name){ return !std::ranges::contains(previousKeys, name); });
			return added;
		}

		/**
		* [EN]
		* Erases every reflection / payload entry this module registered.
		* Must run while the module is still mapped - the registries hold
		* std::function objects whose destructors live inside it.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* このモジュールが登録したリフレクション/ペイロードのエントリを全て
		* 削除する。モジュールがまだメモリ上にある間に実行する必要がある -
		* レジストリが保持する std::function のデストラクタはモジュール内に
		* あるため。
		*/
		void UnregisterModuleReflection();

		/**
		* [EN]
		* For every live component whose code lives in handle_: captures its
		* reflected fields into capturedComponents_, removes it from its
		* Actor, then destroys that type's now-empty sparse-set storage
		* container so no vtable pointer into the module survives.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* handle_ 内にコードがある全ての生きたコンポーネントについて: その
		* リフレクションフィールドを capturedComponents_ へ取得し、その Actor
		* から削除した上で、空になったその型のスパースセットストレージ
		* コンテナを破棄する。モジュール内への vtable ポインタが残らないように
		* するため。
		*/
		void DestroyCapturedComponents(World& world);

		/**
		* [EN]
		* Re-adds every component in capturedComponents_ to its original
		* Actor and restores its captured field values, then clears
		* capturedComponents_.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* capturedComponents_ の各コンポーネントを元の Actor へ再追加し、
		* 取得済みのフィールド値を復元した上で、capturedComponents_ を
		* クリアする。
		*/
		void RestoreCapturedComponents(World& world);

	private:
		using OnGameLoadFn = void(*)(World&);
		using OnGameUnloadFn = void(*)(World&);
		using SetImGuiContextFn = void(*)(ImGuiContext*);

		/// [EN] The real DLL on disk (in the plugin directory).
		/// [JP] ディスク上の実 DLL（プラグインディレクトリ内）。
		std::filesystem::path sourcePath_;

		/// [EN] Handle to the currently loaded shadow-copy module (nullptr while unloaded).
		/// [JP] 現在ロード中のシャドウコピーモジュールへのハンドル（未ロード時は nullptr）。
		HMODULE handle_ = nullptr;

		/// [EN] Path of the shadow-copy DLL currently loaded via handle_.
		/// [JP] handle_ が指す、現在ロード中のシャドウコピー DLL のパス。
		std::filesystem::path shadowPath_;

		/// [EN] Source DLL's last-write time at the moment it was last shadow-copied.
		/// [JP] 直近にシャドウコピーした時点での、元 DLL の最終更新時刻。
		Uint64 lastWriteTime_ = 0;

		OnGameLoadFn onGameLoad_ = nullptr;
		OnGameUnloadFn onGameUnload_ = nullptr;
		SetImGuiContextFn setImGuiContext_ = nullptr;

		/// [EN] Component field snapshots captured from the previously loaded module, awaiting restoration once the new module re-registers the type.
		/// [JP] 直前にロードされていたモジュールから取得したコンポーネントのフィールドスナップショット。新しいモジュールが型を再登録した後に復元されるのを待っている。
		DynamicArray<std::pair<Actor, SerializedComponent>> capturedComponents_;

		/// [EN] Type names this module added to ReflectionRegistry, determined by diffing the registry across LoadLibrary. Erased again before the module is freed.
		/// [JP] このモジュールが ReflectionRegistry へ追加した型名。LoadLibrary の前後でレジストリを差分比較して求める。モジュールを解放する前に再び削除される。
		DynamicArray<String> registeredReflectionNames_;

		/// [EN] Type names this module added to PayloadRegistry (see registeredReflectionNames_).
		/// [JP] このモジュールが PayloadRegistry へ追加した型名（registeredReflectionNames_ を参照）。
		DynamicArray<String> registeredPayloadNames_;
	};
}
