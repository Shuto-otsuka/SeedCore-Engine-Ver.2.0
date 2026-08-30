#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/ECS/ReflectionRegistry.h>

namespace SeedCore
{
	class Actor;
	class World;
	class ResourceCache;

	/**
	* [EN]
	* Serialized value of a single reflected field: a discriminated
	* union of every scalar AttributeType, plus (via isArray_/children_)
	* support for array-valued and nested-struct fields. Only the
	* member matching type_ holds meaningful data.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* リフレクションされた単一フィールドのシリアライズ済みの値。
	* あらゆるスカラー AttributeType のタグ付き共用体であり、
	* （isArray_/children_ を通じて）配列値フィールドとネストされた
	* 構造体フィールドもサポートする。type_ に一致するメンバのみが
	* 意味のあるデータを持つ。
	*/
	struct SerializedField
	{
		/// [EN] The field's name, used to match it back to a FieldInfo on restore.
		/// [JP] フィールドの名前。復元時に FieldInfo と対応付けるために使う。
		String name_;

		/// [EN] Which member below holds this field's actual value.
		/// [JP] 以下のどのメンバがこのフィールドの実際の値を保持するかを示す。
		AttributeType type_ = AttributeType::Unknown;

		/// [EN] Value when type_ is Float.
		/// [JP] type_ が Float の場合の値。
		Float floatValue_ = 0.0f;

		/// [EN] Value when type_ is Vector2.
		/// [JP] type_ が Vector2 の場合の値。
		Vector2 vector2Value_;

		/// [EN] Value when type_ is Vector3.
		/// [JP] type_ が Vector3 の場合の値。
		Vector3 vector3Value_;

		/// [EN] Value when type_ is Color.
		/// [JP] type_ が Color の場合の値。
		Color colorValue_;

		/// [EN] Value when type_ is Bool.
		/// [JP] type_ が Bool の場合の値。
		Bool boolValue_ = false;

		/// [EN] Value when type_ is Int or Enum.
		/// [JP] type_ が Int または Enum の場合の値。
		Int intValue_ = 0;

		/// [EN] Value when type_ is String.
		/// [JP] type_ が String の場合の値。
		String stringValue_;

		/// [EN] Whether this field represents an array; if true, children_ holds one entry per array element instead of a scalar value.
		/// [JP] このフィールドが配列を表すかどうか。true の場合、children_ はスカラー値の代わりに配列の各要素を1エントリずつ保持する。
		Bool isArray_ = false;

		/// [EN] For array fields, one entry per element; for nested-struct fields, the struct's own captured field list. Empty for plain scalar fields.
		/// [JP] 配列フィールドの場合は要素ごとの1エントリ。ネストされた構造体フィールドの場合はその構造体自身の取得済みフィールド一覧。単純なスカラーフィールドの場合は空。
		DynamicArray<SerializedField> children_;

		/**
		* [EN]
		* Serialization hook: reads/writes every member above. type_ is
		* round-tripped through a plain Int since the archive cannot
		* serialize an enum class directly.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* シリアライズ用フック: 上記の全メンバを読み書きする。
		* アーカイブは enum class を直接シリアライズできないため、type_ は
		* 素の Int を経由して往復させる。
		*/
		template<class Archive>
		void Serialize(Archive& archive)
		{
			Int typeValue = static_cast<Int>(type_);
			archive.Field("name", name_);
			archive.Field("type", typeValue);
			archive.Field("float", floatValue_);
			archive.Field("vector2", vector2Value_);
			archive.Field("vector3", vector3Value_);
			archive.Field("color", colorValue_);
			archive.Field("bool", boolValue_);
			archive.Field("int", intValue_);
			archive.Field("string", stringValue_);
			archive.Field("is_array", isArray_);
			archive.Field("children", children_);
			type_ = static_cast<AttributeType>(typeValue);
		}
	};

	/**
	* [EN]
	* Serialized snapshot of a single non-builtin component attached to
	* an actor: its registered type name plus every one of its reflected fields.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* actor にアタッチされた単一の非組み込みコンポーネントのシリアライズ
	* 済みスナップショット。その登録済み型名と、リフレクションされた
	* 全フィールドを保持する。
	*/
	struct SerializedComponent
	{
		/// [EN] The component's registered type name, used to look it up in ComponentRegistry on restore.
		/// [JP] コンポーネントの登録済み型名。復元時に ComponentRegistry から検索するために使う。
		String componentName_;

