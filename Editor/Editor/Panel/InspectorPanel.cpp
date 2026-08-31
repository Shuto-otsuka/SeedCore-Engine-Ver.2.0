#include <Editor/Editor/Panel/InspectorPanel.h>
#include <Editor/Editor/EditorContext.h>
#include <Editor/Editor/ImGui/ImGuiTexture.h>
#include <Editor/Editor/Panel/AnimatorControllerPanel.h>
#include <Editor/Editor/Panel/TimelinePanel.h>
#include <Editor/Editor/Panel/LayerSettingsPanel.h>
#include <Editor/Editor/Panel/MaterialViewerPanel.h>
#include <FoundationEngine/ECS/World.h>
#include <FoundationEngine/ECS/Actor.h>
#include <FoundationEngine/ECS/Component.h>
#include <FoundationEngine/ECS/Component/Name.h>
#include <FoundationEngine/ECS/ComponentRegistry.h>
#include <FoundationEngine/ECS/ComponentCommand.h>
#include <FoundationEngine/ECS/ComponentLifecycleCommand.h>
#include <FoundationEngine/ECS/ArrayFieldCommand.h>
#include <FoundationEngine/ECS/ActorCommand.h>
#include <FoundationEngine/ECS/ReflectionRegistry.h>
#include <FoundationEngine/ECS/PayloadRegistry.h>
#include <FoundationEngine/ECS/TagRegistry.h>
#include <FoundationEngine/ECS/LayerRegistry.h>
#include <FoundationEngine/Resource/ResourceCache.h>
#include <FoundationEngine/Resource/LoaderSystem.h>
#include <FoundationEngine/Resource/Prefab.h>
#include <FoundationEngine/Time/GameTimer.h>
#include <GraphicsEngine/Model/Mesh.h>
#include <GraphicsEngine/Model/ModelResource.h>
#include <GraphicsEngine/Model/Material/Material.h>
#include <GraphicsEngine/Model/Crister.h>

namespace SeedCore
{
	InspectorPanel::InspectorPanel(EditorContext& context, ImGuiTexture& imguiTexture) : context_(context), addComponentPanel_(context), imguiTexture_(imguiTexture)
	{
		newTagBuffer_.resize(64);

		layerNameBuffers_.resize(LayerRegistry::LayerCount);
		for (std::string& buffer : layerNameBuffers_)
		{
			buffer.resize(64);
		}
	}

	void InspectorPanel::Draw()
	{
		ImGuiID dockspaceID = ImGui::GetID("ScDockSpace");
		ImGui::SetNextWindowDockID(dockspaceID, ImGuiCond_FirstUseEver);

		if (ImGui::Begin("インスペクター"))
		{
			if (context_.panelContext_.animatorControllerPanel_ && context_.panelContext_.animatorControllerPanel_->IsFocused())
			{
				context_.panelContext_.animatorControllerPanel_->DrawDetails();
				ImGui::End();
				return;
			}

			if (context_.panelContext_.timelinePanel_ && context_.panelContext_.timelinePanel_->IsFocused())
			{
				context_.panelContext_.timelinePanel_->DrawDetails();
				ImGui::End();
				return;
			}

			if (context_.panelContext_.materialViewerPanel_ && context_.panelContext_.materialViewerPanel_->IsFocused())
			{
				context_.panelContext_.materialViewerPanel_->DrawDetails();
				ImGui::End();
				return;
			}

			if (locked_ && !lockedActor_)
			{
				locked_ = false;
				lockedActor_ = Actor();
			}

			Actor actor = locked_ ? lockedActor_ : context_.selectionContext_.selectedActor_;

			if (actor && actor.GetEntity().Exists())
			{
				ImGui::BeginChild("##InspectorContent", ImVec2(0, 0), ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar);
				DrawName(actor);

				Float tagLayerColumnWidth = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;

				ImGui::BeginChild("##TagColumn", ImVec2(tagLayerColumnWidth, 0.0f), ImGuiChildFlags_AutoResizeY);
				DrawTags(actor);
				ImGui::EndChild();

				ImGui::SameLine();

				ImGui::BeginChild("##LayerColumn", ImVec2(tagLayerColumnWidth, 0.0f), ImGuiChildFlags_AutoResizeY);
				DrawLayer(actor);
				ImGui::EndChild();

				DrawPrefabControls(actor);
				ImGui::Separator();

				Bool disabled = !actor.IsActive();
				if (disabled)
				{
					ImGui::BeginDisabled();
				}

				DrawComponents(actor);
				ImGui::Separator();
				addComponentPanel_.Draw(actor, imguiTexture_);

				if (disabled)
				{
					ImGui::EndDisabled();
				}

				ImGui::EndChild();
			}
			else if (locked_)
			{
				locked_ = false;
				lockedActor_ = Actor();
			}
		}

		ImGui::End();
	}

	void InspectorPanel::DrawName(Actor actor)
	{
		Float iconSize = ImGui::GetTextLineHeight();
		ImTextureID lockIcon = locked_ ? imguiTexture_.Icon(IconType::Lock) : imguiTexture_.Icon(IconType::LockFree);

		if (ImGui::ImageButton("##Lock", lockIcon, ImVec2(iconSize, iconSize)))
		{
			locked_ = !locked_;
			lockedActor_ = locked_ ? actor : Actor();
		}

		ImGui::SameLine();

		Name* nameComponent = static_cast<Name*>(context_.worldContext_.world_->GetComponent(actor.GetEntity(), ComponentRegistry::GetComponentID<Name>()));
		if (!nameComponent)
		{
			return;
		}

		std::string nameBuffer = nameComponent->name_.str();
		nameBuffer.resize(256);

		if (ImGui::InputText("名前", nameBuffer.data(), nameBuffer.capacity(), ImGuiInputTextFlags_EnterReturnsTrue))
		{
			String oldValue = nameComponent->name_;
			nameComponent->name_ = String(std::string_view(nameBuffer.c_str()));
			context_.sceneContext_.history_.Push(MakePtr<ComponentCommand<String>>(*context_.worldContext_.world_, actor.GetEntity(), ComponentRegistry::GetComponentID<Name>(), 0, oldValue, nameComponent->name_));
		}

		ImGui::SameLine();

		Bool active = actor.IsActive();
		if (ImGui::Checkbox("有効", &active))
		{
			context_.sceneContext_.history_.Push(MakePtr<ActorActiveCommand>(*context_.worldContext_.world_, actor, active));
			actor.SetActive(active);
		}
	}

