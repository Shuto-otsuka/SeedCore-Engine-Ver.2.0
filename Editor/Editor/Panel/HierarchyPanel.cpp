#include <Editor/Editor/Panel/HierarchyPanel.h>
#include <Editor/Editor/EditorContext.h>
#include <Editor/Editor/ImGui/ImGuiTexture.h>
#include <FoundationEngine/ECS/World.h>
#include <FoundationEngine/ECS/Actor.h>
#include <FoundationEngine/ECS/Component/Name.h>
#include <FoundationEngine/ECS/ActorCommand.h>
#include <FoundationEngine/ECS/CompoundCommand.h>
#include <FoundationEngine/Resource/ActorSerialization.h>
#include <FoundationEngine/Resource/Prefab.h>
#include <FoundationEngine/Resource/ResourceCache.h>
#include <FoundationEngine/File/FileDialog.h>
#include <GraphicsEngine/Camera/EditorCamera.h>
#include <GraphicsEngine/Camera/CanvasCamera.h>
#include <GraphicsEngine/Graphics.h>
#include <GraphicsEngine/Texture/Image.h>
#include <GraphicsEngine/Font/Text.h>
#include <GraphicsEngine/Movie/Movie.h>
#include <GraphicsEngine/Model/Animation/Animator.h>
#include <GraphicsEngine/D3D12/SwapChain/GraphicsResolution.h>
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
			pendingClickActor_ = Actor();

			const auto& actors = context_.worldContext_.world_->GetActors();
			for (Size index = 0; index < actors.size(); ++index)
			{
				if (!actors[index].GetParent())
				{
					DrawActorNode(actors[index]);
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
						Actor dropped = *static_cast<const Actor*>(preview->Data);
						Actor oldParent = dropped.GetParent();
						Uint32 oldParentId = oldParent ? oldParent.GetPersistentID() : 0;
						Uint32 oldPrevSiblingId = PrevSiblingPersistentId(dropped);
						dropped.SetParent(Actor());
						context_.sceneContext_.history_.Push(MakePtr<ActorReparentCommand>(*context_.worldContext_.world_, dropped.GetPersistentID(), oldParentId, oldPrevSiblingId, 0));
					}
				}

				if (const ImGuiPayload* prefabPayload = ImGui::AcceptDragDropPayload("ASSET_PREFAB"))
				{
					Uint32 assetID = *static_cast<const Uint32*>(prefabPayload->Data);
					Handle<Prefab> handle = context_.worldContext_.resource_->GetPrefabPool().Load(assetID, *context_.worldContext_.resource_);
					Prefab* prefab = context_.worldContext_.resource_->GetPrefabPool().Get(handle);
					if (prefab)
					{
						prefab->Instantiate(*context_.worldContext_.world_, *context_.worldContext_.resource_, Actor(), assetID);
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
				marqueeStartScrollY_ = ImGui::GetScrollY();
			}

			if (marqueeActive_)
			{
				ImVec2 current = ImGui::GetMousePos();

				/// [EN] Compensate marqueeStart_ for any scrolling (auto-scroll or
				///      otherwise) that has happened since the drag began, so the
				///      start edge tracks the row/content position originally
				///      clicked rather than staying pinned to that fixed pixel -
				///      see marqueeStartScrollY_'s header comment.
				/// [JP] ドラッグ開始後に発生したスクロール（オートスクロール含む）を
				///      marqueeStart_ に補正し、開始端が固定ピクセルではなく
				///      最初にクリックした行/コンテンツの位置を追従するようにする —
				///      marqueeStartScrollY_ のヘッダコメント参照。
				ImVec2 adjustedStart(marqueeStart_.x, marqueeStart_.y - (ImGui::GetScrollY() - marqueeStartScrollY_));

				/// [EN] Explorer/Unreal-style auto-scroll: while dragging the marquee
				///      near the window's top/bottom edge, keep scrolling the hierarchy
				///      so rows outside the current viewport can still be reached
				///      (adjustedStart above keeps the start edge correct while this
				///      happens). Margin is 32px and speed is in pixels/second (scaled
				///      by DeltaTime) rather than pixels/frame, so scrolling is
				///      frame-rate independent and fast enough to reach rows many
				///      screens away while the mouse just sits at the edge.
				/// [JP] エクスプローラー/Unreal 風のオートスクロール: マーキーを
				///      ウィンドウ上端/下端付近までドラッグしている間、ヒエラルキーを
				///      スクロールし続け、現在のビューポート外の行にも届くようにする
				///      （その間の開始端の補正は上の adjustedStart が行う）。
				///      マージンは32px、速度はピクセル/フレームではなくピクセル/秒
				///      （DeltaTimeでスケール）。フレームレートに依存せず、マウスを
				///      端に置いたままにするだけで何画面分も離れた行にも届く。
				Float autoScrollMargin = 32.0f;
				Float autoScrollSpeed = 900.0f;
				ImVec2 windowMin = ImGui::GetWindowPos();
				ImVec2 windowMax(windowMin.x + ImGui::GetWindowSize().x, windowMin.y + ImGui::GetWindowSize().y);
				if (current.y < windowMin.y + autoScrollMargin)
				{
					Float depth = (windowMin.y + autoScrollMargin) - current.y;
					ImGui::SetScrollY(ImGui::GetScrollY() - autoScrollSpeed * (depth / autoScrollMargin) * ImGui::GetIO().DeltaTime);
				}
				else if (current.y > windowMax.y - autoScrollMargin)
				{
					Float depth = current.y - (windowMax.y - autoScrollMargin);
					ImGui::SetScrollY(ImGui::GetScrollY() + autoScrollSpeed * (depth / autoScrollMargin) * ImGui::GetIO().DeltaTime);
				}

				ImVec2 rectMin(Min(adjustedStart.x, current.x), Min(adjustedStart.y, current.y));
				ImVec2 rectMax(Max(adjustedStart.x, current.x), Max(adjustedStart.y, current.y));

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

					context_.selectionContext_.selectedActor_ = context_.selectionContext_.selectedActors_.empty() ? Actor() : context_.selectionContext_.selectedActors_.back();
					context_.selectionContext_.selectedEntity_ = context_.selectionContext_.selectedActor_ ? context_.selectionContext_.selectedActor_.GetEntity() : Entity::Null();
					rangeAnchor_ = context_.selectionContext_.selectedActor_;

					marqueeActive_ = false;
				}
			}

			if (pendingClickActor_)
			{
				HandleNodeSelection(pendingClickActor_, pendingClickCtrl_, pendingClickShift_);
				pendingClickActor_ = Actor();
			}

			if (ImGui::BeginPopupContextItem("HierarchyContext"))
			{
				if (ImGui::MenuItem("空のActorを作成"))
				{
					String uniqueName = GetUniqueName();
					Actor actor = context_.worldContext_.world_->CreateActor(uniqueName);
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
				Actor actor = context_.worldContext_.world_->CreateActor(uniqueName);
				DynamicArray<SerializedActorNode> nodes;
				CaptureActorNode(actor, -1, nodes);
				context_.sceneContext_.history_.Push(MakePtr<ActorCreateCommand>(*context_.worldContext_.world_, *context_.worldContext_.resource_, nodes));
			}

			ImGui::Dummy(ImVec2(0.0f, 8.0f));
		}
		ImGui::End();
	}

	void HierarchyPanel::DrawActorNode(Actor actor)
	{
		const Char* label = "Actor";
		const Name* name = actor.GetComponent<Name>();
		if (name && !name->name_.str().empty())
		{
			label = name->name_.c_str();
		}

		Bool selected = IsSelected(actor);
		Bool hasChildren = !actor.GetChildren().empty();
		Bool isChild = static_cast<Bool>(actor.GetParent());

		ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_AllowOverlap;
		if (selected)
		{
			flags |= ImGuiTreeNodeFlags_Selected;
		}
		if (!hasChildren)
		{
			flags |= ImGuiTreeNodeFlags_Leaf;
		}

		ImGui::PushID(static_cast<Int>(actor.GetEntity().GetID().index_));

		ImTextureID icon;
		if (actor.IsPrefabInstance())
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
		///      slides a viewport camera to it. An Image/Text/Movie whose
		///      view type is Sprite is drawn only in the 2D canvas
		///      (ImageRenderer/FontRenderer/MovieRenderer offset it by
		///      +100000 into canvas space), so framing it with the 3D editor
		///      camera would just fly that off to the canvas origin - those
		///      animate the CanvasView camera and pull the canvas panel
		///      forward instead. The same components in Billboard/Fullscreen
		///      mode are ordinary world-space geometry and take the 3D path
		///      like everything else. The 3D path frames the editor camera on
		///      the actor's world-space Bounds centre (Vector3::Transform of
		///      the local centre by the composed world matrix), not the raw
		///      pivot - a skeletal mesh's pivot is usually at the feet / DCC
		///      origin while the body sits well above it, so framing the
		///      pivot puts the character off screen.
		/// [JP] Unreal 風の「選択対象にフレーム」: アクター行をダブルクリック
		///      するとビューポートのカメラがそこへスライドする。表示形式が
		///      Sprite の Image/Text/Movie は 2D キャンバスにしか描かれない
		///      （ImageRenderer/FontRenderer/MovieRenderer が +100000 で
		///      キャンバス空間へずらす）ので、3D エディタカメラでフレーム
		///      するとキャンバス原点へ飛んでいくだけ - これらは CanvasView の
		///      カメラをアニメーションで寄せ、キャンバスパネルを前面に出す。
		///      同じコンポーネントでも Billboard/Fullscreen のときは通常の
		///      ワールド空間ジオメトリなので、他と同じく 3D 経路をとる。
		///      3D 経路は、生のピボットではなくアクターのワールド空間 Bounds
		///      中心（合成ワールド行列でローカル中心を Vector3::Transform
		///      したもの）へフレームする - スケルタルメッシュのピボットは
		///      通常、足元 / DCC 原点にあり本体はその上方にあるため、
		///      ピボットへフレームするとキャラが画面外になる。
		if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
		{
			const Matrix& worldMatrix = actor.GetWorldMatrix();

			const Image* image = actor.GetComponent<Image>();
			const Text* text = actor.GetComponent<Text>();
			const Movie* movie = actor.GetComponent<Movie>();
			Bool isCanvasActor = (image && image->viewType_ == Image::ViewType::Sprite) || (text && text->viewType_ == Text::ViewType::Sprite) || (movie && movie->displayMode_ == Movie::DisplayMode::Sprite);

			if (isCanvasActor && context_.cameraContext_.canvasCamera_)
			{
				/// [EN] The canvas-space point this actor is drawn at - the same +100000 / Y-flip the renderers apply. CanvasCamera::FocusOn animates the slide (see its comment).
				/// [JP] このアクターが描かれるキャンバス空間の点 - レンダラーが掛けるのと同じ +100000 / Y 反転。スライドのアニメーションは CanvasCamera::FocusOn が行う（同コメント参照）。
				Vector3 canvasTarget = Vector3(100000.0f + worldMatrix._41, 100000.0f + (ScResolution::SC_HD.Height - worldMatrix._42), 100000.0f);
				context_.cameraContext_.canvasCamera_->FocusOn(canvasTarget);

				ImGui::SetWindowFocus("キャンバスビュー");
			}
			else if (context_.cameraContext_.editorCamera_)
			{
				/// [EN] An actor with an Animator is skinned: its Bounds is
				///      ModelRenderer's copy of the crister's bind-pose vertex
				///      AABB, which neither tracks the animated pose nor
				///      excludes helper/off-model geometry baked into the bind
				///      pose - so a dolly radius derived from it (and its
				///      centre) can be wildly off and fling the camera away.
				///      Pan to the pivot at the current distance instead. Any
				///      other actor gets the bounds-fit dolly onto its
				///      world-space Bounds centre; every actor has a Bounds
				///      (default 0.5 half-extent, centre 0) so a non-mesh
				///      actor just pans onto its pivot with a small radius.
				/// [JP] Animator を持つアクターはスキン付き: その Bounds は
				///      ModelRenderer が持つ crister のバインドポーズ頂点 AABB
				///      の写しで、アニメ後のポーズを追わず、バインドポーズに
				///      焼き込まれたヘルパー/モデル外ジオメトリも除外しない -
				///      そこから出したドリー radius(と中心)は大きくズレて
				///      カメラを飛ばしうる。代わりに現在の距離のままピボットへ
				///      パンする。それ以外のアクターはワールド空間 Bounds
				///      中心へのバウンズフィットドリー。全アクターが Bounds を
				///      持つ(デフォルトは半径 0.5、中心 0)ので、メッシュでない
				///      アクターは小さな radius でピボットへパンするだけになる。
				Bool skinned = actor.GetComponent<Animator>() != nullptr;

				Float radius = 0.0f;
				Vector3 target = Vector3(worldMatrix._41, worldMatrix._42, worldMatrix._43);

				const Bounds* bounds = actor.GetComponent<Bounds>();
				if (bounds && !skinned)
				{
					Float worldScale = Max(Max(Vector3(worldMatrix._11, worldMatrix._12, worldMatrix._13).Length(), Vector3(worldMatrix._21, worldMatrix._22, worldMatrix._23).Length()), Vector3(worldMatrix._31, worldMatrix._32, worldMatrix._33).Length());
					radius = bounds->extent_.Length() * worldScale;
					target = Vector3::Transform(bounds->center_, worldMatrix);
				}

				context_.cameraContext_.editorCamera_->FocusOn(target, radius);

				/// [EN] Pull the 3D viewport forward too - symmetric with the canvas branch above, so the camera move is actually visible when the double-click came from another dock (e.g. while the canvas view was on top).
				/// [JP] 3D ビューポートも前面に出す - 上のキャンバス分岐と対称。別のドック（例: キャンバスビューが手前のとき）からダブルクリックしてもカメラ移動が実際に見えるように。
				ImGui::SetWindowFocus("エディタービュー");
			}
		}

		rows_.push_back({ actor, ImGui::GetItemRectMin(), ImGui::GetItemRectMax() });

		if (ImGui::BeginDragDropSource())
		{
			ImGui::SetDragDropPayload("HIERARCHY_ACTOR", &actor, sizeof(Actor));
			ImGui::Text("%s", label);
			ImGui::EndDragDropSource();
		}

		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* preview = ImGui::AcceptDragDropPayload("HIERARCHY_ACTOR", ImGuiDragDropFlags_AcceptPeekOnly))
			{
				Actor dropped = *static_cast<const Actor*>(preview->Data);
				Bool canDrop = (dropped != actor && !actor.Descendant(dropped));

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
						Actor oldParent = dropped.GetParent();
						Uint32 oldParentId = oldParent ? oldParent.GetPersistentID() : 0;
						Uint32 oldPrevSiblingId = PrevSiblingPersistentId(dropped);
						dropped.SetParent(actor);
						context_.sceneContext_.history_.Push(MakePtr<ActorReparentCommand>(*context_.worldContext_.world_, dropped.GetPersistentID(), oldParentId, oldPrevSiblingId, actor.GetPersistentID()));
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
		if (!actor.IsActive())
		{
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
		}
		ImGui::Text("%s", label);
		if (!actor.IsActive())
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
		ImTextureID activeIcon = actor.IsActive() ? imguiTexture_.Icon(IconType::ActorActive) : imguiTexture_.Icon(IconType::ActorNonActive);
		ImVec2 activePosition = ImGui::GetCursorScreenPos();
		Float activeYOffset = actor.IsActive() ? 2.0f : 0.0f;
		Float activeY = activePosition.y + (ImGui::GetTextLineHeight() - iconSize) * 0.5f + activeYOffset;
		if (ImGui::InvisibleButton("##Active", ImVec2(iconSize, ImGui::GetTextLineHeight())))
		{
			context_.sceneContext_.history_.Push(MakePtr<ActorActiveCommand>(*context_.worldContext_.world_, actor, !actor.IsActive()));
			actor.SetActive(!actor.IsActive());
		}
		ImGui::GetWindowDrawList()->AddImage(activeIcon, ImVec2(activePosition.x, activeY), ImVec2(activePosition.x + iconSize, activeY + iconSize));

		if (opened)
		{
			for (Actor child : actor.GetChildren())
			{
				DrawActorNode(child);
			}
			ImGui::TreePop();
		}

		ImGui::PopID();
	}

	void HierarchyPanel::SaveAsPrefab(Actor actor)
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

		D3D12Context* d3d12Context = context_.graphicsContext_.graphics_->GetContext();
		context_.worldContext_.resource_->Reload(*context_.worldContext_.loader_, d3d12Context->GetDevice(), d3d12Context->GetDirectQueue(), context_.graphicsContext_.graphics_->GetBC7CompressShader());

		/// [EN] Rebind this Actor's Apply target to the newly-saved file, so a later
		///      "Prefab に適用" writes to this new Prefab instead of any Prefab it
		///      may have originally been instantiated from.
		/// [JP] この Actor の適用先を、新しく保存されたファイルに再バインドする。
		///      これにより以降の「Prefab に適用」は、元々インスタンス化された Prefab
		///      ではなく、この新しい Prefab に書き込まれる。
		std::string relative = std::filesystem::relative(savedPath, context_.worldContext_.resource_->ProjectRootPath()).string();
		std::ranges::replace(relative, '\\', '/');
		Uint32 newAssetID = context_.worldContext_.resource_->GetAssetID(String(relative));
		if (newAssetID != 0)
		{
			actor.SetSourcePrefabAssetID(newAssetID);
		}
	}

	String HierarchyPanel::GetUniqueName()
	{
		String baseName = String("Empty Actor");
		String uniqueName = baseName;

		Int index = 1;

		while (true)
		{
			Bool exists = std::ranges::any_of(context_.worldContext_.world_->GetActors(), [&](const auto& actor)
			{
				const Name* name = actor.GetComponent<Name>();
				return name && name->name_ == uniqueName;
			});

			if (!exists)
			{
				break;
			}

			uniqueName = String(baseName.str() + "(" + std::to_string(index++) + ")");
		}

		return uniqueName;
	}

	Bool HierarchyPanel::IsSelected(Actor actor)const
	{
		return std::ranges::contains(context_.selectionContext_.selectedActors_, actor);
	}

	void HierarchyPanel::HandleNodeSelection(Actor actor, Bool ctrl, Bool shift)
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
			auto it = std::ranges::find(context_.selectionContext_.selectedActors_, actor);
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

		context_.selectionContext_.selectedActor_ = context_.selectionContext_.selectedActors_.empty() ? Actor() : context_.selectionContext_.selectedActors_.back();
		context_.selectionContext_.selectedEntity_ = context_.selectionContext_.selectedActor_ ? context_.selectionContext_.selectedActor_.GetEntity() : Entity::Null();
	}

	void HierarchyPanel::DeleteActor(Actor actor, CompoundCommand* group)
	{
		auto it = std::ranges::find(context_.selectionContext_.selectedActors_, actor);
		if (it != context_.selectionContext_.selectedActors_.end())
		{
			context_.selectionContext_.selectedActors_.erase(it);
		}

		if (context_.selectionContext_.selectedActor_ == actor)
		{
			context_.selectionContext_.selectedActor_ = context_.selectionContext_.selectedActors_.empty() ? Actor() : context_.selectionContext_.selectedActors_.back();
			context_.selectionContext_.selectedEntity_ = context_.selectionContext_.selectedActor_ ? context_.selectionContext_.selectedActor_.GetEntity() : Entity::Null();
		}

		/// [EN] The command must be built (it captures the actor's subtree) before DestroyActor runs. When part of a multi-delete it goes into group instead of the history, so one Ctrl+Z reverts the whole selection.
		/// [JP] コマンドは actor のサブツリーを取得するため DestroyActor より前に構築する。複数削除の一部なら履歴ではなく group へ入れ、Ctrl+Z 一回で選択全体を戻せるようにする。
		ResourcePtr<Command> command = MakePtr<ActorDeleteCommand>(*context_.worldContext_.world_, *context_.worldContext_.resource_, actor);
		if (group)
		{
			group->Add(std::move(command));
		}
		else
		{
			context_.sceneContext_.history_.Push(std::move(command));
		}

		context_.worldContext_.world_->DestroyActor(actor);
	}

	void HierarchyPanel::DeleteSelection()
	{
		DynamicArray<Actor> toDelete = context_.selectionContext_.selectedActors_;

		ResourcePtr<CompoundCommand> group = MakePtr<CompoundCommand>();
		for (Actor actor : toDelete)
		{
			DeleteActor(actor, group.get());
		}

		if (!group->Empty())
		{
			context_.sceneContext_.history_.Push(std::move(group));
		}
	}

	void HierarchyPanel::DuplicateSelection()
	{
		if (context_.selectionContext_.selectedActors_.empty())
		{
			return;
		}

		DynamicArray<Actor> toDuplicate = context_.selectionContext_.selectedActors_;
		DynamicArray<Actor> newSelection;

		ResourcePtr<CompoundCommand> group = MakePtr<CompoundCommand>();

		for (Actor actor : toDuplicate)
		{
			/// [EN] Skip Actors whose ancestor is also selected - they'll already be
			///      duplicated as part of that ancestor's subtree.
			/// [JP] 祖先も選択されている Actor はスキップする — その祖先のサブツリーの
			///      一部として既に複製されるため。
			Bool ancestorSelected = std::ranges::any_of(toDuplicate, [&](Actor other) { return other != actor && actor.Descendant(other); });
			if (ancestorSelected)
			{
				continue;
			}

			Prefab prefab;
			prefab.Capture(actor);

			Actor duplicate = prefab.Instantiate(*context_.worldContext_.world_, *context_.worldContext_.resource_, actor.GetParent(), actor.GetSourcePrefabAssetID());
			if (!duplicate)
			{
				continue;
			}

			DynamicArray<SerializedActorNode> nodes;
			CaptureActorNode(duplicate, -1, nodes);
			Actor duplicateParent = duplicate.GetParent();
			group->Add(MakePtr<ActorCreateCommand>(*context_.worldContext_.world_, *context_.worldContext_.resource_, nodes, duplicateParent ? duplicateParent.GetPersistentID() : 0, actor.GetPersistentID()));

			MoveAfter(duplicate, actor);
			newSelection.push_back(duplicate);
		}

		if (!group->Empty())
		{
			context_.sceneContext_.history_.Push(std::move(group));
		}

		context_.selectionContext_.selectedActors_ = newSelection;
		context_.selectionContext_.selectedActor_ = newSelection.empty() ? Actor() : newSelection.back();
		context_.selectionContext_.selectedEntity_ = context_.selectionContext_.selectedActor_ ? context_.selectionContext_.selectedActor_.GetEntity() : Entity::Null();
		rangeAnchor_ = context_.selectionContext_.selectedActor_;
	}

	void HierarchyPanel::MoveAfter(Actor actor, Actor after)
	{
		Actor parent = actor.GetParent();
		if (parent)
		{
			parent.MoveChild(actor, after);
			return;
		}

		context_.worldContext_.world_->MoveActor(actor, after);
	}

	Uint32 HierarchyPanel::PrevSiblingPersistentId(Actor actor)const
	{
		Actor parent = actor.GetParent();
		if (!parent)
		{
			return 0;
		}

		const DynamicArray<Actor>& siblings = parent.GetChildren();
		auto it = std::ranges::find(siblings, actor);
		if (it == siblings.end() || it == siblings.begin())
		{
			return 0;
		}

		return (*(it - 1)).GetPersistentID();
	}
}