		/// [EN] Every reflected field captured for this component.
		/// [JP] このコンポーネントについて取得済みの、全リフレクションフィールド。
		DynamicArray<SerializedField> fields_;

		/**
		* [EN]
		* Serialization hook: reads/writes componentName_ and fields_.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* シリアライズ用フック: componentName_ と fields_ を読み書きする。
		*/
		template<class Archive>
		void Serialize(Archive& archive)
		{
			archive.Field("component", componentName_);
			archive.Field("fields", fields_);
		}
	};

	/**
	* [EN]
	* Serialized snapshot of a single Actor within a Scene or Prefab:
	* its name/tags/active state/transform, every non-builtin component
	* it holds, its position within the flat parent-index-linked node
	* array, and (if it's actually the root of a nested prefab
	* instance) a reference back to that prefab asset instead of its
	* own component list.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* Scene または Prefab 内の単一 Actor のシリアライズ済みスナップショット。
	* その名前/タグ/アクティブ状態/トランスフォーム、保持している全ての
	* 非組み込みコンポーネント、フラットな親インデックス連結ノード配列内
	* での位置、および（実際にネストされたプレハブインスタンスのルートで
	* あれば）自身のコンポーネント一覧の代わりに、そのプレハブアセットへの
	* 参照。
	*/
	struct SerializedActorNode
	{
		/// [EN] The actor's display name.
		/// [JP] actor の表示名。
		String name_;

		/// [EN] Every tag set on the actor.
		/// [JP] actor に設定されている全タグ。
		DynamicArray<String> tags_;

		/// [EN] Name of the actor's layer at capture time (see LayerRegistry). Stored by name, not index, so a slot reshuffle elsewhere doesn't retarget this actor to the wrong layer.
		/// [JP] 取得時点での actor のレイヤー名（LayerRegistry 参照）。インデックスではなく名前で保存する。他所でのスロット並び替えにより、この actor が別のレイヤーへ誤って向いてしまわないようにするため。
		String layerName_;

		/// [EN] Whether the actor was active at capture time.
		/// [JP] 取得時点で actor がアクティブだったかどうか。
		Bool active_ = true;

		/// [EN] Local position at capture time.
		/// [JP] 取得時点でのローカル位置。
		Vector3 position_;

		/// [EN] Local rotation (Euler angles) at capture time.
		/// [JP] 取得時点でのローカル回転（オイラー角）。
		Vector3 rotation_;

		/// [EN] Local scale at capture time.
		/// [JP] 取得時点でのローカルスケール。
		Vector3 scale_ = Vector3(1.0f, 1.0f, 1.0f);

		/// [EN] Every non-builtin component captured for this actor (empty if this node is a nested prefab instance; see nestedPrefabAssetID_).
		/// [JP] この actor について取得済みの、全非組み込みコンポーネント（このノードがネストされたプレハブインスタンスであれば空。nestedPrefabAssetID_ を参照）。
		DynamicArray<SerializedComponent> components_;

		/// [EN] Index of this node's parent within the owning array, or -1 if it's a root.
		/// [JP] 所有元の配列内における、このノードの親のインデックス。ルートであれば -1。
		Int parentIndex_ = -1;

		/// [EN] The actor's persistent ID at capture time (see Actor::GetPersistentID), restored on instantiation so other actors (e.g. constraint targets) can reference it stably. 0 if never assigned.
		/// [JP] 取得時点での actor の永続ID（Actor::GetPersistentID 参照）。インスタンス化時に復元され、他の actor（例: コンストレイントのターゲット）がこれを安定して参照できるようにする。未割り当てなら 0。
		Uint32 persistentId_ = 0;

		/// [EN] If nonzero, this node is the root of a nested prefab instance: instantiate that prefab asset instead of using components_.
		/// [JP] 0以外であれば、このノードはネストされたプレハブインスタンスのルートである: components_ を使う代わりに、そのプレハブアセットをインスタンス化する。
		Uint32 nestedPrefabAssetID_ = 0;

		/// [EN] Field-value overrides applied on top of a nested prefab instance (currently reserved; see nestedPrefabAssetID_).
		/// [JP] ネストされたプレハブインスタンスの上に適用される、フィールド値の上書き（現時点では予約済み。nestedPrefabAssetID_ を参照）。
		DynamicArray<SerializedField> overrides_;

