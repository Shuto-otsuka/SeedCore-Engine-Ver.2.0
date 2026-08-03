#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/ECS/ReflectionRegistry.h>
#include <FoundationEngine/Resource/ActorSerialization.h>
#include <FoundationEngine/Utility/SerializeFallback.h>

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
	* PrefabNode so it round-trips through Cereal JSON. Capture()
	* records a live subtree; Instantiate() recreates it (as a new,
	* independent copy of actors) in a World.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* Actor のサブツリー（ルートとその全子孫）のシリアライズ可能な
	* スナップショット。Cereal JSON で往復できるよう、親インデックスで
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
		void Capture(Actor* root);

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
		Actor* Instantiate(World& world, ResourceCache& cache, Actor* parent = nullptr, Uint32 sourceAssetID = 0)const;

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
		static Bool Save(Actor* root, const std::filesystem::path& path);

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
		static std::filesystem::path SaveToDirectory(Actor* root, const std::filesystem::path& directory);

		/**
		* [EN]
		* Cereal serialization hook: reads/writes nodes_ and basePrefabAssetID_.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* Cereal シリアライズ用フック: nodes_ と basePrefabAssetID_ を
		* 読み書きする。
		*/
		template<class Archive>
		void serialize(Archive& archive)
		{
			TryLoadField(archive, "nodes", nodes_);
			TryLoadField(archive, "basePrefabAssetID", basePrefabAssetID_);
		}

	private:
		/// [EN] Flat, parent-index-linked array of every captured actor node in the subtree (index 0 is always the root).
		/// [JP] サブツリー内の全取得済み actor ノードの、親インデックスで連結されたフラットな配列（インデックス0は常にルート）。
		DynamicArray<PrefabNode> nodes_;

		/// [EN] Asset ID of the prefab this one was originally derived from, if any (0 if none).
		/// [JP] このプレハブが元々派生した元プレハブのアセット ID（無ければ0）。
		Uint32 basePrefabAssetID_ = 0;
	};
}
