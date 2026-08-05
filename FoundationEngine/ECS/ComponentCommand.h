#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/ECS/Command.h>
#include <FoundationEngine/ECS/Entity.h>
#include <FoundationEngine/ECS/EcsID.h>
#include <FoundationEngine/ECS/World.h>

namespace SeedCore
{
	/**
	* [EN]
	* Undo/redo command for a single scalar/vector/string-typed component
	* field edit (Position/Rotation/Scale via the viewport gizmo, an
	* Inspector DragFloat/DragInt/Checkbox/ColorEdit/enum combo/text
	* field, etc.). Re-resolves the field's address from
	* world_/entity_/componentID_/offset_ every time Redo/Undo runs,
	* rather than caching a raw pointer, so it stays valid even if the
	* component's storage moved (e.g. an archetype migration from adding/
	* removing an unrelated component) between the edit and the undo.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 単一のスカラー/ベクター/String型コンポーネントフィールド編集
	* (ビューポートのギズモ経由のPosition/Rotation/Scale、Inspectorの
	* DragFloat/DragInt/Checkbox/ColorEdit/enumコンボ/テキストフィールド
	* など)に対するUndo/Redoコマンド。フィールドのアドレスは生ポインタを
	* キャッシュせず、Redo/Undo実行のたびに
	* world_/entity_/componentID_/offset_ から再解決する — 編集からUndoの
	* 間にコンポーネントの格納位置が動いていても(無関係な別コンポーネントの
	* 追加/削除によるアーキタイプ移行など)有効なままにするため。
	*/
	template <typename T>
	class ComponentCommand : public Command
	{
	public:
		/**
		* [EN]
		* world/entity/componentID/offset identify the field; oldValue/
		* newValue are what Undo/Redo write into it.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* world/entity/componentID/offset がフィールドを特定し、
		* oldValue/newValue がUndo/Redoでそこへ書き込む値。
		*/
		ComponentCommand(World& world, Entity entity, ComponentID componentID, Size offset, const T& oldValue, const T& newValue)
			: world_(world), entity_(entity), componentID_(componentID), offset_(offset), oldValue_(oldValue), newValue_(newValue)
		{
			/// No Code
		}

		/**
		* [EN]
		* Re-applies this edit (writes newValue_ into the field).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* この編集を再適用する(フィールドへ newValue_ を書き込む)。
		*/
		void Redo()override
		{
			Write(newValue_);
		}

		/**
		* [EN]
		* Reverts this edit (writes oldValue_ into the field).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* この編集を取り消す(フィールドへ oldValue_ を書き込む)。
		*/
		void Undo()override
		{
			Write(oldValue_);
		}

	private:
		/**
		* [EN]
		* Resolves the field's current address and overwrites it with
		* value; does nothing if entity_/componentID_ no longer resolves to
		* live component data.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* フィールドの現在のアドレスを解決し、value で上書きする。
		* entity_/componentID_ が既に有効なコンポーネントデータへ解決
		* できない場合は何もしない。
		*/
		void Write(const T& value)
		{
			void* component = world_.GetComponent(entity_, componentID_);
			if (!component)
			{
				return;
			}

			*reinterpret_cast<T*>(static_cast<Uint8*>(component) + offset_) = value;
		}

		/// [EN] The World the edited entity/component belong to.
		/// [JP] 編集対象のentity/componentが属するWorld。
		World& world_;

		/// [EN] The edited entity.
		/// [JP] 編集対象のentity。
		Entity entity_;

		/// [EN] The edited component's registered ComponentID.
		/// [JP] 編集対象コンポーネントの登録済みComponentID。
		ComponentID componentID_;

		/// [EN] Byte offset of the field within the component.
		/// [JP] コンポーネント内でのフィールドのバイトオフセット。
		Size offset_;

		/// [EN] The field's value before the edit; written back on Undo.
		/// [JP] 編集前のフィールド値。Undo時に書き戻される。
		T oldValue_;

		/// [EN] The field's value after the edit; written back on Redo.
		/// [JP] 編集後のフィールド値。Redo時に書き戻される。
		T newValue_;
	};

	/**
	* [EN]
	* Undo/redo command for a field that isn't reachable from its
	* component by a fixed byte offset (FieldInfo::directPtr_ is set -
	* e.g. an element of a DynamicArray<T> member, which lives in a
	* separately-allocated heap buffer rather than the component's own
	* POD layout). Holds the field's address directly instead of
	* re-resolving it through World/Entity/ComponentID like
	* ComponentCommand does, so it stays correct only as long as that
	* address itself stays valid (i.e. the owning array isn't resized and
	* its owning Actor/component isn't destroyed) between the edit and
	* the undo.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* コンポーネントから固定バイトオフセットで到達できないフィールド
	* (FieldInfo::directPtr_ が設定されている場合 - 例: DynamicArray<T>
	* メンバの要素。コンポーネント自身のPODレイアウトではなく、別途
	* 確保されたヒープバッファ上に存在する)に対するUndo/Redoコマンド。
	* ComponentCommandのようにWorld/Entity/ComponentID経由で毎回再解決
	* するのではなく、フィールドのアドレスを直接保持する - そのため
	* 編集からUndoまでの間、そのアドレス自体が有効であり続ける場合
	* (所有元の配列がリサイズされておらず、所有元のActor/コンポーネントが
	* 破棄されていない場合)にのみ正しく動作する。
	*/
	template <typename T>
	class PointerCommand : public Command
	{
	public:
		/**
		* [EN]
		* pointer identifies the field directly; oldValue/newValue are
		* what Undo/Redo write into it.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* pointer がフィールドを直接特定し、oldValue/newValue が
		* Undo/Redoでそこへ書き込む値。
		*/
		PointerCommand(T* pointer, const T& oldValue, const T& newValue)
			: pointer_(pointer), oldValue_(oldValue), newValue_(newValue)
		{
			/// No Code
		}

		/**
		* [EN]
		* Re-applies this edit (writes newValue_ into the field).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* この編集を再適用する(フィールドへ newValue_ を書き込む)。
		*/
		void Redo()override
		{
			*pointer_ = newValue_;
		}

		/**
		* [EN]
		* Reverts this edit (writes oldValue_ into the field).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* この編集を取り消す(フィールドへ oldValue_ を書き込む)。
		*/
		void Undo()override
		{
			*pointer_ = oldValue_;
		}

	private:
		/// [EN] Direct address of the field's storage.
		/// [JP] フィールドのストレージへの直接アドレス。
		T* pointer_;

		/// [EN] The field's value before the edit; written back on Undo.
		/// [JP] 編集前のフィールド値。Undo時に書き戻される。
		T oldValue_;

		/// [EN] The field's value after the edit; written back on Redo.
		/// [JP] 編集後のフィールド値。Redo時に書き戻される。
		T newValue_;
	};
}