		/**
		* [EN]
		* Serialization hook (save side): writes every member above.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* シリアライズ用フック(保存側): 上記の全メンバを書き込む。
		*/
		template<class Archive>
		void Save(Archive& archive)const
		{
			archive.Field("name", name_);
			archive.Field("tags", tags_);
			archive.Field("layer", layerName_);
			archive.Field("active", active_);
			archive.Field("position", position_);
			archive.Field("rotation", rotation_);
			archive.Field("scale", scale_);
			archive.Field("components", components_);
			archive.Field("parentIndex", parentIndex_);
			archive.Field("nestedPrefabAssetID", nestedPrefabAssetID_);
			archive.Field("overrides", overrides_);
			archive.Field("persistentId", persistentId_);
		}

		/**
		* [EN]
		* Serialization hook (load side): reads every member above.
		* persistentId_ is optional -- nodes saved before this field
		* existed simply leave it 0 (World::CreateActor auto-allocates a
		* fresh ID in that case) -- so a missing key is swallowed rather
		* than failing the whole load.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* シリアライズ用フック(読み込み側): 上記の全メンバを読み込む。
		* persistentId_ は任意項目 -- このフィールドが存在する前に保存された
		* ノードでは単に 0 のままになる(その場合 World::CreateActor が
		* 新規IDを自動割り当てする) -- なので、キーが無くても読み込み全体を
		* 失敗させず読み飛ばす。
		*/
		template<class Archive>
		void Load(Archive& archive)
		{
			archive.TryField("name", name_);
			archive.TryField("tags", tags_);
			archive.TryField("layer", layerName_);
			archive.TryField("active", active_);
			archive.TryField("position", position_);
			archive.TryField("rotation", rotation_);
			archive.TryField("scale", scale_);
			archive.TryField("components", components_);
			archive.TryField("parentIndex", parentIndex_);
			archive.TryField("nestedPrefabAssetID", nestedPrefabAssetID_);
			archive.TryField("overrides", overrides_);
			archive.TryField("persistentId", persistentId_);
		}
	};

	/**
	* [EN]
	* Recursively captures actor and every descendant into outNodes as
	* a flat, parent-index-linked array (used by both Scene::Capture
	* and Prefab::Capture). Returns actor's own index within outNodes.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* actor とその全子孫を、フラットな親インデックス連結配列として
	* outNodes へ再帰的に取得する（Scene::Capture と Prefab::Capture の
	* 両方から使われる）。actor 自身の outNodes 内でのインデックスを返す。
	*/
	SEEDCORE_API Int CaptureActorNode(Actor* actor, Int parentIndex, DynamicArray<SerializedActorNode>& outNodes);

	/**
	* [EN]
	* Recreates a single captured node as a live Actor in world (used by
	* both Scene::Instantiate and Prefab::Instantiate): either by
	* instantiating a referenced nested prefab, or by creating a fresh
	* actor and restoring its component fields. Applies the node's
	* transform/active/tags and reparents under parentActor. Returns the
	* new actor, or nullptr on failure.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 取得済みの単一ノードを world 内の生きた Actor として再生成する
	* （Scene::Instantiate と Prefab::Instantiate の両方から使われる）:
	* 参照されているネストされたプレハブをインスタンス化するか、新しい
	* actor を生成してそのコンポーネントフィールドを復元する。ノードの
	* トランスフォーム/アクティブ状態/タグを適用し、parentActor の下へ
	* 再親化する。新しい actor を返す。失敗時は nullptr を返す。
	*/
	SEEDCORE_API Actor* InstantiateActorNode(World& world, ResourceCache& cache, const SerializedActorNode& node, Actor* parentActor, Bool markAsPrefabInstance);

	/**
	* [EN]
	* Captures a single component instance's reflected fields into a
	* SerializedComponent, without needing an owning Actor/World —
	* useful when a component type's own storage container is about to
	* be destroyed (e.g. before a hot-reload) and only its field values
	* need to survive the round trip.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 所有元の Actor/World を必要とせず、単一コンポーネントインスタンスの
	* リフレクションフィールドを SerializedComponent へ取得する。
	* コンポーネント型自身のストレージコンテナがこれから破棄される場合
	* （ホットリロード前など）に、フィールド値だけを往復させたいときに使う。
	*/
	SEEDCORE_API SerializedComponent CaptureComponent(const String& componentName, void* componentData);

	/**
	* [EN]
	* Restores previously captured field values onto an existing
	* component instance, matched by field name (see CaptureComponent).
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 以前に取得したフィールド値を、既存のコンポーネントインスタンスへ
	* フィールド名で対応付けて復元する(CaptureComponent を参照)。
	*/
	SEEDCORE_API void ApplyComponent(const SerializedComponent& component, void* componentData);
}
