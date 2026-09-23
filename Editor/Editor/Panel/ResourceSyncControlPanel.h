#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/Resource/Asset/Asset.h>

namespace SeedCore
{
	struct EditorContext;
	class Actor;

	/**
	* [EN]
	* The interface side of asset sharing: the pieces of the Editor that
	* show what the team has, who is editing what, and offer the actions
	* that reach the shared library. All state lives in ResourceSync; this
	* only draws it and forwards what the member asks for.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* アセット共有の画面側。チームが何を持っているか、誰が何を編集中かを
	* 表示し、共有ライブラリへ届く操作を提供する部分。状態は全て
	* ResourceSync にあり、ここはそれを描いて、メンバーの要求を渡すだけ。
	*/
	class ResourceSyncControlPanel
	{
	public:
		/**
		* [EN]
		* Draws the one-line state of the shared library at the top of the
		* content browser.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* コンテンツブラウザの上部に、共有ライブラリの状態を1行で描く。
		*/
		static void DrawStatus(EditorContext& context);

		/**
		* [EN]
		* Adds the shared-library entries to an asset's context menu.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* アセットの右クリックメニューへ、共有ライブラリ用の項目を足す。
		*/
		static void DrawActions(EditorContext& context, const AssetRecord& asset);

		/**
		* [EN]
		* Draws an asset's revision and who is currently editing it.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* アセットの Revision と、今それを誰が編集しているかを描く。
		*/
		static void DrawState(EditorContext& context, const AssetRecord& asset);

		/**
		* [EN]
		* Whether the given actor may be changed, asking for the right to
		* edit it when request says the member is reaching for it.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* その Actor を変更してよいかどうか。request が立っていれば、メンバー
		* が操作しようとしているとみなして編集権を要求する。
		*/
		static Bool EditableActor(EditorContext& context, Actor actor, Bool request);

		/**
		* [EN]
		* Whether the scene's own structure may be changed, which covers
		* adding, removing and reparenting entities.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* Scene の構造を変更してよいかどうか。Entity の追加・削除・親の変更
		* がこれにあたる。
		*/
		static Bool EditableStructure(EditorContext& context, Bool request);

		/**
		* [EN]
		* Whether every selected actor may be changed, which is what the
		* gizmos ask before they move anything.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 選択中の Actor をすべて変更してよいかどうか。ギズモが何かを動かす
		* 前に確認するのがこれ。
		*/
		static Bool EditableSelection(EditorContext& context, Bool request);

		/**
		* [EN]
		* Draws who holds an actor beside its name in the hierarchy.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 階層で Actor の名前の横に、誰がそれを保持しているかを描く。
		*/
		static void DrawActorState(EditorContext& context, Actor actor);
	};
}