	void InspectorPanel::DrawTags(Actor actor)
	{
		DynamicArray<String> currentTags = actor.GetTagList();

		std::string previewLabel;
		for (Size index = 0; index < currentTags.size(); ++index)
		{
			if (index > 0)
			{
				previewLabel += ", ";
			}
			previewLabel += currentTags[index].str();
		}
		if (previewLabel.empty())
		{
			previewLabel = "(なし)";
		}

		ImGui::TextDisabled("タグ");

		ImGui::SetNextItemWidth(-1.0f);
		if (!ImGui::BeginCombo("##Tags", previewLabel.c_str()))
		{
			return;
		}

		String removeTag;
		Bool hasRemoveTag = false;
		String deleteTag;
		Bool hasDeleteTag = false;

		if (!currentTags.empty())
		{
			const ImGuiStyle& style = ImGui::GetStyle();
			Float windowVisibleX = ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;

			for (Size index = 0; index < currentTags.size(); ++index)
			{
				const String& tag = currentTags[index];

				ImGui::PushID(tag.c_str());
				ImGui::SmallButton(tag.c_str());

				if (ImGui::BeginPopupContextItem())
				{
					if (ImGui::MenuItem("このActorから外す"))
					{
						removeTag = tag;
						hasRemoveTag = true;
					}
					if (ImGui::MenuItem("タグを削除（すべてのActorから）"))
					{
						deleteTag = tag;
						hasDeleteTag = true;
					}
					ImGui::EndPopup();
				}
				else if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("右クリックでメニューを開く");
				}

				ImGui::PopID();

				Float lastButtonX = ImGui::GetItemRectMax().x;
				if (index + 1 < currentTags.size())
				{
					const String& nextTag = currentTags[index + 1];
					Float nextButtonWidth = ImGui::CalcTextSize(nextTag.c_str()).x + style.FramePadding.x * 2.0f;
					Float nextButtonX = lastButtonX + style.ItemSpacing.x + nextButtonWidth;
					if (nextButtonX < windowVisibleX)
					{
						ImGui::SameLine();
					}
				}
			}

			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();
		}

		if (hasRemoveTag)
		{
			context_.sceneContext_.history_.Push(MakePtr<ActorTagCommand>(*context_.worldContext_.world_, actor.GetPersistentID(), removeTag, false));
			actor.RemoveTag(removeTag);
		}
		if (hasDeleteTag)
		{
			TagRegistry::Remove(deleteTag);
		}

		ImGui::SetNextItemWidth(140.0f);
		Bool entered = ImGui::InputText("##NewTag", newTagBuffer_.data(), newTagBuffer_.capacity(), ImGuiInputTextFlags_EnterReturnsTrue);
		ImGui::SameLine();
		Bool addClicked = ImGui::SmallButton("追加");

		if (entered || addClicked)
		{
			std::string text(newTagBuffer_.c_str());
			if (!text.empty())
			{
				String newTag = String(std::string_view(text));
				context_.sceneContext_.history_.Push(MakePtr<ActorTagCommand>(*context_.worldContext_.world_, actor.GetPersistentID(), newTag, true));
				actor.AddTag(newTag);
			}
			std::ranges::fill(newTagBuffer_, '\0');
			ImGui::SetKeyboardFocusHere(-1);
		}

		const DynamicArray<String>& allNames = TagRegistry::GetNames();

		Bool hasActiveTag = std::ranges::any_of(std::views::iota(Size{ 0 }, allNames.size()), [](Size index) { return !TagRegistry::IsRemoved(index); });

		if (hasActiveTag)
		{
			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();

			for (Size index = 0; index < allNames.size(); ++index)
			{
				if (TagRegistry::IsRemoved(index))
				{
					continue;
				}

				const String& tag = allNames[index];

				ImGui::PushID(static_cast<Int>(index));

				Bool hasTag = actor.HasTag(tag);
				if (ImGui::Checkbox(tag.c_str(), &hasTag))
				{
					context_.sceneContext_.history_.Push(MakePtr<ActorTagCommand>(*context_.worldContext_.world_, actor.GetPersistentID(), tag, hasTag));
					if (hasTag)
					{
						actor.AddTag(tag);
					}
					else
					{
						actor.RemoveTag(tag);
					}
				}

				if (ImGui::BeginPopupContextItem())
				{
					if (ImGui::MenuItem("タグを削除（すべてのActorから）"))
					{
						actor.RemoveTag(tag);
						TagRegistry::Remove(tag);
					}
					ImGui::EndPopup();
				}

				ImGui::PopID();
			}
		}

