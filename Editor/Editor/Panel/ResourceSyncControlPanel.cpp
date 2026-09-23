#include <Editor/Editor/Panel/ResourceSyncControlPanel.h>
#include <Editor/Editor/EditorContext.h>
#include <FoundationEngine/World/Actor/Actor.h>

namespace SeedCore
{
	/**
	* [EN]
	* Draws the one-line state of the shared library at the top of the
	* content browser: whether it is reachable, what it is doing, and
	* anything that went wrong.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* コンテンツブラウザの上部に、共有ライブラリの状態を1行で描く。到達
	* できているか、今何をしているか、何か問題が起きていないか。
	*/
	void ResourceSyncControlPanel::DrawStatus(EditorContext& context)
	{
		ResourceSync& sync = *context.resourceSync_;
		if (!sync.Configured())
		{
			return;
		}

		ImGui::TextUnformatted(sync.Online() ? "共有ライブラリ: 接続中" : "共有ライブラリ: 未接続");
		ImGui::SameLine();
		if (ImGui::SmallButton("最新を確認"))
		{
			sync.RequestRefresh();
		}

		/// [EN] What the worker is doing is shown while it runs, because an upload can take long enough to look like a hang.
		/// [JP] 実行中の内容を表示する。アップロードは、固まって見えるほど時間がかかることがあるため。
		if (!sync.Operation().str().empty())
		{
			ImGui::SameLine();
			ImGui::TextDisabled("%s", sync.Operation().c_str());
		}
		if (!sync.Error().str().empty())
		{
			ImGui::TextWrapped("%s", sync.Error().c_str());
		}
	}

	/**
	* [EN]
	* Adds the shared-library entries to an asset's context menu, which
	* differ depending on whether the team already has it.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* アセットの右クリックメニューへ、共有ライブラリ用の項目を足す。
	* 内容は、チームが既にそれを持っているかどうかで変わる。
	*/
	void ResourceSyncControlPanel::DrawActions(EditorContext& context, const AssetRecord& asset)
	{
		ResourceSync& sync = *context.resourceSync_;
		if (!sync.Configured())
		{
			return;
		}

		/// [EN] Every entry is disabled while the library is out of reach, since each one needs it to answer.
		/// [JP] ライブラリへ届いていない間は全ての項目を無効にする。どれも応答を必要とするため。
		ImGui::BeginDisabled(!sync.Online());
		const SharedAsset* shared = sync.GetAsset(asset.assetID_);
		if (!shared)
		{
			if (ImGui::MenuItem("チームへ共有"))
			{
				sync.RequestRegister(asset);
			}
		}
		else
		{
			DrawState(context, asset);
			if (ImGui::MenuItem("取得 / 更新"))
			{
				sync.RequestGet(asset.assetID_);
			}

			/// [EN] A scene is checked out one entity at a time, so taking the whole file would defeat working in it together.
			/// [JP] Scene は Entity 単位で編集権を取るため、ファイル全体を取ってしまうと共同作業の意味が無くなる。
			if (!shared->scene_ && ImGui::MenuItem("編集権を取得"))
			{
				sync.RequestEdit(asset.assetID_, String("asset"));
			}
			if (ImGui::MenuItem("変更を公開"))
			{
				sync.RequestPublish(asset.assetID_);
			}
			if (ImGui::MenuItem("編集権を解放"))
			{
				sync.RequestRelease(asset.assetID_, String("asset"));
			}
			if (ImGui::BeginMenu("共有を終了..."))
			{
				ImGui::TextUnformatted("チーム全体から見えなくなります。");
				if (ImGui::MenuItem("実行する"))
				{
					sync.RequestRetire(asset.assetID_);
				}
				ImGui::EndMenu();
			}
		}
		ImGui::EndDisabled();
		ImGui::Separator();
	}

	/**
	* [EN]
	* Draws an asset's revision and who is currently editing it, for the
	* details pane and the context menu.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* アセットの Revision と、今それを誰が編集しているかを描く。詳細欄と
	* 右クリックメニューで使う。
	*/
	void ResourceSyncControlPanel::DrawState(EditorContext& context, const AssetRecord& asset)
	{
		ResourceSync& sync = *context.resourceSync_;
		const SharedAsset* shared = sync.GetAsset(asset.assetID_);
		if (!shared)
		{
			return;
		}

		/// [EN] The revision is what a member compares when deciding whether their copy is the current one.
		/// [JP] Revision は、手元の写しが最新かどうかをメンバーが判断する際の比較対象になる。
		ImGui::Text("共有 / r%llu", shared->revision_);
		if (sync.RemoteOnly(asset.assetID_))
		{
			ImGui::TextDisabled("この PC にはまだありません");
		}

		/// [EN] The conflict is stated instead of the plain unsent-work line, since what to do about it differs.
		/// [JP] 競合のときは単なる未送信の表示に代えてこちらを出す。取るべき対応が違うため。
		if (sync.Conflicted(asset.assetID_))
		{
			ImGui::Text("競合: こちらとライブラリの両方が変わっています");
		}
		else if (sync.Modified(asset.assetID_))
		{
			ImGui::Text("未公開の変更があります");
		}

		for (const EditLease& lease : sync.GetLeases())
		{
			if (lease.assetId_ == shared->id_)
			{
				ImGui::Text("編集中: %s (%s)", lease.owner_.c_str(), lease.scope_.c_str());
			}
		}
	}

