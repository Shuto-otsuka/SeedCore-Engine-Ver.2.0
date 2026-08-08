#include <Editor/Editor/Panel/HierarchyPanel.h>
#include <Editor/Editor/EditorContext.h>
#include <Editor/Editor/ImGui/ImGuiTexture.h>
#include <FoundationEngine/ECS/World.h>
#include <FoundationEngine/ECS/Actor.h>
#include <FoundationEngine/ECS/Component/Name.h>
#include <FoundationEngine/ECS/ActorCommand.h>
#include <FoundationEngine/Resource/ActorSerialization.h>
#include <FoundationEngine/Resource/Prefab.h>
#include <FoundationEngine/Resource/ResourceCache.h>
#include <FoundationEngine/File/FileDialog.h>
#include <GraphicsEngine/Camera/EditorCamera.h>
#include <FoundationEngine/ECS/Component/Bounds.h>

namespace SeedCore
{
	HierarchyPanel::HierarchyPanel(EditorContext& context, ImGuiTexture& imguiTexture) : context_(context), imguiTexture_(imguiTexture)
	{
		/// No Code
	}

	void HierarchyPanel::Draw()
	{
		ImGuiID dockspaceID = ImGui::GetID("ScDockSpace");
		ImGui::SetNextWindowDockID(dockspaceID, ImGuiCond_FirstUseEver);

		if (ImGui::Begin("ヒエラルキー"))
		{
			rows_.clear();
			pendingClickActor_ = nullptr;

			const auto& actors = context_.worldContext_.world_->GetActors();
			for (Size index = 0; index < actors.size(); ++index)
			{
				if (actors[index]->GetParent() == nullptr)
				{
					DrawActorNode(actors[index].get());
				}
			}

			/// [EN] +20 covers the separator block above the button (Dummy 8 +
			///      Separator + Dummy 4 + spacings); +8 covers the Dummy added
			///      below the button for bottom margin. Keep this in sync with
			///      the actual content drawn after the drop-target area below.
			/// [JP] +20 はボタン上の区切りブロック（Dummy 8 + Separator +
			///      Dummy 4 + spacing 分）。+8 はボタン下に付けた余白用
			///      Dummy 分。この後のドロップターゲット領域より下で実際に
			///      描画する内容と食い違わないようにすること。
			Float buttonHeight = ImGui::GetFrameHeightWithSpacing() + 20.0f + 8.0f;
			ImVec2 remaining = ImGui::GetContentRegionAvail();
			remaining.y -= buttonHeight;
			if (remaining.y < 24.0f)
			{
				remaining.y = 24.0f;
			}
			ImGui::InvisibleButton("##HierarchyDrop", remaining);

			if (ImGui::BeginDragDropTarget())
			{
				if (const ImGuiPayload* preview = ImGui::AcceptDragDropPayload("HIERARCHY_ACTOR", ImGuiDragDropFlags_AcceptPeekOnly))
				{
					ImVec2 min = ImGui::GetItemRectMin();
					ImVec2 max = ImGui::GetItemRectMax();
					ImGui::GetWindowDrawList()->AddRectFilled(min, max, IM_COL32(100, 180, 255, 50));
					ImGui::GetWindowDrawList()->AddRect(min, max, IM_COL32(100, 180, 255, 200), 0.0f, 0, 2.0f);

					if (preview->IsDelivery())
					{
						Actor* dropped = *static_cast<Actor* const*>(preview->Data);
						Actor* oldParent = dropped->GetParent();
						Uint32 oldParentId = oldParent ? oldParent->GetPersistentID() : 0;
						dropped->SetParent(nullptr);
						context_.sceneContext_.history_.Push(MakePtr<ActorReparentCommand>(*context_.worldContext_.world_, dropped->GetPersistentID(), oldParentId, 0));
					}
				}

				if (const ImGuiPayload* prefabPayload = ImGui::AcceptDragDropPayload("ASSET_PREFAB"))
				{
					Uint32 assetID = *static_cast<const Uint32*>(prefabPayload->Data);
					Handle<Prefab> handle = context_.worldContext_.resource_->GetPrefabPool().Load(assetID, *context_.worldContext_.resource_);
					Prefab* prefab = context_.worldContext_.resource_->GetPrefabPool().Get(handle);
					if (prefab)
					{
						prefab->Instantiate(*context_.worldContext_.world_, *context_.worldContext_.resource_, nullptr, assetID);
					}
				}

				ImGui::EndDragDropTarget();
			}

			/// [EN] Marquee drag-select (Explorer-style): starting a plain left-press on
			///      this empty area begins a rubber-band select; dragging an existing
			///      Actor here (a HIERARCHY_ACTOR payload) is handled above instead, so
			///      this never fires mid-reparent-drag.
			/// [JP] マーキードラッグ選択（エクスプローラー風）: この空白領域での単純な
			///      左クリックはラバーバンド選択を開始する。既存の Actor をここに
			///      ドラッグする場合（HIERARCHY_ACTOR ペイロード）は上で別途処理される
			///      ため、再親付けドラッグ中にこれが発火することはない。
			if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::IsDragDropActive())
			{
				marqueeActive_ = true;
				marqueeStart_ = ImGui::GetMousePos();
			}

			if (marqueeActive_)
			{
				ImVec2 current = ImGui::GetMousePos();
				ImVec2 rectMin(Min(marqueeStart_.x, current.x), Min(marqueeStart_.y, current.y));
				ImVec2 rectMax(Max(marqueeStart_.x, current.x), Max(marqueeStart_.y, current.y));

				ImGui::GetWindowDrawList()->AddRectFilled(rectMin, rectMax, IM_COL32(100, 180, 255, 40));
				ImGui::GetWindowDrawList()->AddRect(rectMin, rectMax, IM_COL32(100, 180, 255, 200));

				if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
				{
					context_.selectionContext_.selectedActors_.clear();
					for (const RowRect& row : rows_)
					{
						Bool overlaps = !(row.max_.x < rectMin.x || row.min_.x > rectMax.x || row.max_.y < rectMin.y || row.min_.y > rectMax.y);
						if (overlaps)
						{
							context_.selectionContext_.selectedActors_.push_back(row.actor_);
						}
					}

					context_.selectionContext_.selectedActor_ = context_.selectionContext_.selectedActors_.empty() ? nullptr : context_.selectionContext_.selectedActors_.back();
					context_.selectionContext_.selectedEntity_ = context_.selectionContext_.selectedActor_ ? context_.selectionContext_.selectedActor_->GetEntity() : Entity::Null();
					rangeAnchor_ = context_.selectionContext_.selectedActor_;

					marqueeActive_ = false;
				}
			}

			if (pendingClickActor_)
			{
				HandleNodeSelection(pendingClickActor_, pendingClickCtrl_, pendingClickShift_);
				pendingClickActor_ = nullptr;
			}

			if (ImGui::BeginPopupContextItem("HierarchyContext"))
			{
				if (ImGui::MenuItem("空のActorを作成"))
				{
					String uniqueName = GetUniqueName();
					Actor* actor = context_.worldContext_.world_->CreateActor(uniqueName);
					DynamicArray<SerializedActorNode> nodes;
					CaptureActorNode(actor, -1, nodes);
					context_.sceneContext_.history_.Push(MakePtr<ActorCreateCommand>(*context_.worldContext_.world_, *context_.worldContext_.resource_, nodes));
				}
				ImGui::EndPopup();
			}

			if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows))
			{
				ImGuiIO& io = ImGui::GetIO();
				if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_D, false))
				{
					DuplicateSelection();
				}
				if (ImGui::IsKeyPressed(ImGuiKey_Delete, false))
				{
					DeleteSelection();
				}
			}

			ImGui::Dummy(ImVec2(0.0f, 8.0f));
			ImGui::Separator();
			ImGui::Dummy(ImVec2(0.0f, 4.0f));

			Float availableWidth = ImGui::GetContentRegionAvail().x;
			Float buttonWidth = 200.0f;
			ImGui::SetCursorPosX((availableWidth - buttonWidth) * 0.5f);

			if (ImGui::Button("空のActorを追加", ImVec2(buttonWidth, 0)))
			{
				String uniqueName = GetUniqueName();
				Actor* actor = context_.worldContext_.world_->CreateActor(uniqueName);
				DynamicArray<SerializedActorNode> nodes;
				CaptureActorNode(actor, -1, nodes);
				context_.sceneContext_.history_.Push(MakePtr<ActorCreateCommand>(*context_.worldContext_.world_, *context_.worldContext_.resource_, nodes));
			}

			ImGui::Dummy(ImVec2(0.0f, 8.0f));
		}
		ImGui::End();
	}

	void HierarchyPanel::DrawActorNode(Actor* actor)
	{
		const Char* label = "Actor";
		const Name* name = actor->GetComponent<Name>();
		if (name && !name->name_.str().empty())
		{
			label = name->name_.c_str();
		}

		Bool selected = IsSelected(actor);
		Bool hasChildren = !actor->GetChildren().empty();
		Bool isChild = actor->GetParent() != nullptr;

		ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_AllowOverlap;
		if (selected)
		{
			flags |= ImGuiTreeNodeFlags_Selected;
		}
		if (!hasChildren)
		{
			flags |= ImGuiTreeNodeFlags_Leaf;
		}

		ImGui::PushID(actor);

		ImTextureID icon;
		if (actor->IsPrefabInstance())
		{
			icon = isChild ? imguiTexture_.Icon(IconType::PrefabChild) : imguiTexture_.Icon(IconType::Prefab);
		}
		else
		{
			icon = isChild ? imguiTexture_.Icon(IconType::ActorChild) : imguiTexture_.Icon(IconType::Actor);
		}
		Float iconSize = ImGui::GetTextLineHeight();

		Bool opened = ImGui::TreeNodeEx("##actor", flags);
		Bool treeClicked = ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen();

		/// [EN] Unreal-style "frame selected": double-clicking an actor row
		///      slides the editor viewport camera to it (see EditorCamera::
		///      FocusOn). Uses the actor's cached world matrix translation
		///      rather than its Position component so it still works for
		///      children (world-space, parent chain composed in).
		/// [JP] Unreal 風の「選択対象にフレーム」: アクター行をダブルクリック
		///      するとエディタビューポートのカメラがそこへスライドする
		///      （EditorCamera::FocusOn 参照）。Position コンポーネントでは
		///      なくアクターのキャッシュ済みワールド行列の平行移動成分を
		///      使うため、子アクター（親チェーン込みのワールド空間）でも
		///      正しく動作する。
		if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && context_.cameraContext_.editorCamera_)
		{
			const Matrix& worldMatrix = actor->GetWorldMatrix();

			/// [EN] Bounds-fit dolly distance when this actor has a Mesh
			///      (auto-derived Bounds — see Bounds's class comment);
			///      otherwise FocusOn's radius<=0 default just pans in.
			/// [JP] このアクターに Mesh があれば（自動導出された Bounds —
			///      Bounds のクラスコメント参照）、それに合わせたドリー距離。
			///      無ければ FocusOn の radius<=0 デフォルトでパンのみ。
			Float radius = 0.0f;
			const Bounds* bounds = actor->GetComponent<Bounds>();
			if (bounds)
			{
				Float worldScale = Max(Max(Vector3(worldMatrix._11, worldMatrix._12, worldMatrix._13).Length(), Vector3(worldMatrix._21, worldMatrix._22, worldMatrix._23).Length()), Vector3(worldMatrix._31, worldMatrix._32, worldMatrix._33).Length());
				radius = bounds->extent_.Length() * worldScale;
			}

			context_.cameraContext_.editorCamera_->FocusOn(Vector3(worldMatrix._41, worldMatrix._42, worldMatrix._43), radius);
		}

		rows_.push_back({ actor, ImGui::GetItemRectMin(), ImGui::GetItemRectMax() });

		if (ImGui::BeginDragDropSource())
		{
			ImGui::SetDragDropPayload("HIERARCHY_ACTOR", &actor, sizeof(Actor*));
			ImGui::Text("%s", label);
			ImGui::EndDragDropSource();
		}

		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* preview = ImGui::AcceptDragDropPayload("HIERARCHY_ACTOR", ImGuiDragDropFlags_AcceptPeekOnly))
			{
				Actor* dropped = *static_cast<Actor* const*>(preview->Data);
				Bool canDrop = (dropped != actor && !actor->Descendant(dropped));

				ImVec2 min = ImGui::GetItemRectMin();
				ImVec2 max = ImGui::GetItemRectMax();

				if (!canDrop)
				{
					ImGui::GetWindowDrawList()->AddRect(min, max, IM_COL32(255, 60, 60, 255), 0.0f, 0, 2.0f);
				}
				else
				{
					ImGui::GetWindowDrawList()->AddRectFilled(min, max, IM_COL32(100, 180, 255, 50));
					ImGui::GetWindowDrawList()->AddRect(min, max, IM_COL32(100, 180, 255, 200), 0.0f, 0, 2.0f);

					if (preview->IsDelivery())
					{
						Actor* oldParent = dropped->GetParent();
						Uint32 oldParentId = oldParent ? oldParent->GetPersistentID() : 0;
						dropped->SetParent(actor);
						context_.sceneContext_.history_.Push(MakePtr<ActorReparentCommand>(*context_.worldContext_.world_, dropped->GetPersistentID(), oldParentId, actor->GetPersistentID()));
					}
				}
			}

			if (const ImGuiPayload* prefabPayload = ImGui::AcceptDragDropPayload("ASSET_PREFAB"))
			{
				Uint32 assetID = *static_cast<const Uint32*>(prefabPayload->Data);
				Handle<Prefab> handle = context_.worldContext_.resource_->GetPrefabPool().Load(assetID, *context_.worldContext_.resource_);
				Prefab* prefab = context_.worldContext_.resource_->GetPrefabPool().Get(handle);
				if (prefab)
				{
					prefab->Instantiate(*context_.worldContext_.world_, *context_.worldContext_.resource_, actor, assetID);
				}
			}

			ImGui::EndDragDropTarget();
		}

		if (ImGui::BeginPopupContextItem("##ActorContext"))
		{
			if (ImGui::MenuItem("Prefab として保存"))
			{
				SaveAsPrefab(actor);
			}
			if (ImGui::MenuItem("複製"))
			{
				if (!IsSelected(actor))
				{
					HandleNodeSelection(actor, false, false);
				}
				DuplicateSelection();
			}
			if (ImGui::MenuItem("Actorを削除"))
			{
				if (IsSelected(actor))
				{
					DeleteSelection();
				}
				else
				{
					DeleteActor(actor);
				}
				ImGui::EndPopup();
				if (opened)
				{
					ImGui::TreePop();
				}
				ImGui::PopID();
				return;
			}
			ImGui::EndPopup();
		}

		ImGui::SameLine();
		ImGui::Image(icon, ImVec2(iconSize, iconSize));
		ImGui::SameLine();
		if (!actor->IsActive())
		{
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
		}
		ImGui::Text("%s", label);
		if (!actor->IsActive())
		{
			ImGui::PopStyleColor();
		}

		if (treeClicked || ImGui::IsItemClicked())
		{
			pendingClickActor_ = actor;
			pendingClickCtrl_ = ImGui::GetIO().KeyCtrl;
			pendingClickShift_ = ImGui::GetIO().KeyShift;
		}

		ImGui::SameLine(ImGui::GetContentRegionAvail().x + ImGui::GetCursorPosX() - iconSize);
		ImTextureID activeIcon = actor->IsActive() ? imguiTexture_.Icon(IconType::ActorActive) : imguiTexture_.Icon(IconType::ActorNonActive);
		ImVec2 activePosition = ImGui::GetCursorScreenPos();
		Float activeYOffset = actor->IsActive() ? 2.0f : 0.0f;
		Float activeY = activePosition.y + (ImGui::GetTextLineHeight() - iconSize) * 0.5f + activeYOffset;
		if (ImGui::InvisibleButton("##Active", ImVec2(iconSize, ImGui::GetTextLineHeight())))
		{
			context_.sceneContext_.history_.Push(MakePtr<ActorActiveCommand>(*context_.worldContext_.world_, actor, !actor->IsActive()));
			actor->SetActive(!actor->IsActive());
		}
		ImGui::GetWindowDrawList()->AddImage(activeIcon, ImVec2(activePosition.x, activeY), ImVec2(activePosition.x + iconSize, activeY + iconSize));

		if (opened)
		{
			for (Actor* child : actor->GetChildren())
			{
				DrawActorNode(child);
			}
			ImGui::TreePop();
		}

		ImGui::PopID();
	}

	void HierarchyPanel::SaveAsPrefab(Actor* actor)
	{
		std::filesystem::path prefabDir = context_.worldContext_.resource_->ProjectRootPath() / "UserProject" / "Assets" / "Prefab";
		std::filesystem::create_directories(prefabDir);

		std::filesystem::path savedPath;
		if (!FileDialog::SaveFile(savedPath, prefabDir, L"Prefab Files (*.prefab)", L"*.prefab", L"prefab"))
		{
			return;
		}

		if (!Prefab::Save(actor, savedPath))
		{
			return;
		}

		context_.worldContext_.resource_->Reload(*context_.worldContext_.loader_, context_.graphicsContext_.device_, context_.graphicsContext_.cmdQueue_, *context_.graphicsContext_.bc7Shader_);

		/// [EN] Rebind this Actor's Apply target to the newly-saved file, so a later
		///      "Prefab に適用" writes to this new Prefab instead of any Prefab it
		///      may have originally been instantiated from.
		/// [JP] この Actor の適用先を、新しく保存されたファイルに再バインドする。
		///      これにより以降の「Prefab に適用」は、元々インスタンス化された Prefab
		///      ではなく、この新しい Prefab に書き込まれる。
		std::string relative = std::filesystem::relative(savedPath, context_.worldContext_.resource_->ProjectRootPath()).string();
		std::replace(relative.begin(), relative.end(), '\\', '/');
		Uint32 newAssetID = context_.worldContext_.resource_->GetAssetID(String(relative));
		if (newAssetID != 0)
		{
			actor->SetSourcePrefabAssetID(newAssetID);
		}
	}

	String HierarchyPanel::GetUniqueName()
	{
		String baseName = String("Empty Actor");
		String uniqueName = baseName;

		Int index = 1;

		while (true)
		{
			Bool exists = false;

			for (auto& actor : context_.worldContext_.world_->GetActors())
			{
				const Name* name = actor->GetComponent<Name>();
				if (!name)
				{
					continue;
				}

				if (name->name_ == uniqueName)
				{
					exists = true;
					break;
				}
			}

			if (!exists)
			{
				break;
			}

			uniqueName = String(baseName.str() + "(" + std::to_string(index++) + ")");
		}

		return uniqueName;
	}

	Bool HierarchyPanel::IsSelected(Actor* actor)const
	{
		return std::find(context_.selectionContext_.selectedActors_.begin(), context_.selectionContext_.selectedActors_.end(), actor) != context_.selectionContext_.selectedActors_.end();
	}

	void HierarchyPanel::HandleNodeSelection(Actor* actor, Bool ctrl, Bool shift)
	{
		if (shift && rangeAnchor_)
		{
			Size anchorIndex = SIZE_MAX;
			Size targetIndex = SIZE_MAX;

			for (Size rowIndex = 0; rowIndex < rows_.size(); ++rowIndex)
			{
				if (rows_[rowIndex].actor_ == rangeAnchor_)
				{
					anchorIndex = rowIndex;
				}
				if (rows_[rowIndex].actor_ == actor)
				{
					targetIndex = rowIndex;
				}
			}

			if (anchorIndex != SIZE_MAX && targetIndex != SIZE_MAX)
			{
				Size lo = Min(anchorIndex, targetIndex);
				Size hi = Max(anchorIndex, targetIndex);

				context_.selectionContext_.selectedActors_.clear();
				for (Size rowIndex = lo; rowIndex <= hi; ++rowIndex)
				{
					context_.selectionContext_.selectedActors_.push_back(rows_[rowIndex].actor_);
				}
			}
		}
		else if (ctrl)
		{
			auto it = std::find(context_.selectionContext_.selectedActors_.begin(), context_.selectionContext_.selectedActors_.end(), actor);
			if (it != context_.selectionContext_.selectedActors_.end())
			{
				context_.selectionContext_.selectedActors_.erase(it);
			}
			else
			{
				context_.selectionContext_.selectedActors_.push_back(actor);
			}
			rangeAnchor_ = actor;
		}
		else
		{
			context_.selectionContext_.selectedActors_.clear();
			context_.selectionContext_.selectedActors_.push_back(actor);
			rangeAnchor_ = actor;
		}

		context_.selectionContext_.selectedActor_ = context_.selectionContext_.selectedActors_.empty() ? nullptr : context_.selectionContext_.selectedActors_.back();
		context_.selectionContext_.selectedEntity_ = context_.selectionContext_.selectedActor_ ? context_.selectionContext_.selectedActor_->GetEntity() : Entity::Null();
	}

	void HierarchyPanel::DeleteActor(Actor* actor)
	{
		auto it = std::find(context_.selectionContext_.selectedActors_.begin(), context_.selectionContext_.selectedActors_.end(), actor);
		if (it != context_.selectionContext_.selectedActors_.end())
		{
			context_.selectionContext_.selectedActors_.erase(it);
		}

		if (context_.selectionContext_.selectedActor_ == actor)
		{
			context_.selectionContext_.selectedActor_ = context_.selectionContext_.selectedActors_.empty() ? nullptr : context_.selectionContext_.selectedActors_.back();
			context_.selectionContext_.selectedEntity_ = context_.selectionContext_.selectedActor_ ? context_.selectionContext_.selectedActor_->GetEntity() : Entity::Null();
		}

		context_.sceneContext_.history_.Push(MakePtr<ActorDeleteCommand>(*context_.worldContext_.world_, *context_.worldContext_.resource_, actor));
		context_.worldContext_.world_->DestroyActor(actor);
	}

	void HierarchyPanel::DeleteSelection()
	{
		DynamicArray<Actor*> toDelete = context_.selectionContext_.selectedActors_;
		for (Actor* actor : toDelete)
		{
			DeleteActor(actor);
		}
	}

	void HierarchyPanel::DuplicateSelection()
	{
		if (context_.selectionContext_.selectedActors_.empty())
		{
			return;
		}

		DynamicArray<Actor*> toDuplicate = context_.selectionContext_.selectedActors_;
		DynamicArray<Actor*> newSelection;

		for (Actor* actor : toDuplicate)
		{
			/// [EN] Skip Actors whose ancestor is also selected - they'll already be
			///      duplicated as part of that ancestor's subtree.
			/// [JP] 祖先も選択されている Actor はスキップする — その祖先のサブツリーの
			///      一部として既に複製されるため。
			Bool ancestorSelected = false;
			for (Actor* other : toDuplicate)
			{
				if (other != actor && actor->Descendant(other))
				{
					ancestorSelected = true;
					break;
				}
			}
			if (ancestorSelected)
			{
				continue;
			}

			Prefab prefab;
			prefab.Capture(actor);

			Actor* duplicate = prefab.Instantiate(*context_.worldContext_.world_, *context_.worldContext_.resource_, actor->GetParent(), actor->GetSourcePrefabAssetID());
			if (!duplicate)
			{
				continue;
			}

			DynamicArray<SerializedActorNode> nodes;
			CaptureActorNode(duplicate, -1, nodes);
			Actor* duplicateParent = duplicate->GetParent();
			context_.sceneContext_.history_.Push(MakePtr<ActorCreateCommand>(*context_.worldContext_.world_, *context_.worldContext_.resource_, nodes, duplicateParent ? duplicateParent->GetPersistentID() : 0));

			MoveAfter(duplicate, actor);
			newSelection.push_back(duplicate);
		}

		context_.selectionContext_.selectedActors_ = newSelection;
		context_.selectionContext_.selectedActor_ = newSelection.empty() ? nullptr : newSelection.back();
		context_.selectionContext_.selectedEntity_ = context_.selectionContext_.selectedActor_ ? context_.selectionContext_.selectedActor_->GetEntity() : Entity::Null();
		rangeAnchor_ = context_.selectionContext_.selectedActor_;
	}

	void HierarchyPanel::MoveAfter(Actor* actor, Actor* after)
	{
		Actor* parent = actor->GetParent();
		if (parent)
		{
			parent->MoveChildAfter(actor, after);
			return;
		}

		auto& actors = context_.worldContext_.world_->GetActors();

		auto actorIt = std::find_if(actors.begin(), actors.end(), [actor](const ResourcePtr<Actor>& a) { return a.get() == actor; });
		if (actorIt == actors.end())
		{
			return;
		}

		ResourcePtr<Actor> moved = std::move(*actorIt);
		actors.erase(actorIt);

		auto afterIt = std::find_if(actors.begin(), actors.end(), [after](const ResourcePtr<Actor>& a) { return a.get() == after; });
		if (afterIt == actors.end())
		{
			actors.push_back(std::move(moved));
		}
		else
		{
			actors.insert(afterIt + 1, std::move(moved));
		}
	}
}