		ImGui::EndCombo();
	}

	void InspectorPanel::DrawLayer(Actor actor)
	{
		const DynamicArray<String>& layerNames = LayerRegistry::GetNames();
		if (layerNames.empty())
		{
			return;
		}

		Size currentLayer = actor.GetLayer();
		if (currentLayer >= layerNames.size())
		{
			currentLayer = 0;
		}

		ImGui::TextDisabled("レイヤー");

		ImGui::SetNextItemWidth(-1.0f);
		if (ImGui::BeginCombo("##Layer", layerNames[currentLayer].c_str()))
		{
			/// [EN] The combo's popup is its own ImGui window - refresh the
			///      edit buffers from LayerRegistry only on the frame it
			///      opens, not every frame, so an in-progress edit isn't
			///      overwritten by its own unsubmitted keystrokes.
			/// [JP] コンボのポップアップはそれ自体が1つのImGuiウィンドウ -
			///      毎フレームではなく開いたフレームだけ編集バッファを
			///      LayerRegistry から再読込する。そうしないと入力中の文字が
			///      自分自身の未確定な入力で上書きされてしまう。
			if (ImGui::IsWindowAppearing())
			{
				for (Size index = 0; index < LayerRegistry::LayerCount; ++index)
				{
					std::ranges::fill(layerNameBuffers_[index], '\0');
					std::string name = layerNames[index].str();
					std::ranges::copy(name, layerNameBuffers_[index].begin());
				}
			}

			for (Size index = 0; index < LayerRegistry::LayerCount; ++index)
			{
				ImGui::PushID(static_cast<Int>(index));

				Bool isSelected = (index == currentLayer);
				if (ImGui::Checkbox("##Select", &isSelected) && isSelected)
				{
					String oldLayerName = actor.GetLayerName();
					String newLayerName = layerNames[index];
					context_.sceneContext_.history_.Push(MakePtr<ActorLayerCommand>(*context_.worldContext_.world_, actor.GetPersistentID(), oldLayerName, newLayerName));
					actor.SetLayer(index);
				}

				ImGui::SameLine();
				ImGui::SetNextItemWidth(140.0f);

				if (index == LayerRegistry::DefaultLayer)
				{
					ImGui::BeginDisabled();
					ImGui::InputText("##Name", layerNameBuffers_[index].data(), layerNameBuffers_[index].capacity());
					ImGui::EndDisabled();
				}
				else if (ImGui::InputText("##Name", layerNameBuffers_[index].data(), layerNameBuffers_[index].capacity()))
				{
					LayerRegistry::SetName(index, String(std::string_view(layerNameBuffers_[index].c_str())));
					LayerRegistry::Save();
				}

				ImGui::PopID();
			}

			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();

			if (ImGui::Selectable("編集") && context_.panelContext_.layerSettingsPanel_)
			{
				context_.panelContext_.layerSettingsPanel_->Open();
			}

			ImGui::EndCombo();
		}
	}

	void InspectorPanel::DrawPrefabControls(Actor actor)
	{
		Uint32 assetID = actor.GetSourcePrefabAssetID();
		if (assetID == 0)
		{
			return;
		}

		Asset* asset = context_.worldContext_.resource_->GetAsset(assetID);
		if (!asset)
		{
			return;
		}

		ImGui::Text("Prefab: %s", asset->path_.c_str());

		Bool isPlaying = context_.worldContext_.gameTimer_->IsPlaying();
		if (isPlaying)
		{
			ImGui::BeginDisabled();
		}

		if (ImGui::Button("Prefab に適用"))
		{
			Handle<Prefab> handle = context_.worldContext_.resource_->GetPrefabPool().Load(assetID, *context_.worldContext_.resource_);
			Prefab* prefab = context_.worldContext_.resource_->GetPrefabPool().Get(handle);
			if (prefab)
			{
				prefab->Capture(actor);
				prefab->Write(asset->fullpath_.c_str());
			}
		}

		if (isPlaying)
		{
			ImGui::EndDisabled();
		}
	}

	/**
	* [EN]
	* Draws one component's header (with the delete-component context menu)
	* and, if expanded, its reflected fields. Shared by DrawComponents'
	* archetype-layout loop and its sparse-set loop below, since both need the
	* identical header/popup/fields sequence and differ only in how they
	* discovered componentID/componentData.
	*
	* Returns true when the component was removed this frame, so the caller
	* can stop iterating its own component list immediately rather than
	* continuing to reference now-stale data.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 1コンポーネントぶんのヘッダー(削除用コンテキストメニュー付き)と、
	* 開いていればそのリフレクションフィールドを描画する。下の
	* DrawComponents のアーキタイプ一覧ループとスパースセットループの両方が
	* 共有する — どちらも componentID/componentData の見つけ方が違うだけで、
	* ヘッダー/ポップアップ/フィールド描画の並びは同一なため。
	*
	* このフレームでコンポーネントが削除された場合は true を返す。呼び出し側は
	* 古くなったデータを参照し続けないよう、自分のコンポーネント一覧の走査を
	* 即座に打ち切ること。
	*/
	Bool InspectorPanel::DrawComponentEntry(Actor actor, ComponentID componentID, const String& componentName, void* componentData)
	{
		/// [EN] CollapsingHeader always draws its own label starting at the
		///      left edge and ignores a preceding SameLine(), so an icon
		///      placed before it (like Unity's "▽ [icon] Name" row) would
		///      land on its own line instead of inline. Use the same
		///      TreeNodeEx + AllowOverlap technique HierarchyPanel already
		///      uses for Actor rows instead: an empty-label header (arrow
		///      only) reserves the row, then the icon/text are drawn
		///      overlapping it via SameLine().
		/// [JP] CollapsingHeader は常に自前のラベルを左端から描画し、直前の
		///      SameLine() を無視するため、その前にアイコンを置いても
		///      （Unity の「▽ [icon] Name」行のように）別行になってしまう。
		///      HierarchyPanel が Actor 行で既に使っている
		///      TreeNodeEx + AllowOverlap の手法に合わせる: 空ラベルの
		///      ヘッダー（矢印のみ）で行を確保し、アイコン/テキストは
		///      SameLine() でその上に重ねて描画する。
		ImGui::PushID(componentData);
		Bool isHeaderOpen = ImGui::TreeNodeEx("##header", ImGuiTreeNodeFlags_CollapsingHeader | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_AllowOverlap | ImGuiTreeNodeFlags_DefaultOpen);

		/// [EN] Bind the right-click menu to the header item itself (the
		///      "last item" BeginPopupContextItem defaults to) before
		///      drawing the icon/text overlay, so the overlay doesn't
		///      become the new "last item" instead.
		/// [JP] 右クリックメニューは、アイコン/テキストの重ね描画を行う前に
		///      ヘッダー自身（BeginPopupContextItem が既定で使う「直前の
		///      アイテム」）へ結び付ける。そうしないと重ね描画の方が
		///      新しい「直前のアイテム」になってしまう。
		Bool removed = false;
		if (ImGui::BeginPopupContextItem())
		{
			if (ImGui::MenuItem("コンポーネントを削除"))
			{
				context_.sceneContext_.history_.Push(MakePtr<ComponentRemoveCommand>(*context_.worldContext_.world_, actor.GetPersistentID(), componentID, componentName, componentData));
				actor.RemoveComponent(componentID);
				removed = true;
			}
			ImGui::EndPopup();
		}

		/// [EN] ImGui::Image() top-aligns to the cursor, but the header row
		///      is taller than the icon (frame padding above/below), so a
		///      plain SameLine()+Image()+Text() sits the icon above center
		///      relative to the text. Measure the header's own item rect
		///      and place both the icon and the text into it by hand via
		///      the draw list (same technique DrawSearchBar already uses
		///      for its search icon) so both land on the row's true
		///      vertical center regardless of frame padding.
		/// [JP] ImGui::Image() はカーソル位置に対してトップ揃えで描画されるが、
		///      ヘッダー行自体はアイコンより背が高い（上下にフレーム
		///      パディングがある）ため、単純な SameLine()+Image()+Text()
		///      だとアイコンがテキストより上寄りになる。ヘッダー自身の
		///      アイテム矩形を測り、アイコンとテキストの両方を DrawList で
		///      直接その中へ配置する（DrawSearchBar が検索アイコンで既に
		///      使っている手法と同じ）ことで、フレームパディングに関係なく
		///      両方とも行の真の垂直中央に来るようにする。
		ImVec2 headerMin = ImGui::GetItemRectMin();
		Float headerHeight = ImGui::GetItemRectSize().y;
		Float iconSize = ImGui::GetTextLineHeight();
		Float contentX = headerMin.x + ImGui::GetTreeNodeToLabelSpacing();
		Float iconY = headerMin.y + (headerHeight - iconSize) * 0.5f;
		ImGui::GetWindowDrawList()->AddImage(GetComponentIcon(componentName), ImVec2(contentX, iconY), ImVec2(contentX + iconSize, iconY + iconSize));

		Float textY = headerMin.y + (headerHeight - ImGui::GetTextLineHeight()) * 0.5f;
		ImGui::GetWindowDrawList()->AddText(ImVec2(contentX + iconSize + ImGui::GetStyle().ItemInnerSpacing.x, textY), ImGui::GetColorU32(ImGuiCol_Text), componentName.c_str());
		ImGui::PopID();

		if (removed)
		{
			return true;
		}

		if (isHeaderOpen)
		{
			DrawReflectedFields(componentName, componentData, componentID, actor.GetEntity());
		}

		return false;
	}

	void InspectorPanel::DrawComponents(Actor actor)
	{
		Entity entity = actor.GetEntity();

		if (const Mesh* mesh = actor.GetComponent<Mesh>(); mesh && mesh->meshID_ != 0 && !actor.GetComponent<Material>())
		{
			Material* materialComponent = actor.AddComponent<Material>();
			ModelResource* modelResource = context_.worldContext_.resource_->GetModelResource();
			Crister* crister = modelResource->Resolve(*context_.worldContext_.loader_, modelResource->GetHandle(mesh->meshID_));
			Asset* modelAsset = context_.worldContext_.resource_->GetAsset(mesh->meshID_);
			if (materialComponent && crister && modelAsset)
			{
				std::filesystem::path modelPath(modelAsset->fullpath_.c_str());
				std::filesystem::path directory = modelPath.parent_path() / (modelPath.stem().string() + ".Materials");
				const DynamicArray<Surface>& surfaces = crister->Surfaces();
				materialComponent->materialIDs_.resize(surfaces.size(), 0);
				for (Size slot = 0; slot < surfaces.size(); slot++)
				{
					std::string target = (directory / (surfaces[slot].name_ + ".material")).string();
					std::ranges::replace(target, '\\', '/');
					for (const auto& [assetId, asset] : context_.worldContext_.resource_->AssetList())
					{
						if (asset.type_ == AssetType::Material && asset.fullpath_.str() == target)
						{
							materialComponent->materialIDs_[slot] = assetId;
							break;
						}
					}
				}
			}
		}

		const DynamicArray<ComponentID>& layout = context_.worldContext_.world_->GetLayout(entity);

		static const String nameString("Name");
		static const String positionString("Position");
		static const String rotationString("Rotation");
		static const String scaleString("Scale");
		static const String velocityString("Velocity");
		static const String activeString("Active");
		static const String boundsString("Bounds");

		ComponentID positionID = ComponentRegistry::GetComponentID(positionString);
		ComponentID rotationID = ComponentRegistry::GetComponentID(rotationString);
		ComponentID scaleID = ComponentRegistry::GetComponentID(scaleString);

		Bool hasTransform = positionID && rotationID && scaleID && actor.HasComponent(positionID) && actor.HasComponent(rotationID) && actor.HasComponent(scaleID);

		if (hasTransform)
		{
			/// [EN] Same TreeNodeEx + AllowOverlap overlay technique as
			///      DrawComponentEntry - see its comment for why a plain
			///      Image()+SameLine() before CollapsingHeader doesn't work.
			/// [JP] DrawComponentEntry と同じ TreeNodeEx + AllowOverlap の
			///      重ね描画手法 — CollapsingHeader の前に単純な
			///      Image()+SameLine() を置いても効かない理由はそちらの
			///      コメント参照。
			Bool transformHeaderOpen = ImGui::TreeNodeEx("##transformHeader", ImGuiTreeNodeFlags_CollapsingHeader | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_AllowOverlap | ImGuiTreeNodeFlags_DefaultOpen);

			/// [EN] See DrawComponentEntry's comment: Image() top-aligns to
			///      the cursor while the header row is taller (frame
			///      padding), so icon+text are placed by hand into the
			///      header's own measured rect instead of via SameLine().
			/// [JP] DrawComponentEntry のコメント参照: Image() はカーソル
			///      位置にトップ揃えされる一方ヘッダー行はそれより背が
			///      高い（フレームパディング）ため、アイコン/テキストは
			///      SameLine() ではなくヘッダー自身の実測矩形へ手動で配置する。
			ImVec2 transformHeaderMin = ImGui::GetItemRectMin();
			Float transformHeaderHeight = ImGui::GetItemRectSize().y;
			Float transformIconSize = ImGui::GetTextLineHeight();
			Float transformContentX = transformHeaderMin.x + ImGui::GetTreeNodeToLabelSpacing();
			Float transformIconY = transformHeaderMin.y + (transformHeaderHeight - transformIconSize) * 0.5f;
			ImGui::GetWindowDrawList()->AddImage(imguiTexture_.Icon(IconType::ComponentTransform), ImVec2(transformContentX, transformIconY), ImVec2(transformContentX + transformIconSize, transformIconY + transformIconSize));

			Float transformTextY = transformHeaderMin.y + (transformHeaderHeight - ImGui::GetTextLineHeight()) * 0.5f;
			ImGui::GetWindowDrawList()->AddText(ImVec2(transformContentX + transformIconSize + ImGui::GetStyle().ItemInnerSpacing.x, transformTextY), ImGui::GetColorU32(ImGuiCol_Text), "Transform");

			if (transformHeaderOpen)
			{
				Float* positionData = static_cast<Float*>(context_.worldContext_.world_->GetComponent(entity, positionID));
				Float* rotationData = static_cast<Float*>(context_.worldContext_.world_->GetComponent(entity, rotationID));
				Float* scaleData = static_cast<Float*>(context_.worldContext_.world_->GetComponent(entity, scaleID));

				if (positionData)
				{
					DrawTransform(positionData, "位置", positionLinked_, previousPosition_, entity, positionID);
				}
				if (rotationData)
				{
					DrawTransform(rotationData, "回転", rotationLinked_, previousRotation_, entity, rotationID);
				}
				if (scaleData)
				{
					DrawTransform(scaleData, "拡大縮小", scaleLinked_, previousScale_, entity, scaleID);
				}
			}
		}

		for (const ComponentID& componentID : layout)
		{
			String componentName = ComponentRegistry::GetName(componentID);

			if (componentName == nameString || componentName == positionString || componentName == rotationString || componentName == scaleString || componentName == velocityString || componentName == activeString || componentName == boundsString)
			{
				continue;
			}

			void* componentData = context_.worldContext_.world_->GetComponent(entity, componentID);
			if (!componentData)
			{
				continue;
			}

			if (DrawComponentEntry(actor, componentID, componentName, componentData))
			{
				break;
			}
		}

		/// [EN] World::GetLayout only returns the entity's ARCHETYPE component
		///      list (World.cpp: "it->second.archetype_->Layout()") - components
		///      registered with ComponentStorage::SparseSet live outside the
		///      archetype by design (that is the whole point of sparse-set
		///      storage: attaching/detaching one never migrates the entity to a
		///      different archetype), so the loop above never sees them. Walk
		///      every registered component type instead and ask the entity
		///      directly whether it has each sparse one.
		/// [JP] World::GetLayout はエンティティの【アーキタイプ】のコンポーネント
		///      一覧しか返さない(World.cpp: "it->second.archetype_->Layout()")。
		///      ComponentStorage::SparseSet で登録されたコンポーネントは設計上
		///      アーキタイプの外に置かれる(付け外ししてもエンティティが別
		///      アーキタイプへ移行しない、というのがスパースセットの存在理由)
		///      ので、上のループには一切現れない。代わりに登録済みの全
		///      コンポーネント型を走査し、スパースなものだけエンティティに
		///      直接尋ねる。
		for (const auto& [componentID, metadata] : ComponentRegistry::GetRegistry())
		{
			if (metadata.storage_ != ComponentStorage::SparseSet || metadata.isComponentBase_)
			{
				continue;
			}

			void* componentData = context_.worldContext_.world_->GetComponent(entity, componentID);
			if (!componentData)
			{
				continue;
			}

			String componentName = ComponentRegistry::GetName(componentID);

			if (DrawComponentEntry(actor, componentID, componentName, componentData))
			{
				break;
			}
		}

		for (ComponentID componentBaseID : actor.ComponentBaseIDList())
		{
			String componentName = ComponentRegistry::GetName(componentBaseID);
			EntityID entityID = entity.GetID();
			void* componentData = context_.worldContext_.world_->GetComponent(entityID, componentBaseID);
			if (!componentData)
			{
				continue;
			}

			/// [EN] Same TreeNodeEx + AllowOverlap overlay technique as
			///      DrawComponentEntry - see its comment for why a plain
			///      Image()+SameLine() before CollapsingHeader doesn't work.
			/// [JP] DrawComponentEntry と同じ TreeNodeEx + AllowOverlap の
			///      重ね描画手法 — CollapsingHeader の前に単純な
			///      Image()+SameLine() を置いても効かない理由はそちらの
			///      コメント参照。
			ImGui::PushID(componentData);
			Bool isHeaderOpen = ImGui::TreeNodeEx("##header", ImGuiTreeNodeFlags_CollapsingHeader | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_AllowOverlap | ImGuiTreeNodeFlags_DefaultOpen);

			Bool removed = false;
			if (ImGui::BeginPopupContextItem())
			{
				if (ImGui::MenuItem("コンポーネントを削除"))
				{
					context_.sceneContext_.history_.Push(MakePtr<ComponentRemoveCommand>(*context_.worldContext_.world_, actor.GetPersistentID(), componentBaseID, componentName, componentData));
					actor.RemoveComponent(componentBaseID);
					removed = true;
				}
				ImGui::EndPopup();
			}

			/// [EN] See DrawComponentEntry's comment: Image() top-aligns to
			///      the cursor while the header row is taller (frame
			///      padding), so icon+text are placed by hand into the
			///      header's own measured rect instead of via SameLine().
			/// [JP] DrawComponentEntry のコメント参照: Image() はカーソル
			///      位置にトップ揃えされる一方ヘッダー行はそれより背が
			///      高い（フレームパディング）ため、アイコン/テキストは
			///      SameLine() ではなくヘッダー自身の実測矩形へ手動で配置する。
			ImVec2 componentBaseHeaderMin = ImGui::GetItemRectMin();
			Float componentBaseHeaderHeight = ImGui::GetItemRectSize().y;
			Float componentBaseIconSize = ImGui::GetTextLineHeight();
			Float componentBaseContentX = componentBaseHeaderMin.x + ImGui::GetTreeNodeToLabelSpacing();
			Float componentBaseIconY = componentBaseHeaderMin.y + (componentBaseHeaderHeight - componentBaseIconSize) * 0.5f;
			ImGui::GetWindowDrawList()->AddImage(GetComponentIcon(componentName), ImVec2(componentBaseContentX, componentBaseIconY), ImVec2(componentBaseContentX + componentBaseIconSize, componentBaseIconY + componentBaseIconSize));

			Float componentBaseTextY = componentBaseHeaderMin.y + (componentBaseHeaderHeight - ImGui::GetTextLineHeight()) * 0.5f;
			ImGui::GetWindowDrawList()->AddText(ImVec2(componentBaseContentX + componentBaseIconSize + ImGui::GetStyle().ItemInnerSpacing.x, componentBaseTextY), ImGui::GetColorU32(ImGuiCol_Text), componentName.c_str());
			ImGui::PopID();

			if (removed)
			{
				break;
			}

			if (isHeaderOpen)
			{
				ImGui::PushID(componentData);

				DynamicArray<FieldInfo> fields;

				auto& reflectionRegistry = ReflectionRegistry::GetRegistry();
				auto reflectionIt = reflectionRegistry.find(componentName);
				if (reflectionIt != reflectionRegistry.end())
				{
					reflectionIt->second(componentData, fields);
				}

				auto& payloadRegistry = PayloadRegistry::GetRegistry();
				auto payloadIt = payloadRegistry.find(componentName);
				if (payloadIt != payloadRegistry.end())
				{
					payloadIt->second(componentData, fields);
				}

				std::ranges::stable_sort(fields, [](const FieldInfo& a, const FieldInfo& b) { return a.offset_ < b.offset_; });
				DrawFieldList(fields, componentData, entity, componentBaseID, 0);

				static_cast<ComponentBase*>(componentData)->DispatchInspectorGUI();

				ImGui::PopID();
			}
		}
	}

	void InspectorPanel::DrawReflectedFields(String componentName, void* componentData, ComponentID componentID, Entity entity)
	{
		if (!componentData)
		{
			return;
		}

		ImGui::PushID(componentID);

		DynamicArray<FieldInfo> fields;

		auto& reflectionRegistry = ReflectionRegistry::GetRegistry();
		auto reflectionIt = reflectionRegistry.find(componentName);
		if (reflectionIt != reflectionRegistry.end())
		{
			reflectionIt->second(componentData, fields);
		}

		auto& payloadRegistry = PayloadRegistry::GetRegistry();
		auto payloadIt = payloadRegistry.find(componentName);
		if (payloadIt != payloadRegistry.end())
		{
			payloadIt->second(componentData, fields);
		}

		std::ranges::stable_sort(fields, [](const FieldInfo& a, const FieldInfo& b) { return a.offset_ < b.offset_; });
		DrawFieldList(fields, componentData, entity, componentID, 0);

		ImGui::PopID();
	}

	void InspectorPanel::DrawTransform(Float* data, const Char* label, Bool& linked, Float* previousValues, Entity entity, ComponentID componentID)
	{
		std::string checkboxID = std::string("##Link_") + label;

		ImGui::Checkbox(checkboxID.c_str(), &linked);
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("X, Y, Z を連動");
		}

		ImGui::SameLine();
		ImGui::SetNextItemWidth(300.0f);
		ImGui::DragFloat3(label, data, 0.1f);

		if (ImGui::IsItemActivated())
		{
			pendingOldVector3_ = Vector3(data[0], data[1], data[2]);
		}

		if (linked)
		{
			for (Int axis = 0; axis < 3; ++axis)
			{
				if (data[axis] != previousValues[axis])
				{
					Float value = data[axis];
					data[0] = value;
					data[1] = value;
					data[2] = value;
					break;
				}
			}
		}

		if (ImGui::IsItemDeactivatedAfterEdit())
		{
			Vector3 newValue(data[0], data[1], data[2]);
			if (newValue != pendingOldVector3_)
			{
				context_.sceneContext_.history_.Push(MakePtr<ComponentCommand<Vector3>>(*context_.worldContext_.world_, entity, componentID, 0, pendingOldVector3_, newValue));
			}
		}

		previousValues[0] = data[0];
		previousValues[1] = data[1];
		previousValues[2] = data[2];
	}

	void InspectorPanel::DrawFieldList(DynamicArray<FieldInfo>& fields, void* baseData, Entity entity, ComponentID componentID, Size baseOffset)
	{
		for (Size index = 0; index < fields.size(); ++index)
		{
			auto& field = fields[index];

			if (!field.editorVisible_)
			{
				Size skipCount = field.array_.size_;
				index += skipCount;
				continue;
			}

			if (field.type_ == AttributeType::Struct)
			{
				Size skipCount = field.array_.size_;

				/// [EN] A single embedded struct (not an array of structs):
				///      recurse and draw its own fields as a collapsible child
				///      group. Arrays of nested structs (skipCount > 0, e.g. an
				///      AnimationCondition list) keep the original skip-only
				///      behavior - each element is a separate flat FieldInfo
				///      entry with its own directPtr_, and rendering those as
				///      an editable list needs its own add/remove-element UI
				///      that does not exist yet.
				if (skipCount == 0 && !field.array_.add_)
				{
					void* nestedData = field.directPtr_ ? field.directPtr_ : (static_cast<Uint8*>(baseData) + field.offset_);

					auto& reflectionRegistry = ReflectionRegistry::GetRegistry();
					auto reflectionIt = reflectionRegistry.find(field.nestedTypeName_);
					if (reflectionIt != reflectionRegistry.end())
					{
						/// [EN] Honour SC_REFLECTION_FIELD_CONDITION here too.
						///      This branch returns via continue before ever
						///      reaching the scalar path's enableIf_ check
						///      below, so without this a condition on a nested
						///      struct field silently did nothing - the group
						///      stayed fully editable no matter what it was
						///      conditioned on. Disabling the whole subtree
						///      rather than hiding it matches how scalar
						///      fields behave when their condition is false.
						/// [JP] SC_REFLECTION_FIELD_CONDITION をここでも見る。
						///      この分岐は下のスカラー側の enableIf_ 判定へ
						///      到達する前に continue で抜けるため、これが
						///      無いとネストされた構造体フィールドに付けた
						///      条件が黙って無視され、何を条件にしていても
						///      グループが編集可能なままだった。非表示に
						///      せず部分木ごと無効化するのは、条件が偽の
						///      ときのスカラーフィールドの挙動に合わせるため。
						Bool nestedEnabled = !field.enableIf_ || field.enableIf_(baseData);
						ImGui::BeginDisabled(!nestedEnabled);

						ImGui::PushID(field.name_.c_str());

						if (ImGui::TreeNodeEx(field.name_.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
						{
							DynamicArray<FieldInfo> nestedFields;
							reflectionIt->second(nestedData, nestedFields);
							std::ranges::stable_sort(nestedFields, [](const FieldInfo& a, const FieldInfo& b) { return a.offset_ < b.offset_; });

							Size nestedBaseOffset = baseOffset + field.offset_;

							ImGui::Indent();
							DrawFieldList(nestedFields, nestedData, entity, componentID, nestedBaseOffset);
							ImGui::Unindent();

							ImGui::TreePop();
						}

						ImGui::PopID();

						ImGui::EndDisabled();
					}

					continue;
				}

				index += skipCount;
				continue;
			}

			if (field.array_.size_ > 0 || field.array_.add_)
			{
				Size count = field.array_.size_;
				Bool isPayloadArray = field.assetType_ != PayloadAssetType::None;

				ImGui::PushID(field.name_.c_str());

				if (isPayloadArray)
				{
					/// [EN] Flat layout, no collapse arrow: label -> fixed append
					///      drop zone -> "<name>一覧:" -> list of what's already
					///      assigned.
					/// [JP] 折りたたみ無しのフラットなレイアウト: ラベル→固定の
					///      追加用ドロップ枠→「<name>一覧:」→登録済み一覧。
					ImGui::TextUnformatted(field.name_.c_str());

					DynamicArray<Int> existingValues;
					existingValues.reserve(count);
					for (Size elementIndex = 0; elementIndex < count && (index + 1 + elementIndex) < fields.size(); ++elementIndex)
					{
						auto& element = fields[index + 1 + elementIndex];
						void* ptr = element.directPtr_ ? element.directPtr_ : (static_cast<Uint8*>(baseData) + element.offset_);
						existingValues.push_back(*static_cast<Int*>(ptr));
					}

					DrawPayloadArrayAppendSlot(field, existingValues, entity, componentID);

					ImGui::Spacing();
					ImGui::Text("%s一覧:", field.name_.c_str());

					Float listHeight = ImGui::GetTextLineHeightWithSpacing() * 4.0f;

					if (ImGui::BeginListBox("##list", ImVec2(-FLT_MIN, listHeight)))
					{
						Size removeIndex = SIZE_MAX;

						for (Size elementIndex = 0; elementIndex < count && (index + 1 + elementIndex) < fields.size(); ++elementIndex)
						{
							auto& element = fields[index + 1 + elementIndex];
							void* ptr = element.directPtr_ ? element.directPtr_ : (static_cast<Uint8*>(baseData) + element.offset_);
							ImGui::PushID(static_cast<Int>(elementIndex));
							DrawPayloadArrayRow(element, ptr);
							if (field.array_.remove_ && ImGui::BeginPopupContextItem("##payloadRowContext"))
							{
								if (ImGui::MenuItem("削除"))
								{
									removeIndex = elementIndex;
								}
								ImGui::EndPopup();
							}
							ImGui::PopID();
						}

						ImGui::EndListBox();

						if (removeIndex != SIZE_MAX && field.array_.remove_)
						{
							auto& removedElement = fields[index + 1 + removeIndex];
							void* removedPtr = removedElement.directPtr_ ? removedElement.directPtr_ : (static_cast<Uint8*>(baseData) + removedElement.offset_);
							Int removedValue = *static_cast<Int*>(removedPtr);
							context_.sceneContext_.history_.Push(MakePtr<PayloadArrayCommand>(*context_.worldContext_.world_, entity, componentID, field.name_, removeIndex, removedValue, false));
							field.array_.remove_(removeIndex);
						}
					}

					ImGui::Spacing();
				}
				else
				{
					Bool opened = ImGui::TreeNodeEx("##arr", ImGuiTreeNodeFlags_DefaultOpen);
					ImGui::SameLine();
					ImGui::Text("%s [%zu]", field.name_.c_str(), count);

					if (field.array_.add_)
					{
						ImGui::SameLine();
						if (ImGui::SmallButton("+"))
						{
							context_.sceneContext_.history_.Push(MakePtr<ArrayAppendCommand>(*context_.worldContext_.world_, entity, componentID, field.name_, count));
							field.array_.add_();
						}
					}

					if (opened)
					{
						Size removeIndex = SIZE_MAX;
						for (Size elementIndex = 0; elementIndex < count && (index + 1 + elementIndex) < fields.size(); ++elementIndex)
						{
							auto& element = fields[index + 1 + elementIndex];
							void* ptr = element.directPtr_ ? element.directPtr_ : (static_cast<Uint8*>(baseData) + element.offset_);

							if (field.array_.remove_)
							{
								ImGui::PushID(static_cast<Int>(elementIndex));
								if (ImGui::SmallButton("-"))
								{
									removeIndex = elementIndex;
								}
								ImGui::SameLine();
								ImGui::PopID();
							}

							Size elementOffset = baseOffset + element.offset_;

							if (element.assetType_ != PayloadAssetType::None)
							{
								DrawPayloadField(element, ptr, entity, componentID, elementOffset);
							}
							else
							{
								DrawField(element, ptr, entity, componentID, elementOffset);
							}
						}

						/// [EN] A plain (non-payload) array's "-" removal has no undo
						///      support: unlike payload arrays, ArrayInfo exposes no
						///      lastPtr_ equivalent for these elements, so there is
						///      no way to recover the removed value on Undo (add_'s
						///      counterpart ArrayAppendCommand is safe precisely
						///      because add_/remove_ are exact inverses with nothing
						///      to lose).
						/// [JP] プレーン(Payloadでない)配列の「-」削除にはUndo対応が
						///      無い: Payload配列と異なり、ArrayInfoはこれらの要素
						///      向けのlastPtr_相当を持たないため、Undo時に削除された
						///      値を復元する手段が無い(add_側のArrayAppendCommandが
						///      安全なのは、add_/remove_が失うデータの無い完全な逆
						///      操作だからである)。
						if (removeIndex != SIZE_MAX && field.array_.remove_)
						{
							field.array_.remove_(removeIndex);
						}

						ImGui::TreePop();
					}
				}

				ImGui::PopID();
				index += count;
				continue;
			}

			void* ptr = field.directPtr_ ? field.directPtr_ : (static_cast<Uint8*>(baseData) + field.offset_);

			Bool enabled = !field.enableIf_ || field.enableIf_(baseData);
			ImGui::BeginDisabled(!enabled);

			Size fieldOffset = baseOffset + field.offset_;

			if (field.assetType_ != PayloadAssetType::None)
			{
				DrawPayloadField(field, ptr, entity, componentID, fieldOffset);
			}
			else
			{
				DrawField(field, ptr, entity, componentID, fieldOffset);
			}

			ImGui::EndDisabled();
		}
	}

	void InspectorPanel::DrawField(const FieldInfo& field, void* pointer, Entity entity, ComponentID componentID, Size fieldOffset)
	{
		const Char* label = field.name_.c_str();

		Float cMin = field.clampMin_;
		Float cMax = field.clampMax_;
		Bool hasClamped = (cMin != -FLT_MAX || cMax != FLT_MAX);

		switch (field.type_)
		{
		case AttributeType::Int:
		{
			Int* value = static_cast<Int*>(pointer);
			ImGui::DragInt(label, value, 1.0f, hasClamped ? static_cast<Int>(cMin) : 0, hasClamped ? static_cast<Int>(cMax) : 0);
			if (ImGui::IsItemActivated())
			{
				pendingOldInt_ = *value;
			}
			if (ImGui::IsItemDeactivatedAfterEdit())
			{
				if (field.directPtr_)
				{
					context_.sceneContext_.history_.Push(MakePtr<PointerCommand<Int>>(value, pendingOldInt_, *value));
				}
				else
				{
					context_.sceneContext_.history_.Push(MakePtr<ComponentCommand<Int>>(*context_.worldContext_.world_, entity, componentID, fieldOffset, pendingOldInt_, *value));
				}
			}
			break;
		}
		case AttributeType::Float:
		{
			Float* value = static_cast<Float*>(pointer);
			ImGui::DragFloat(label, value, 0.1f, cMin, cMax);
			if (ImGui::IsItemActivated())
			{
				pendingOldFloat_ = *value;
			}
			if (ImGui::IsItemDeactivatedAfterEdit())
			{
				if (field.directPtr_)
				{
					context_.sceneContext_.history_.Push(MakePtr<PointerCommand<Float>>(value, pendingOldFloat_, *value));
				}
				else
				{
					context_.sceneContext_.history_.Push(MakePtr<ComponentCommand<Float>>(*context_.worldContext_.world_, entity, componentID, fieldOffset, pendingOldFloat_, *value));
				}
			}
			break;
		}
		case AttributeType::Bool:
		{
			Bool* value = static_cast<Bool*>(pointer);
			Bool oldValue = *value;
			if (ImGui::Checkbox(label, value))
			{
				if (field.directPtr_)
				{
					context_.sceneContext_.history_.Push(MakePtr<PointerCommand<Bool>>(value, oldValue, *value));
				}
				else
				{
					context_.sceneContext_.history_.Push(MakePtr<ComponentCommand<Bool>>(*context_.worldContext_.world_, entity, componentID, fieldOffset, oldValue, *value));
				}
			}
			break;
		}
		case AttributeType::Vector2:
		{
			Vector2* value = static_cast<Vector2*>(pointer);
			ImGui::DragFloat2(label, &value->x, 0.1f, cMin, cMax);
			if (ImGui::IsItemActivated())
			{
				pendingOldVector2_ = *value;
			}
			if (ImGui::IsItemDeactivatedAfterEdit())
			{
				if (field.directPtr_)
				{
					context_.sceneContext_.history_.Push(MakePtr<PointerCommand<Vector2>>(value, pendingOldVector2_, *value));
				}
				else
				{
					context_.sceneContext_.history_.Push(MakePtr<ComponentCommand<Vector2>>(*context_.worldContext_.world_, entity, componentID, fieldOffset, pendingOldVector2_, *value));
				}
			}
			break;
		}
		case AttributeType::Vector3:
		{
			Vector3* value = static_cast<Vector3*>(pointer);
			ImGui::DragFloat3(label, &value->x, 0.1f, cMin, cMax);
			if (ImGui::IsItemActivated())
			{
				pendingOldVector3_ = *value;
			}
			if (ImGui::IsItemDeactivatedAfterEdit())
			{
				if (field.directPtr_)
				{
					context_.sceneContext_.history_.Push(MakePtr<PointerCommand<Vector3>>(value, pendingOldVector3_, *value));
				}
				else
				{
					context_.sceneContext_.history_.Push(MakePtr<ComponentCommand<Vector3>>(*context_.worldContext_.world_, entity, componentID, fieldOffset, pendingOldVector3_, *value));
				}
			}
			break;
		}
		case AttributeType::String:
		{
			String* stringValue = static_cast<String*>(pointer);
			std::string textBuffer = stringValue->str();
			textBuffer.resize(1024);
			if (ImGui::InputTextMultiline(label, textBuffer.data(), textBuffer.capacity(), ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * 4)))
			{
				*stringValue = String(std::string_view(textBuffer.c_str()));
			}
			if (ImGui::IsItemActivated())
			{
				pendingOldString_ = *stringValue;
			}
			if (ImGui::IsItemDeactivatedAfterEdit())
			{
				if (field.directPtr_)
				{
					context_.sceneContext_.history_.Push(MakePtr<PointerCommand<String>>(stringValue, pendingOldString_, *stringValue));
				}
				else
				{
					context_.sceneContext_.history_.Push(MakePtr<ComponentCommand<String>>(*context_.worldContext_.world_, entity, componentID, fieldOffset, pendingOldString_, *stringValue));
				}
			}
			break;
		}
		case AttributeType::Color:
		{
			Color* value = static_cast<Color*>(pointer);
			ImGui::ColorEdit4(label, &value->x);
			if (ImGui::IsItemActivated())
			{
				pendingOldColor_ = *value;
			}
			if (ImGui::IsItemDeactivatedAfterEdit())
			{
				if (field.directPtr_)
				{
					context_.sceneContext_.history_.Push(MakePtr<PointerCommand<Color>>(value, pendingOldColor_, *value));
				}
				else
				{
					context_.sceneContext_.history_.Push(MakePtr<ComponentCommand<Color>>(*context_.worldContext_.world_, entity, componentID, fieldOffset, pendingOldColor_, *value));
				}
			}
			break;
		}
		case AttributeType::Enum:
		{
			Int* current = static_cast<Int*>(pointer);
			const auto* entries = EnumRegistry::GetEntries(field.enum_.typeName_);
			if (entries)
			{
				const Char* preview = "Unknown";
				for (const EnumEntry& entry : *entries)
				{
					if (entry.value_ == *current)
					{
						preview = entry.name_.c_str();
						break;
					}
				}
				if (ImGui::BeginCombo(label, preview))
				{
					for (const EnumEntry& entry : *entries)
					{
						Bool selected = (entry.value_ == *current);
						if (ImGui::Selectable(entry.name_.c_str(), selected))
						{
							Int oldValue = *current;
							*current = entry.value_;
							if (oldValue != entry.value_)
							{
								if (field.directPtr_)
								{
									context_.sceneContext_.history_.Push(MakePtr<PointerCommand<Int>>(current, oldValue, entry.value_));
								}
								else
								{
									context_.sceneContext_.history_.Push(MakePtr<ComponentCommand<Int>>(*context_.worldContext_.world_, entity, componentID, fieldOffset, oldValue, entry.value_));
								}
							}
						}
						if (selected)
						{
							ImGui::SetItemDefaultFocus();
						}
					}
					ImGui::EndCombo();
				}
			}
			else
			{
				ImGui::TextDisabled("%s (enum未登録)", label);
			}
			break;
		}
		default:
			ImGui::TextDisabled("%s (リフレクション未対応の型です)", label);
			break;
		}
	}

	const Char* InspectorPanel::GetPayloadDropType(PayloadAssetType assetType)const
	{
		switch (assetType)
		{
		case PayloadAssetType::Texture:
			return "ASSET_TEXTURE";
		case PayloadAssetType::Model:
			return "ASSET_MODEL";
		case PayloadAssetType::Effect:
			return "ASSET_EFFECT";
		case PayloadAssetType::Audio:
			return "ASSET_AUDIO";
		case PayloadAssetType::Font:
			return "ASSET_FONT";
		case PayloadAssetType::Movie:
			return "ASSET_MOVIE";
		case PayloadAssetType::Animation:
			return "ASSET_ANIMATION";
		case PayloadAssetType::MeshCollision:
			return "ASSET_MESHCOLLISION";
		case PayloadAssetType::Material:
			return "ASSET_MATERIAL";
		case PayloadAssetType::Sky:
			return "ASSET_SKY";
		case PayloadAssetType::Prefab:
			return "ASSET_PREFAB";
		case PayloadAssetType::Actor:
			return "HIERARCHY_ACTOR";
		default:
			return nullptr;
		}
	}

	/// [EN] Maps a component's registered name to its Inspector header icon
	///      (Unity-style). Falls back to IconType::ComponentCustom for
	///      anything not explicitly listed — covers UserProject scripts and
	///      any built-in component without a dedicated icon.
	/// [JP] コンポーネントの登録名を Inspector ヘッダー用アイコン（Unity 風）
	///      へ対応付ける。明示的に列挙されていないものは
	///      IconType::ComponentCustom にフォールバックする — UserProject の
	///      スクリプトや、専用アイコンを持たない組み込みコンポーネントを
	///      カバーする。
	ImTextureID InspectorPanel::GetComponentIcon(const String& componentName)const
	{
		return imguiTexture_.Icon(ImGuiTexture::ComponentIconType(componentName));
	}

	void InspectorPanel::DrawPayloadField(const FieldInfo& field, void* pointer, Entity entity, ComponentID componentID, Size fieldOffset)
	{
		Int* value = static_cast<Int*>(pointer);

		const Char* dropType = GetPayloadDropType(field.assetType_);

		/// [EN] PayloadAssetType::Actor references a live Actor by persistent ID via World::FindActor, not a ResourceCache asset -- resolve/accept it separately from every other payload type.
		/// [JP] PayloadAssetType::Actor は ResourceCache のアセットではなく、World::FindActor 経由で永続IDから生きた Actor を参照する -- 他の全ペイロード型とは別に解決/受け付けを行う。
		if (field.assetType_ == PayloadAssetType::Actor)
		{
			Uint32 targetId = static_cast<Uint32>(*value);
			Actor target = (targetId != 0) ? context_.worldContext_.world_->FindActor(targetId) : Actor();

			std::string buttonLabel = "ここにドロップ";
			if (target)
			{
				Name* nameComponent = static_cast<Name*>(context_.worldContext_.world_->GetComponent(target.GetEntity(), ComponentRegistry::GetComponentID<Name>()));
				buttonLabel = nameComponent ? nameComponent->name_.str() : "(名前なし)";
			}

			ImGui::Text("%s", field.name_.c_str());
			ImGui::Button(buttonLabel.c_str(), ImVec2(-1, 0));

			if (dropType && ImGui::BeginDragDropTarget())
			{
				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(dropType))
				{
					Actor droppedActor = *static_cast<const Actor*>(payload->Data);
					Int oldValue = *value;
					*value = static_cast<Int>(droppedActor.GetPersistentID());
					if (field.directPtr_)
					{
						context_.sceneContext_.history_.Push(MakePtr<PointerCommand<Int>>(value, oldValue, *value));
					}
					else
					{
						context_.sceneContext_.history_.Push(MakePtr<ComponentCommand<Int>>(*context_.worldContext_.world_, entity, componentID, fieldOffset, oldValue, *value));
					}
				}
				ImGui::EndDragDropTarget();
			}

			return;
		}

		Uint32 assetId = static_cast<Uint32>(*value);
		Asset* asset = (assetId != 0) ? context_.worldContext_.resource_->GetAsset(assetId) : nullptr;

		std::string buttonLabel = asset ? std::filesystem::path(asset->path_.c_str()).filename().string() : "ここにドロップ";

		ImGui::Text("%s", field.name_.c_str());
		ImGui::Button(buttonLabel.c_str(), ImVec2(-1, 0));

		if (dropType && ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(dropType))
			{
				Int oldValue = *value;
				*value = *static_cast<const Int*>(payload->Data);
				if (field.directPtr_)
				{
					context_.sceneContext_.history_.Push(MakePtr<PointerCommand<Int>>(value, oldValue, *value));
				}
				else
				{
					context_.sceneContext_.history_.Push(MakePtr<ComponentCommand<Int>>(*context_.worldContext_.world_, entity, componentID, fieldOffset, oldValue, *value));
				}
			}
			ImGui::EndDragDropTarget();
		}
	}

	void InspectorPanel::DrawPayloadArrayRow(const FieldInfo& field, void* pointer)
	{
		Int* value = static_cast<Int*>(pointer);
		Uint32 assetId = static_cast<Uint32>(*value);
		Asset* asset = (assetId != 0) ? context_.worldContext_.resource_->GetAsset(assetId) : nullptr;

		std::string label = asset ? std::filesystem::path(asset->path_.c_str()).filename().string() : "(空)";
		ImGui::Selectable(label.c_str());
	}

	void InspectorPanel::DrawPayloadArrayAppendSlot(const FieldInfo& field, const DynamicArray<Int>& existingValues, Entity entity, ComponentID componentID)
	{
		const Char* dropType = GetPayloadDropType(field.assetType_);
		if (!dropType || !field.array_.add_ || !field.array_.lastPtr_)
		{
			return;
		}

		ImGui::Button("ここにドロップ", ImVec2(-1, 0));

		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(dropType))
			{
				Int droppedValue = *static_cast<const Int*>(payload->Data);
				Bool alreadyExists = std::ranges::contains(existingValues, droppedValue);
				if (!alreadyExists)
				{
					context_.sceneContext_.history_.Push(MakePtr<PayloadArrayCommand>(*context_.worldContext_.world_, entity, componentID, field.name_, existingValues.size(), droppedValue, true));
					field.array_.add_();
					*static_cast<Int*>(field.array_.lastPtr_()) = droppedValue;
				}
			}
			ImGui::EndDragDropTarget();
		}
	}
}