	/**
	* [EN]
	* Whether the given actor may be changed, asking for the right to
	* edit it when request says the member is reaching for it.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* その Actor を変更してよいかどうか。request が立っていれば、メンバーが
	* 操作しようとしているとみなして編集権を要求する。
	*/
	Bool ResourceSyncControlPanel::EditableActor(EditorContext& context, Actor actor, Bool request)
	{
		ResourceSync& sync = *context.resourceSync_;

		/// [EN] A scene that is not shared, or no scene at all, is nobody else's business.
		/// [JP] 共有されていない Scene、あるいは Scene が開かれていない場合は、他の誰にも関係しない。
		const SharedAsset* scene = sync.GetAsset(context.sceneContext_.currentScenePath_);
		if (!actor || !scene)
		{
			return true;
		}

		/// [EN] The entity's own identifier is what the lease is on, so renaming or reparenting it changes nothing here.
		/// [JP] Lease が結び付くのは Entity 自身の識別子。リネームや親の変更をしても、ここでの扱いは変わらない。
		String scope = String(std::format("entity:{}", actor.CollaborationID().str()));
		if (request)
		{
			sync.RequestEdit(scene->runtimeId_, scope);
		}
		return sync.Editable(scene->runtimeId_, scope);
	}

	/**
	* [EN]
	* Whether the scene's own structure may be changed, which covers
	* adding, removing and reparenting entities.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* Scene の構造を変更してよいかどうか。Entity の追加・削除・親の変更が
	* これにあたる。
	*/
	Bool ResourceSyncControlPanel::EditableStructure(EditorContext& context, Bool request)
	{
		ResourceSync& sync = *context.resourceSync_;
		const SharedAsset* scene = sync.GetAsset(context.sceneContext_.currentScenePath_);
		if (!scene)
		{
			return true;
		}

		/// [EN] One structure lease covers the whole hierarchy, because moving one entity can change another's place in it.
		/// [JP] 構造の Lease は階層全体を対象にする。1つの Entity を動かすと、他の Entity の位置も変わり得るため。
		if (request)
		{
			sync.RequestEdit(scene->runtimeId_, String("structure"));
		}
		return sync.Editable(scene->runtimeId_, String("structure"));
	}

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
	Bool ResourceSyncControlPanel::EditableSelection(EditorContext& context, Bool request)
	{
		/// [EN] The loop is not cut short, so a member dragging several actors asks for all of them rather than the first.
		/// [JP] 途中で打ち切らない。複数の Actor を掴んでいるメンバーが、先頭だけでなく全てについて要求できるようにするため。
		Bool editable = true;
		for (Actor actor : context.selectionContext_.selectedActors_)
		{
			editable = EditableActor(context, actor, request) && editable;
		}
		return editable;
	}

	/**
	* [EN]
	* Draws who holds an actor beside its name in the hierarchy, so the
	* team's work in one scene is visible at a glance.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 階層で Actor の名前の横に、誰がそれを保持しているかを描く。1つの
	* Scene 内でのチームの作業が一目で分かるようにするため。
	*/
	void ResourceSyncControlPanel::DrawActorState(EditorContext& context, Actor actor)
	{
		ResourceSync& sync = *context.resourceSync_;
		const SharedAsset* scene = sync.GetAsset(context.sceneContext_.currentScenePath_);
		if (!scene)
		{
			return;
		}

		/// [EN] A structure lease is shown on every actor, since while it is held nobody else may rearrange any of them.
		/// [JP] 構造の Lease は全ての Actor に表示する。保持されている間は、誰も並べ替えられないため。
		String scope = String(std::format("entity:{}", actor.CollaborationID().str()));
		for (const EditLease& lease : sync.GetLeases())
		{
			if (lease.assetId_ != scene->id_ || lease.mine_)
			{
				continue;
			}
			if (lease.scope_ == scope || lease.scope_ == String("structure"))
			{
				ImGui::SameLine();
				ImGui::TextDisabled("[%s]", lease.owner_.c_str());
			}
		}
	}
}
