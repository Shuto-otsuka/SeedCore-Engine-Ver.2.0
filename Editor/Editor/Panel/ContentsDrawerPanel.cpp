#include <Editor/Editor/Panel/ContentsDrawerPanel.h>
#include <Editor/Editor/EditorContext.h>
#include <Editor/Editor/ImGui/ImGuiTexture.h>
#include <Editor/Editor/ImGui/ImGuiRenderer.h>
#include <Editor/Editor/Panel/MaterialViewerPanel.h>
#include <FoundationEngine/Resource/ResourceCache.h>
#include <FoundationEngine/Resource/Prefab/Prefab.h>
#include <FoundationEngine/World/Actor/Actor.h>
#include <GraphicsEngine/Texture/TextureResource.h>
#include <GraphicsEngine/Texture/Texture.h>
#include <GraphicsEngine/Model/ModelResource.h>
#include <GraphicsEngine/Model/ModelLoader.h>
#include <GraphicsEngine/Model/ModelExporter.h>
#include <GraphicsEngine/Model/Crister.h>
#include <FoundationEngine/File/FileDialog.h>
#include <GraphicsEngine/D3D12/Descriptor/BindlessHeap.h>
#include <GraphicsEngine/D3D12/Descriptor/DescriptorHeap.h>
#include <GraphicsEngine/Graphics.h>
#include <FoundationEngine/Log/Warning.h>
#include <FoundationEngine/Log/Notice.h>
#include <Editor/Editor/Build/VisualStudioAutomation.h>

namespace SeedCore
{
	ContentsDrawerPanel::ContentsDrawerPanel(EditorContext& context, ImGuiTexture& imguiTexture) : context_(context), imguiTexture_(imguiTexture)
	{
		searchBuffer_.resize(256, '\0');
		BuildDirectoryTree();

		const std::filesystem::path& projectRoot = context_.worldContext_.resource_->ProjectRootPath();
		std::filesystem::path contentRoot = projectRoot / "UserProject";
		if (!std::filesystem::exists(contentRoot))
		{
			contentRoot = projectRoot;
		}
		directoryWatchHandle_ = FindFirstChangeNotificationW(contentRoot.wstring().c_str(), TRUE, FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE);
		if (directoryWatchHandle_ == INVALID_HANDLE_VALUE || directoryWatchHandle_ == nullptr)
		{
			directoryWatchHandle_ = INVALID_HANDLE_VALUE;
		}
	}

	ContentsDrawerPanel::~ContentsDrawerPanel()
	{
		if (directoryWatchHandle_ != INVALID_HANDLE_VALUE && directoryWatchHandle_ != nullptr)
		{
			FindCloseChangeNotification(directoryWatchHandle_);
			directoryWatchHandle_ = INVALID_HANDLE_VALUE;
		}
	}


	void ContentsDrawerPanel::Draw()
	{
		if (context_.resourceSync_ && sharingRevision_ != context_.resourceSync_->Revision())
		{
			sharingRevision_ = context_.resourceSync_->Revision();
			needsRebuild_ = true;
		}
		ImGuiID dockspaceID = ImGui::GetID("ScDockSpace");
		ImGui::SetNextWindowDockID(dockspaceID, ImGuiCond_FirstUseEver);

		if (directoryWatchHandle_ != INVALID_HANDLE_VALUE && WaitForSingleObject(directoryWatchHandle_, 0) == WAIT_OBJECT_0)
		{
			D3D12Context* d3d12Context = context_.graphicsContext_.graphics_->GetContext();
			context_.worldContext_.resource_->Reload(*context_.worldContext_.loader_, d3d12Context->GetDevice(), d3d12Context->GetDirectQueue(), context_.graphicsContext_.graphics_->GetBC7CompressShader());
			needsRebuild_ = true;
			FindNextChangeNotification(directoryWatchHandle_);
		}

		if (ImGui::Begin("コンテンツドロワー"))
		{
			if (context_.resourceSync_)
			{
				ResourceSyncControlPanel::DrawStatus(context_);
			}
			if (ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows))
			{
				if (ImGui::IsMouseClicked(3) && historyIndex_ > 0)
				{
					--historyIndex_;
					selectedDirectory_ = directoryHistory_[historyIndex_];
				}
				if (ImGui::IsMouseClicked(4) && historyIndex_ < static_cast<Int>(directoryHistory_.size()) - 1)
				{
					++historyIndex_;
					selectedDirectory_ = directoryHistory_[historyIndex_];
				}
			}

			if (needsRebuild_)
			{
				BuildDirectoryTree();
				needsRebuild_ = false;
			}

			if (ImGui::RadioButton("リスト", viewMode_ == ViewMode::List))
			{
				viewMode_ = ViewMode::List;
			}
			ImGui::SameLine();
			if (ImGui::RadioButton("グリッド", viewMode_ == ViewMode::Grid))
			{
				viewMode_ = ViewMode::Grid;
			}

			if (viewMode_ == ViewMode::Grid)
			{
				ImGui::SameLine();
				ImGui::SetNextItemWidth(120.0f);
				ImGui::SliderFloat("##IconSize", &gridIconSize_, 32.0f, 128.0f, "%.0f");
			}

			ImGui::SameLine();
			Float iconSize = ImGui::GetTextLineHeight();
			Float originalPaddingX = ImGui::GetStyle().FramePadding.x;
			Float iconPadding = iconSize + originalPaddingX * 2.0f;
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(iconPadding, ImGui::GetStyle().FramePadding.y));
			ImGui::SetNextItemWidth(-1.0f);
			ImGui::InputTextWithHint("##Search", "検索...", searchBuffer_.data(), searchBuffer_.size());
			ImGui::PopStyleVar();
			ImVec2 inputMin = ImGui::GetItemRectMin();
			Float inputHeight = ImGui::GetItemRectSize().y;
			Float iconY = inputMin.y + (inputHeight - iconSize) * 0.5f;
			ImGui::GetWindowDrawList()->AddImage(imguiTexture_.Icon(IconType::Search), ImVec2(inputMin.x + originalPaddingX, iconY), ImVec2(inputMin.x + originalPaddingX + iconSize, iconY + iconSize));
			ImGui::Separator();

			std::string searchKey(searchBuffer_.c_str());

			if (!searchKey.empty())
			{
				for (AssetRecord& record : browserAssets_)
				{
					AssetRecord* asset = &record;
					if (asset->path_.str().find(searchKey) == std::string::npos)
					{
						continue;
					}
					ImGui::PushID(asset->assetID_);

					ImTextureID icon = GetAssetIcon(*asset);
					ImGui::Image(icon, ImVec2(ImGui::GetTextLineHeight(), ImGui::GetTextLineHeight()));

					/// [EN] The badge sits on the lower-right quarter of the icon, the
					///      corner an asset icon is least likely to fill.
					/// [JP] バッジはアイコンの右下 1/4 に重ねる。アセットのアイコンが
					///      埋めている可能性が最も低い角だから。
					ImTextureID sharingIcon = GetSharingIcon(*asset);
					if (sharingIcon)
					{
						ImVec2 badgeMin = ImGui::GetItemRectMin();
						ImVec2 badgeMax = ImGui::GetItemRectMax();
						badgeMin.x = badgeMin.x + (badgeMax.x - badgeMin.x) * 0.5f;
						badgeMin.y = badgeMin.y + (badgeMax.y - badgeMin.y) * 0.5f;
						ImGui::GetWindowDrawList()->AddImage(sharingIcon, badgeMin, badgeMax);
					}

					ImGui::SameLine();
					ImGui::Selectable(asset->path_.c_str(), false, ImGuiSelectableFlags_SpanAvailWidth | ImGuiSelectableFlags_AllowDoubleClick);

					if ((!context_.resourceSync_ || !context_.resourceSync_->RemoteOnly(asset->assetID_)) && ImGui::BeginDragDropSource())
					{
						const Char* payloadType = GetDragDropType(asset->type_);
						ImGui::SetDragDropPayload(payloadType, &asset->assetID_, sizeof(Uint32));
						ImGui::Text("%s", std::filesystem::path(asset->path_.c_str()).filename().string().c_str());
						ImGui::EndDragDropSource();
					}

					if (ImGui::IsItemHovered())
					{
						DrawAssetTooltip(*asset);
						if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
						{
							OpenAssetExternal(*asset);
						}
					}

					ImGui::PopID();
				}
			}
			else
			{
				Float panelWidth = ImGui::GetContentRegionAvail().x;
				Float treeWidth = panelWidth * 0.3f;
				if (treeWidth < 150.0f)
				{
					treeWidth = 150.0f;
				}

				ImGui::BeginChild("##DirectoryTree", ImVec2(treeWidth, 0), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX);
				DrawDirectoryTree(root_);

				if (ImGui::BeginPopupContextWindow("##TreeBackground", ImGuiPopupFlags_NoOpenOverItems | ImGuiPopupFlags_MouseButtonRight))
				{
					if (ImGui::MenuItem("新規フォルダ"))
					{
						CreateNewFolder(root_.fullPath);
						ImGui::CloseCurrentPopup();
					}

					Bool canPaste = clipboardAction_ != ClipboardAction::None;
					if (ImGui::MenuItem("貼り付け", nullptr, false, canPaste))
					{
						ExecutePaste(selectedDirectory_);
						ImGui::CloseCurrentPopup();
					}

					ImGui::Separator();

					if (ImGui::MenuItem("エクスプローラーで開く"))
					{
						std::filesystem::path fullPath = ResolveFullPath(selectedDirectory_);
						ShellExecuteW(NULL, L"explore", fullPath.wstring().c_str(), NULL, NULL, SW_SHOWNORMAL);
						ImGui::CloseCurrentPopup();
					}

					ImGui::EndPopup();
				}

				ImGui::EndChild();

				ImGui::SameLine();

				ImGui::BeginChild("##AssetList", ImVec2(0, 0), ImGuiChildFlags_Borders);
				DrawAssetList();
				ImGui::EndChild();

				if (ImGui::BeginDragDropTarget())
				{
					if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("HIERARCHY_ACTOR"))
					{
						Actor dropped = *static_cast<const Actor*>(payload->Data);
						std::filesystem::path directory = ResolveFullPath(selectedDirectory_);
						std::filesystem::path savedPath = Prefab::SaveToDirectory(dropped, directory);
						if (!savedPath.empty())
						{
							D3D12Context* d3d12Context = context_.graphicsContext_.graphics_->GetContext();
							context_.worldContext_.resource_->Reload(*context_.worldContext_.loader_, d3d12Context->GetDevice(), d3d12Context->GetDirectQueue(), context_.graphicsContext_.graphics_->GetBC7CompressShader());
							needsRebuild_ = true;

							std::string relative = std::filesystem::relative(savedPath, context_.worldContext_.resource_->ProjectRootPath()).string();
							std::ranges::replace(relative, '\\', '/');
							Uint32 newAssetID = context_.worldContext_.resource_->GetAssetID(String(relative));
							if (newAssetID != 0)
							{
								dropped.PrefabID(newAssetID);
							}
						}
					}
					ImGui::EndDragDropTarget();
				}
			}
		}

		ImGui::End();
	}

	void ContentsDrawerPanel::BuildDirectoryTree()
	{
		root_ = {};
		root_.name = "Project";
		root_.fullPath = "";

		static const std::set<std::string> excludeDirectories =
		{
			"AIEngine", "AudioEngine", "CompiledShaderObject", "Editor",
			"External", "FoundationEngine", "GraphicsEngine", "Launcher", "Logs",
			"Package", "PhysicsEngine", "Runtime", "SeedCore", "Tools",
			".vs", "x64", ".git", ".asset",
		};

		const std::filesystem::path& projectRoot = context_.worldContext_.resource_->ProjectRootPath();
		std::error_code errorCode;
		for (auto it = std::filesystem::recursive_directory_iterator(projectRoot, errorCode); it != std::filesystem::recursive_directory_iterator(); ++it)
		{
			if (!it->is_directory())
			{
				continue;
			}

			std::string directoryName = it->path().filename().string();
			if (excludeDirectories.contains(directoryName))
			{
				it.disable_recursion_pending();
				continue;
			}

			std::string relativePath = std::filesystem::relative(it->path(), projectRoot, errorCode).string();
			std::ranges::replace(relativePath, '\\', '/');

			DirectoryNode* current = &root_;
			std::istringstream stream(relativePath);
			std::string segment;
			std::string builtPath;

			while (std::getline(stream, segment, '/'))
			{
				if (!builtPath.empty())
				{
					builtPath += "/";
				}
				builtPath += segment;

				if (!current->children.contains(segment))
				{
					DirectoryNode node;
					node.name = segment;
					node.fullPath = builtPath;
					current->children.insert(segment, std::move(node));
				}
				current = &current->children.at(segment);
			}
		}

		const auto& allAssets = context_.worldContext_.resource_->AssetList();
		browserAssets_.clear();
		for (const AssetRecord& asset : allAssets | std::ranges::views::values)
		{
			browserAssets_.push_back(asset);
		}
		if (context_.resourceSync_)
		{
			DynamicArray<AssetRecord> remoteAssets;
			context_.resourceSync_->Gather(remoteAssets);
			for (const AssetRecord& remote : remoteAssets)
			{
				if (!std::ranges::any_of(browserAssets_, [&remote](const AssetRecord& local) { return local.assetID_ == remote.assetID_; }))
				{
					browserAssets_.push_back(remote);
				}
			}
		}
		for (const AssetRecord& asset : browserAssets_)
		{
			std::string path = asset.path_.str();

			std::string directory;
			auto lastSlash = path.rfind('/');
			if (lastSlash != std::string::npos)
			{
				directory = path.substr(0, lastSlash);
			}

			DirectoryNode* current = &root_;
			if (!directory.empty())
			{
				std::istringstream stream(directory);
				std::string segment;
				std::string builtPath;

				while (std::getline(stream, segment, '/'))
				{
					if (!builtPath.empty())
					{
						builtPath += "/";
					}
					builtPath += segment;

					if (!current->children.contains(segment))
					{
						DirectoryNode node;
						node.name = segment;
						node.fullPath = builtPath;
						current->children.insert(segment, std::move(node));
					}
					current = &current->children.at(segment);
				}
			}

			current->assets.push_back(&asset);
		}

		if (selectedDirectory_.empty())
		{
			selectedDirectory_ = root_.fullPath;
			if (directoryHistory_.empty())
			{
				directoryHistory_.push_back(selectedDirectory_);
				historyIndex_ = 0;
			}
		}
	}

	void ContentsDrawerPanel::DrawDirectoryTree(DirectoryNode& node)
	{
		ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_AllowOverlap;

		if (node.children.empty())
		{
			flags |= ImGuiTreeNodeFlags_Leaf;
		}

		if (selectedDirectory_ == node.fullPath)
		{
			flags |= ImGuiTreeNodeFlags_Selected;
		}

		if (&node == &root_)
		{
			flags |= ImGuiTreeNodeFlags_DefaultOpen;
		}

		Bool isCut = clipboardAction_ == ClipboardAction::Cut && clipboardIsDirectory_ && clipboardPath_ == ResolveFullPath(node.fullPath);
		if (isCut)
		{
			ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.4f);
		}

		ImGui::PushID(node.name.c_str());
		Bool opened = ImGui::TreeNodeEx("##tree", flags);
		Bool treeClicked = ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen();
		DrawFolderContextMenu(node.fullPath, node.name);

		ImGui::SameLine();
		ImTextureID folderIcon = GetFolderIcon(node);
		Float iconSize = ImGui::GetTextLineHeight();
		ImGui::Image(folderIcon, ImVec2(iconSize, iconSize));
		ImGui::SameLine();
		if (!DrawInlineRename(ResolveFullPath(node.fullPath), node.name))
		{
			ImGui::Text("%s", node.name.c_str());
		}

		if (treeClicked || ImGui::IsItemClicked())
		{
			NavigateTo(node.fullPath);
		}

		if (opened)
		{
			for (auto& child : node.children | std::ranges::views::values)
			{
				DrawDirectoryTree(child);
			}
			ImGui::TreePop();
		}
		ImGui::PopID();

		if (isCut)
		{
			ImGui::PopStyleVar();
		}
	}

	void ContentsDrawerPanel::DrawAssetList()
	{
		DirectoryNode* target = &root_;

		DynamicArray<std::pair<std::string, DirectoryNode*>> breadcrumb;
		breadcrumb.push_back({ root_.name, &root_ });

		if (!selectedDirectory_.empty())
		{
			std::istringstream stream(selectedDirectory_);
			std::string segment;
			DirectoryNode* current = &root_;

			while (std::getline(stream, segment, '/'))
			{
				if (current->children.contains(segment))
				{
					current = &current->children.at(segment);
					breadcrumb.push_back({ segment, current });
				}
				else
				{
					break;
				}
			}
			target = current;
		}

		for (Size breadIndex = 0; breadIndex < breadcrumb.size(); ++breadIndex)
		{
			if (breadIndex > 0)
			{
				ImGui::SameLine(0.0f, 2.0f);
				ImGui::TextDisabled(">");
				ImGui::SameLine(0.0f, 2.0f);
			}

			auto& [name, node] = breadcrumb[breadIndex];
			if (breadIndex == breadcrumb.size() - 1)
			{
				ImGui::Text("%s", name.c_str());
			}
			else
			{
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
				ImGui::PushID(static_cast<Int>(breadIndex));
				if (ImGui::SmallButton(name.c_str()))
				{
					NavigateTo(node->fullPath);
				}
				ImGui::PopID();
				ImGui::PopStyleColor();
			}
		}
		ImGui::Separator();

		switch (viewMode_)
		{
		case ViewMode::List:
			DrawAssetListMode(target);
			break;
		case ViewMode::Grid:
			DrawAssetGridMode(target);
			break;
		}
	}

	void ContentsDrawerPanel::DrawAssetListMode(DirectoryNode* target)
	{
		Float iconSize = ImGui::GetTextLineHeight();
		Float indent = iconSize + ImGui::GetStyle().ItemSpacing.x;
		ImDrawList* drawList = ImGui::GetWindowDrawList();

		for (auto& [name, child] : target->children)
		{
			Bool isCut = clipboardAction_ == ClipboardAction::Cut && clipboardIsDirectory_ && clipboardPath_ == ResolveFullPath(child.fullPath);
			if (isCut) 
			{
				ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.4f);
			}

			ImGui::PushID(name.c_str());

			ImGui::Dummy(ImVec2(iconSize, iconSize));
			ImVec2 iconMin = ImGui::GetItemRectMin();
			ImVec2 iconMax = ImGui::GetItemRectMax();
			ImGui::SameLine();

			std::filesystem::path folderFullPath = ResolveFullPath(child.fullPath);
			if (!DrawInlineRename(folderFullPath, name))
			{
				if (ImGui::Selectable(name.c_str(), false, ImGuiSelectableFlags_SpanAvailWidth))
				{
					NavigateTo(child.fullPath);
				}
				DrawFolderContextMenu(child.fullPath, name);
			}

			ImTextureID folderIcon = GetFolderIcon(child);
			drawList->AddImage(folderIcon, iconMin, iconMax);

			ImGui::PopID();

			if (isCut) 
			{
				ImGui::PopStyleVar();
			}
		}

		for (const AssetRecord* asset : target->assets)
		{
			Bool isCut = clipboardAction_ == ClipboardAction::Cut && !clipboardIsDirectory_ && clipboardPath_ == std::filesystem::path(asset->fullpath_.str());
			if (isCut)
			{
				ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.4f);
			}

			ImGui::PushID(asset->assetID_);

			ImGui::Dummy(ImVec2(iconSize, iconSize));
			ImVec2 iconMin = ImGui::GetItemRectMin();
			ImVec2 iconMax = ImGui::GetItemRectMax();
			ImGui::SameLine();

			std::string assetFilename = std::filesystem::path(asset->path_.c_str()).filename().string();
			std::filesystem::path assetFullPath(asset->fullpath_.str());
			if (!DrawInlineRename(assetFullPath, assetFilename))
			{
				ImGui::Selectable(assetFilename.c_str(), false, ImGuiSelectableFlags_SpanAvailWidth | ImGuiSelectableFlags_AllowDoubleClick);
			}

			if ((!context_.resourceSync_ || !context_.resourceSync_->RemoteOnly(asset->assetID_)) && ImGui::BeginDragDropSource())
			{
				const Char* payloadType = GetDragDropType(asset->type_);
				ImGui::SetDragDropPayload(payloadType, &asset->assetID_, sizeof(Uint32));
				ImGui::Text("%s", std::filesystem::path(asset->path_.c_str()).filename().string().c_str());
				ImGui::EndDragDropSource();
			}

			DrawAssetContextMenu(*asset);

			if (ImGui::IsItemHovered())
			{
				DrawAssetTooltip(*asset);
				if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
				{
					OpenAssetExternal(*asset);
				}
			}

			ImTextureID icon = GetAssetIcon(*asset);
			drawList->AddImage(icon, iconMin, iconMax);

			/// [EN] The badge sits on the lower-right quarter of the icon, the
			///      corner an asset icon is least likely to fill.
			/// [JP] バッジはアイコンの右下 1/4 に重ねる。アセットのアイコンが
			///      埋めている可能性が最も低い角だから。
			ImTextureID sharingIcon = GetSharingIcon(*asset);
			if (sharingIcon)
			{
				ImVec2 badgeMin = ImVec2(iconMin.x + (iconMax.x - iconMin.x) * 0.5f, iconMin.y + (iconMax.y - iconMin.y) * 0.5f);
				drawList->AddImage(sharingIcon, badgeMin, iconMax);
			}

			ImGui::PopID();

			if (isCut) 
			{
				ImGui::PopStyleVar();
			}
		}

		DrawBackgroundContextMenu();
	}

	void ContentsDrawerPanel::DrawAssetGridMode(DirectoryNode* target)
	{
		Float availWidth = ImGui::GetContentRegionAvail().x;
		Float cellWidth = gridIconSize_ + ImGui::GetStyle().FramePadding.x * 2.0f + ImGui::GetStyle().ItemSpacing.x;
		Int columns = static_cast<Int>(availWidth / cellWidth);
		if (columns < 1)
		{
			columns = 1;
		}

		Int index = 0;

		for (auto& [name, child] : target->children)
		{
			if (index > 0 && index % columns != 0)
			{
				ImGui::SameLine();
			}

			Bool isCut = clipboardAction_ == ClipboardAction::Cut && clipboardIsDirectory_ && clipboardPath_ == ResolveFullPath(child.fullPath);
			if (isCut)
			{
				ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.4f);
			}

			ImGui::BeginGroup();

			ImTextureID folderIcon = GetFolderIcon(child);
			ImGui::PushID(name.c_str());
			if (ImGui::ImageButton("##folder", folderIcon, ImVec2(gridIconSize_, gridIconSize_)))
			{
				NavigateTo(child.fullPath);
			}

			DrawFolderContextMenu(child.fullPath, name);

			ImGui::PopID();

			Float buttonWidth = gridIconSize_ + ImGui::GetStyle().FramePadding.x * 2.0f;
			std::filesystem::path folderFullPath = ResolveFullPath(child.fullPath);
			if (!DrawInlineRename(folderFullPath, name, buttonWidth))
			{
				Float textWidth = ImGui::CalcTextSize(name.c_str()).x;
				if (textWidth > buttonWidth)
				{
					ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + buttonWidth);
					ImGui::TextWrapped("%s", name.c_str());
					ImGui::PopTextWrapPos();
				}
				else
				{
					Float offset = (buttonWidth - textWidth) * 0.5f;
					ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offset);
					ImGui::Text("%s", name.c_str());
				}
			}

			ImGui::EndGroup();

			if (isCut)
			{
				ImGui::PopStyleVar();
			}

			++index;
		}

		for (const AssetRecord* asset : target->assets)
		{
			if (index > 0 && index % columns != 0)
			{
				ImGui::SameLine();
			}

			Bool isCut = clipboardAction_ == ClipboardAction::Cut && !clipboardIsDirectory_ && clipboardPath_ == std::filesystem::path(asset->fullpath_.str());
			if (isCut) 
			{
				ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.4f);
			}

			ImGui::BeginGroup();

			ImTextureID icon = GetAssetIcon(*asset);
			ImGui::PushID(asset->assetID_);
			ImGui::ImageButton("##asset", icon, ImVec2(gridIconSize_, gridIconSize_));

			/// [EN] The badge takes a third of the button in grid mode, where the
			///      icon is large enough that a quarter would read as noise.
			/// [JP] グリッド表示ではボタンの1/3をバッジに使う。アイコンが大きいため、
			///      1/4 ではゴミのように見えてしまう。
			ImTextureID sharingIcon = GetSharingIcon(*asset);
			if (sharingIcon)
			{
				ImVec2 badgeMax = ImGui::GetItemRectMax();
				ImVec2 badgeMin = ImVec2(badgeMax.x - gridIconSize_ / 3.0f, badgeMax.y - gridIconSize_ / 3.0f);
				ImGui::GetWindowDrawList()->AddImage(sharingIcon, badgeMin, badgeMax);
			}

			if ((!context_.resourceSync_ || !context_.resourceSync_->RemoteOnly(asset->assetID_)) && ImGui::BeginDragDropSource())
			{
				const Char* payloadType = GetDragDropType(asset->type_);
				ImGui::SetDragDropPayload(payloadType, &asset->assetID_, sizeof(Uint32));
				ImGui::Text("%s", std::filesystem::path(asset->path_.c_str()).filename().string().c_str());
				ImGui::EndDragDropSource();
			}

			DrawAssetContextMenu(*asset);

			ImGui::PopID();

			if (ImGui::IsItemHovered())
			{
				DrawAssetTooltip(*asset);
				if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
				{
					OpenAssetExternal(*asset);
				}
			}

			std::string filename = std::filesystem::path(asset->path_.c_str()).filename().string();
			Float buttonWidth = gridIconSize_ + ImGui::GetStyle().FramePadding.x * 2.0f;
			std::filesystem::path assetFullPath(asset->fullpath_.str());
			if (!DrawInlineRename(assetFullPath, filename, buttonWidth))
			{
				Float textWidth = ImGui::CalcTextSize(filename.c_str()).x;
				if (textWidth > buttonWidth)
				{
					ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + buttonWidth);
					ImGui::TextWrapped("%s", filename.c_str());
					ImGui::PopTextWrapPos();
				}
				else
				{
					Float offset = (buttonWidth - textWidth) * 0.5f;
					ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offset);
					ImGui::Text("%s", filename.c_str());
				}
			}

			ImGui::EndGroup();

			if (isCut) 
			{
				ImGui::PopStyleVar();
			}

			++index;
		}

		DrawBackgroundContextMenu();
	}

	ImTextureID ContentsDrawerPanel::GetAssetTypeIcon(AssetType type)const
	{
		switch (type)
		{
		case AssetType::Model:
			return imguiTexture_.Icon(IconType::Model);
		case AssetType::Effect:
			return imguiTexture_.Icon(IconType::Effect);
		case AssetType::Audio:
			return imguiTexture_.Icon(IconType::Audio);
		case AssetType::Font:
			return imguiTexture_.Icon(IconType::Font);
		case AssetType::Skymap:
			return imguiTexture_.Icon(IconType::Sky);
		case AssetType::Animation:
			return imguiTexture_.Icon(IconType::Animation);
		case AssetType::MeshCollision:
			return imguiTexture_.Icon(IconType::MeshCollision);
		case AssetType::Material:
			return imguiTexture_.Icon(IconType::Material);
		case AssetType::Skeleton:
			return imguiTexture_.Icon(IconType::Skeleton);
		case AssetType::Movie:
			return imguiTexture_.Icon(IconType::Movie);
		case AssetType::Prefab:
			return imguiTexture_.Icon(IconType::Prefab);
		case AssetType::Scene:
			return imguiTexture_.Icon(IconType::Scene);
		case AssetType::Texture:
			[[fallthrough]];
		default:
			return imguiTexture_.Icon(IconType::Text);
		}
	}

	ImTextureID ContentsDrawerPanel::GetAssetIcon(const AssetRecord& asset)const
	{
		if (asset.type_ == AssetType::Texture && asset.isLoaded_)
		{
			if (thumbnailCache_.contains(asset.assetID_))
			{
				return thumbnailCache_.at(asset.assetID_);
			}

			TextureResource* textureResource = context_.worldContext_.resource_->GetResource<TextureResource>(AssetType::Texture);
			Handle<Texture> handle = textureResource->GetHandle(asset.assetID_);
			Texture* texture = textureResource->Resolve(*context_.worldContext_.loader_, context_.graphicsContext_.graphics_->GetBindlessHeap(), handle, context_.uiFrame_);
			if (texture && texture->Resource())
			{
				texture->Pin();

				/// [EN] Shader-visible heaps are CPU write-only, so they cannot be a
				///      CopyDescriptorsSimple source. Create the SRV directly into the
				///      ImGui heap from the texture resource instead (null desc =
				///      default view covering the whole resource).
				/// [JP] shader-visible ヒープは CPU 書き込み専用のため CopyDescriptorsSimple の
				///      コピー元にできない。代わりにテクスチャリソースから ImGui ヒープへ
				///      SRV を直接作成する（desc null = リソース全体のデフォルトビュー）。
				DescriptorHeap* descHeap = context_.graphicsContext_.imgui_->GetDescriptorHeap();
				Uint descIndex = descHeap->AllocateIndex();
				D3D12_CPU_DESCRIPTOR_HANDLE dest = descHeap->CPUHandle(descIndex);
				context_.graphicsContext_.graphics_->GetContext()->GetDevice()->CreateShaderResourceView(texture->Resource(), nullptr, dest);
				ImTextureID textureID = static_cast<ImTextureID>(descHeap->GPUHandle(descIndex).ptr);
				thumbnailCache_.insert({ asset.assetID_, textureID });
				return textureID;
			}
		}

		std::string ext = std::filesystem::path(asset.path_.c_str()).extension().string();
		if (ext == ".h")
		{
			return imguiTexture_.Icon(IconType::Header);
		}
		if (ext == ".cpp")
		{
			return imguiTexture_.Icon(IconType::Cpp);
		}
		if (ext == ".hlsli" || ext == ".hlsl")
		{
			return imguiTexture_.Icon(IconType::Hlsl);
		}

		return GetAssetTypeIcon(asset.type_);
	}

	ImTextureID ContentsDrawerPanel::GetSharingIcon(const AssetRecord& asset)const
	{
		if (!context_.resourceSync_)
		{
			return 0;
		}

		const SharedAsset* shared = context_.resourceSync_->GetAsset(asset.assetID_);
		if (!shared)
		{
			return 0;
		}

		/// [EN] A conflict comes first because it is the only state that no
		///      automatic step will clear on its own.
		/// [JP] 競合を最優先にする。自動の処理では解消されない唯一の状態だから。
		if (context_.resourceSync_->Conflicted(asset.assetID_))
		{
			return imguiTexture_.Icon(IconType::SharedConflict);
		}

		/// [EN] Someone else's lease is next, since it is the one state that
		///      stops this member from doing anything with the asset.
		/// [JP] 次は他のメンバーの Lease。このメンバーがそのアセットに何もできない、
		///      唯一の状態だから。
		for (const EditLease& lease : context_.resourceSync_->GetLeases())
		{
			if (lease.assetId_ == shared->id_ && !lease.mine_)
			{
				return imguiTexture_.Icon(IconType::Lock);
			}
		}

		/// [EN] Unsent work outranks holding the lease, because the lease is
		///      already visible in the panel while unsent work is not.
		/// [JP] 未送信の作業は、Lease を持っていることより優先する。Lease はパネルに
		///      既に出ているが、未送信の作業はどこにも出ないため。
		if (context_.resourceSync_->Modified(asset.assetID_))
		{
			return imguiTexture_.Icon(IconType::SharedModified);
		}

		for (const EditLease& lease : context_.resourceSync_->GetLeases())
		{
			if (lease.assetId_ == shared->id_)
			{
				return imguiTexture_.Icon(IconType::Unlock);
			}
		}

		/// [EN] Remote-only means the catalog has it but this workspace does
		///      not, so it is on its way in rather than usable.
		/// [JP] RemoteOnly はカタログにあってこのワークスペースに無い状態。
		///      使えるのではなく、これから入ってくるということ。
		if (context_.resourceSync_->RemoteOnly(asset.assetID_))
		{
			return imguiTexture_.Icon(IconType::SharedOutdated);
		}

		return imguiTexture_.Icon(IconType::SharedAsset);
	}

	void ContentsDrawerPanel::DrawAssetTooltip(const AssetRecord& asset)
	{
		ImGui::BeginTooltip();

		ImTextureID icon = GetAssetIcon(asset);
		Float previewSize = 128.0f;

		if (asset.type_ == AssetType::Texture && asset.isLoaded_)
		{
			ImGui::Image(icon, ImVec2(previewSize, previewSize));
			ImGui::Separator();
		}
		else
		{
			ImGui::Image(icon, ImVec2(ImGui::GetTextLineHeight(), ImGui::GetTextLineHeight()));
			ImGui::SameLine();
		}

		ImGui::Text("%s", asset.path_.c_str());
		ImGui::Text("ID: %u", asset.assetID_);
		if (context_.resourceSync_)
		{
			ResourceSyncControlPanel::DrawState(context_, asset);
		}

		std::error_code errorCode;
		auto fileSize = std::filesystem::file_size(std::filesystem::path(asset.fullpath_.c_str()), errorCode);
		if (!errorCode)
		{
			if (fileSize >= 1024 * 1024)
			{
				ImGui::Text("%.2f MB", static_cast<Float>(fileSize) / (1024.0f * 1024.0f));
			}
			else if (fileSize >= 1024)
			{
				ImGui::Text("%.1f KB", static_cast<Float>(fileSize) / 1024.0f);
			}
			else
			{
				ImGui::Text("%llu Bytes", fileSize);
			}
		}

		if (asset.type_ == AssetType::Texture && asset.isLoaded_)
		{
			TextureResource* textureResource = context_.worldContext_.resource_->GetResource<TextureResource>(AssetType::Texture);
			Handle<Texture> handle = textureResource->GetHandle(asset.assetID_);
			Texture* texture = textureResource->Resolve(*context_.worldContext_.loader_, context_.graphicsContext_.graphics_->GetBindlessHeap(), handle, context_.uiFrame_);
			if (texture && texture->Resource())
			{
				D3D12_RESOURCE_DESC desc = texture->Resource()->GetDesc();
				ImGui::Text("%llu x %u", desc.Width, desc.Height);
			}
		}

		ImGui::EndTooltip();
	}

	void ContentsDrawerPanel::OpenAssetExternal(const AssetRecord& asset)
	{
		if (context_.resourceSync_ && context_.resourceSync_->RemoteOnly(asset.assetID_))
		{
			context_.resourceSync_->RequestGet(asset.assetID_);
			return;
		}
		if (asset.type_ == AssetType::Scene)
		{
			context_.sceneContext_.requestedSceneAssetID_ = asset.assetID_;
			return;
		}

		if (asset.type_ == AssetType::Material)
		{
			context_.panelContext_.materialViewerPanel_->Open();
			return;
		}

		std::wstring widePath = asset.fullpath_.w_str();
		ShellExecuteW(NULL, L"open", widePath.c_str(), NULL, NULL, SW_SHOWNORMAL);
	}

	const Char* ContentsDrawerPanel::GetDragDropType(AssetType type)const
	{
		switch (type)
		{
		case AssetType::Texture:
			return "ASSET_TEXTURE";
		case AssetType::Model:
			return "ASSET_MODEL";
		case AssetType::Effect:
			return "ASSET_EFFECT";
		case AssetType::Audio:
			return "ASSET_AUDIO";
		case AssetType::Font:
			return "ASSET_FONT";
		case AssetType::Movie:
			return "ASSET_MOVIE";
		case AssetType::Animation:
			return "ASSET_ANIMATION";
		case AssetType::MeshCollision:
			return "ASSET_MESHCOLLISION";
		case AssetType::Material:
			return "ASSET_MATERIAL";
		case AssetType::Skeleton:
			return "ASSET_SKELETON";
		case AssetType::Skymap:
			return "ASSET_SKY";
		case AssetType::Prefab:
			return "ASSET_PREFAB";
		case AssetType::Scene:
			return "ASSET_SCENE";
		default:
			return "ASSET_UNKNOWN";
		}
	}

	ImTextureID ContentsDrawerPanel::GetFolderIcon(const DirectoryNode& node)const
	{
		if (!node.children.empty() || !node.assets.empty())
		{
			return imguiTexture_.Icon(IconType::FolderInItem);
		}
		return imguiTexture_.Icon(IconType::FolderNoItem);
	}

	void ContentsDrawerPanel::DrawFolderContextMenu(const std::string& relativePath, const std::string& folderName)
	{
		if (ImGui::BeginPopupContextItem("##FolderContext"))
		{
			if (ImGui::MenuItem("新規フォルダ"))
			{
				CreateNewFolder(relativePath);
				ImGui::CloseCurrentPopup();
			}

			if (ImGui::MenuItem("新規 C++ スクリプト"))
			{
				RequestCreateScript(relativePath);
				ImGui::CloseCurrentPopup();
			}

			if (ImGui::MenuItem("新規 C# スクリプト"))
			{
				// TODO C#
				ImGui::CloseCurrentPopup();
			}

			ImGui::Separator();

			if (ImGui::MenuItem("切り取り"))
			{
				clipboardAction_ = ClipboardAction::Cut;
				clipboardPath_ = ResolveFullPath(relativePath);
				clipboardIsDirectory_ = true;
				ImGui::CloseCurrentPopup();
			}

			if (ImGui::MenuItem("コピー"))
			{
				clipboardAction_ = ClipboardAction::Copy;
				clipboardPath_ = ResolveFullPath(relativePath);
				clipboardIsDirectory_ = true;
				ImGui::CloseCurrentPopup();
			}

			Bool canPaste = clipboardAction_ != ClipboardAction::None;
			if (ImGui::MenuItem("貼り付け", nullptr, false, canPaste))
			{
				ExecutePaste(relativePath);
				ImGui::CloseCurrentPopup();
			}

			ImGui::Separator();

			if (ImGui::MenuItem("名前変更"))
			{
				renaming_ = true;
				renameNeedsFocus_ = true;
				renameTargetPath_ = ResolveFullPath(relativePath);
				renameBuffer_ = folderName;
				renameBuffer_.resize(256);
				ImGui::CloseCurrentPopup();
			}

			if (ImGui::MenuItem("削除"))
			{
				ExecuteDelete(ResolveFullPath(relativePath));
				ImGui::CloseCurrentPopup();
			}

			ImGui::Separator();

			if (ImGui::MenuItem("エクスプローラーで開く"))
			{
				std::filesystem::path fullPath = ResolveFullPath(relativePath);
				ShellExecuteW(NULL, L"explore", fullPath.wstring().c_str(), NULL, NULL, SW_SHOWNORMAL);
				ImGui::CloseCurrentPopup();
			}

			ImGui::EndPopup();
		}
	}

	void ContentsDrawerPanel::DrawAssetContextMenu(const AssetRecord& asset)
	{
		if (ImGui::BeginPopupContextItem("##AssetContext"))
		{
			if (context_.resourceSync_)
			{
				ResourceSyncControlPanel::DrawActions(context_, asset);
			}
			if (ImGui::MenuItem("開く"))
			{
				OpenAssetExternal(asset);
				ImGui::CloseCurrentPopup();
			}

			if (asset.type_ == AssetType::Model && ImGui::BeginMenu("アセットアクション"))
			{
				if (ImGui::BeginMenu("コリジョン生成"))
				{
					if (ImGui::MenuItem("Proxy"))
					{
						GenerateMeshCollision(asset, MeshCollisionDetail::Proxy);
						ImGui::CloseCurrentPopup();
					}

					if (ImGui::MenuItem("Exact"))
					{
						GenerateMeshCollision(asset, MeshCollisionDetail::Exact);
						ImGui::CloseCurrentPopup();
					}

					ImGui::EndMenu();
				}

				if (ImGui::MenuItem("モデル変換"))
				{
					context_.modelTransformPreviewContext_.requestedAssetId_ = asset.assetID_;
					ImGui::CloseCurrentPopup();
				}

				if (ImGui::MenuItem("マテリアル生成"))
				{
					GenerateMaterial(asset);
					ImGui::CloseCurrentPopup();
				}

				if (ImGui::MenuItem("スケルトン生成"))
				{
					GenerateSkeleton(asset);
					ImGui::CloseCurrentPopup();
				}

				if (ImGui::BeginMenu("エクスポート"))
				{
					if (ImGui::MenuItem("glTF"))
					{
						ExportModel(asset, ExportPreset::Gltf, L"gltf");
						ImGui::CloseCurrentPopup();
					}

					if (ImGui::MenuItem("glTF binary"))
					{
						ExportModel(asset, ExportPreset::Glb, L"glb");
						ImGui::CloseCurrentPopup();
					}

					if (ImGui::MenuItem("FBX (Maya)"))
					{
						ExportModel(asset, ExportPreset::FbxMaya, L"fbx");
						ImGui::CloseCurrentPopup();
					}

					if (ImGui::MenuItem("FBX (Unreal)"))
					{
						ExportModel(asset, ExportPreset::FbxUnreal, L"fbx");
						ImGui::CloseCurrentPopup();
					}

					if (ImGui::MenuItem("FBX (Unity)"))
					{
						ExportModel(asset, ExportPreset::FbxUnity, L"fbx");
						ImGui::CloseCurrentPopup();
					}

					if (ImGui::MenuItem("FBX (エンジン)"))
					{
						ExportModel(asset, ExportPreset::FbxNative, L"fbx");
						ImGui::CloseCurrentPopup();
					}

					ImGui::EndMenu();
				}

				ImGui::EndMenu();
			}

			ImGui::Separator();

			if (ImGui::MenuItem("切り取り"))
			{
				clipboardAction_ = ClipboardAction::Cut;
				clipboardPath_ = std::filesystem::path(asset.fullpath_.str());
				clipboardIsDirectory_ = false;
				ImGui::CloseCurrentPopup();
			}

			if (ImGui::MenuItem("コピー"))
			{
				clipboardAction_ = ClipboardAction::Copy;
				clipboardPath_ = std::filesystem::path(asset.fullpath_.str());
				clipboardIsDirectory_ = false;
				ImGui::CloseCurrentPopup();
			}

			ImGui::Separator();

			if (ImGui::MenuItem("名前変更"))
			{
				renaming_ = true;
				renameNeedsFocus_ = true;
				renameTargetPath_ = std::filesystem::path(asset.fullpath_.str());
				renameBuffer_ = std::filesystem::path(asset.path_.c_str()).filename().string();
				renameBuffer_.resize(256);
				ImGui::CloseCurrentPopup();
			}

			if (ImGui::MenuItem("削除"))
			{
				ExecuteDelete(std::filesystem::path(asset.fullpath_.str()));
				ImGui::CloseCurrentPopup();
			}

			ImGui::Separator();

			if (ImGui::MenuItem("エクスプローラーで表示"))
			{
				std::wstring param = L"/select,\"" + std::filesystem::path(asset.fullpath_.str()).wstring() + L"\"";
				ShellExecuteW(NULL, L"open", L"explorer.exe", param.c_str(), NULL, SW_SHOWNORMAL);
				ImGui::CloseCurrentPopup();
			}

			ImGui::EndPopup();
		}
	}

	void ContentsDrawerPanel::GenerateMeshCollision(const AssetRecord& asset, MeshCollisionDetail detail)
	{
		D3D12Context* d3d12Context = context_.graphicsContext_.graphics_->GetContext();

		Bool baked = context_.worldContext_.resource_->GetResource<ModelResource>(AssetType::Model)->GenerateCollision(*context_.worldContext_.loader_, d3d12Context->GetDevice(), d3d12Context->GetDirectQueue(), context_.graphicsContext_.graphics_->GetBindlessHeap(), context_.graphicsContext_.graphics_->GetBC7CompressShader(), *context_.worldContext_.resource_, asset.assetID_, detail);
		if (!baked)
		{
			SC_LOG_WARNING("ContentsDrawerPanel: コリジョン生成に失敗しました: {}", asset.path_.c_str());
			return;
		}

		/// [EN] Rescan so the just-written ".collision" sibling is picked up as its own asset, and rebuild the tree so it shows in the panel.
		/// [JP] 書き出した ".collision" 兄弟を個別アセットとして拾えるよう再スキャンし、パネルに出るようツリーを再構築する。
		context_.worldContext_.resource_->Reload(*context_.worldContext_.loader_, d3d12Context->GetDevice(), d3d12Context->GetDirectQueue(), context_.graphicsContext_.graphics_->GetBC7CompressShader());
		needsRebuild_ = true;

		SC_LOG_NOTICE("ContentsDrawerPanel: コリジョンを生成しました: {}", asset.path_.c_str());
	}

	void ContentsDrawerPanel::GenerateMaterial(const AssetRecord& asset)
	{
		D3D12Context* d3d12Context = context_.graphicsContext_.graphics_->GetContext();

		Bool written = context_.worldContext_.resource_->GetResource<ModelResource>(AssetType::Model)->GenerateMaterial(*context_.worldContext_.loader_, d3d12Context->GetDevice(), d3d12Context->GetDirectQueue(), context_.graphicsContext_.graphics_->GetBindlessHeap(), context_.graphicsContext_.graphics_->GetBC7CompressShader(), *context_.worldContext_.resource_, asset.assetID_, true);
		if (!written)
		{
			SC_LOG_WARNING("ContentsDrawerPanel: マテリアル生成に失敗しました: {}", asset.path_.c_str());
			return;
		}

		/// [EN] Rescan so the just-written ".material" siblings are picked up as their own assets, and rebuild the tree so they show in the panel.
		/// [JP] 書き出した ".material" 兄弟を個別アセットとして拾えるよう再スキャンし、パネルに出るようツリーを再構築する。
		context_.worldContext_.resource_->Reload(*context_.worldContext_.loader_, d3d12Context->GetDevice(), d3d12Context->GetDirectQueue(), context_.graphicsContext_.graphics_->GetBC7CompressShader());
		needsRebuild_ = true;

		SC_LOG_NOTICE("ContentsDrawerPanel: マテリアルを生成しました: {}", asset.path_.c_str());
	}

	void ContentsDrawerPanel::GenerateSkeleton(const AssetRecord& asset)
	{
		D3D12Context* d3d12Context = context_.graphicsContext_.graphics_->GetContext();

		Bool written = context_.worldContext_.resource_->GetResource<ModelResource>(AssetType::Model)->GenerateSkeleton(*context_.worldContext_.loader_, d3d12Context->GetDevice(), d3d12Context->GetDirectQueue(), context_.graphicsContext_.graphics_->GetBindlessHeap(), context_.graphicsContext_.graphics_->GetBC7CompressShader(), *context_.worldContext_.resource_, asset.assetID_, false);
		if (!written)
		{
			SC_LOG_WARNING("ContentsDrawerPanel: スケルトン生成に失敗しました（スキン無し？）: {}", asset.path_.c_str());
			return;
		}

		/// [EN] Rescan so the just-written ".skeleton" sibling is picked up as its own asset, and rebuild the tree so it shows in the panel.
		/// [JP] 書き出した ".skeleton" 兄弟を個別アセットとして拾えるよう再スキャンし、パネルに出るようツリーを再構築する。
		context_.worldContext_.resource_->Reload(*context_.worldContext_.loader_, d3d12Context->GetDevice(), d3d12Context->GetDirectQueue(), context_.graphicsContext_.graphics_->GetBC7CompressShader());
		needsRebuild_ = true;

		SC_LOG_NOTICE("ContentsDrawerPanel: スケルトンを生成しました: {}", asset.path_.c_str());
	}

	void ContentsDrawerPanel::ExportModel(const AssetRecord& asset, ExportPreset preset, const Wchar* extension)
	{
		std::filesystem::path sourcePath(asset.fullpath_.str());

		std::wstring filterName = L"*.";
		filterName += extension;
		std::wstring filterExt = L"*.";
		filterExt += extension;

		std::wstring initialFileName = sourcePath.stem().wstring();

		std::filesystem::path outputPath;
		if (!FileDialog::SaveFile(outputPath, sourcePath.parent_path(), filterName.c_str(), filterExt.c_str(), extension, initialFileName.c_str()))
		{
			return;
		}

		D3D12Context* d3d12Context = context_.graphicsContext_.graphics_->GetContext();

		Bool exported = context_.worldContext_.resource_->GetResource<ModelResource>(AssetType::Model)->Export(*context_.worldContext_.loader_, d3d12Context->GetDevice(), d3d12Context->GetDirectQueue(), context_.graphicsContext_.graphics_->GetBindlessHeap(), context_.graphicsContext_.graphics_->GetBC7CompressShader(), *context_.worldContext_.resource_, asset.assetID_, preset, String(outputPath.string()));
		if (!exported)
		{
			SC_LOG_WARNING("ContentsDrawerPanel: モデルのエクスポートに失敗しました: {}", asset.path_.c_str());
			return;
		}

		context_.worldContext_.resource_->Reload(*context_.worldContext_.loader_, d3d12Context->GetDevice(), d3d12Context->GetDirectQueue(), context_.graphicsContext_.graphics_->GetBC7CompressShader());
		needsRebuild_ = true;

		SC_LOG_NOTICE("ContentsDrawerPanel: モデルをエクスポートしました: {}", asset.path_.c_str());
	}

	void ContentsDrawerPanel::DrawBackgroundContextMenu()
	{
		if (ImGui::BeginPopupContextWindow("##BackgroundContext", ImGuiPopupFlags_NoOpenOverItems | ImGuiPopupFlags_MouseButtonRight))
		{
			if (ImGui::MenuItem("新規フォルダ"))
			{
				CreateNewFolder(selectedDirectory_);
				ImGui::CloseCurrentPopup();
			}

			if (ImGui::MenuItem("新規 C++ スクリプト"))
			{
				RequestCreateScript(selectedDirectory_);
				ImGui::CloseCurrentPopup();
			}

			if (ImGui::MenuItem("新規 C# スクリプト"))
			{
				// TODO C#
				ImGui::CloseCurrentPopup();
			}

			Bool canPaste = clipboardAction_ != ClipboardAction::None;
			if (ImGui::MenuItem("貼り付け", nullptr, false, canPaste))
			{
				ExecutePaste(selectedDirectory_);
				ImGui::CloseCurrentPopup();
			}

			ImGui::Separator();

			if (ImGui::MenuItem("エクスプローラーで開く"))
			{
				std::filesystem::path fullPath = ResolveFullPath(selectedDirectory_);
				ShellExecuteW(NULL, L"explore", fullPath.wstring().c_str(), NULL, NULL, SW_SHOWNORMAL);
				ImGui::CloseCurrentPopup();
			}

			ImGui::EndPopup();
		}

		DrawCreateScriptPopup();
	}

	Bool ContentsDrawerPanel::DrawInlineRename(const std::filesystem::path& itemFullPath, const std::string& displayName, Float width)
	{
		if (!renaming_ || renameTargetPath_ != itemFullPath)
		{
			return false;
		}

		if (renameNeedsFocus_)
		{
			ImGui::SetKeyboardFocusHere();
			renameNeedsFocus_ = false;
		}

		if (width > 0.0f)
		{
			ImGui::SetNextItemWidth(width);
		}

		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2.0f, 0.0f));
		Bool confirmed = ImGui::InputText("##InlineRename", renameBuffer_.data(), renameBuffer_.capacity(), ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);
		ImGui::PopStyleVar();

		if (confirmed)
		{
			std::string newName(renameBuffer_.c_str());
			if (!newName.empty())
			{
				ExecuteRename(renameTargetPath_, newName);
			}
			renaming_ = false;
		}
		else if (ImGui::IsItemDeactivated())
		{
			renaming_ = false;
		}

		return true;
	}

	std::filesystem::path ContentsDrawerPanel::ResolveFullPath(const std::string& relativePath)const
	{
		if (relativePath.empty())
		{
			return context_.worldContext_.resource_->ProjectRootPath();
		}
		return context_.worldContext_.resource_->ProjectRootPath() / relativePath;
	}

	void ContentsDrawerPanel::CreateNewFolder(const std::string& parentRelative)
	{
		std::filesystem::path parentFull = ResolveFullPath(parentRelative);
		std::string baseName = "New Folder";
		std::filesystem::path newPath = parentFull / baseName;

		if (std::filesystem::exists(newPath))
		{
			for (Int index = 2; ; ++index)
			{
				newPath = parentFull / (baseName + "(" + std::to_string(index) + ")");
				if (!std::filesystem::exists(newPath))
				{
					break;
				}
			}
		}

		std::error_code errorCode;
		std::filesystem::create_directories(newPath, errorCode);

		std::string newName = newPath.filename().string();
		std::string newRelative = parentRelative.empty() ? newName : parentRelative + "/" + newName;

		DirectoryNode* parent = &root_;
		if (!parentRelative.empty())
		{
			std::istringstream stream(parentRelative);
			std::string segment;
			while (std::getline(stream, segment, '/'))
			{
				if (parent->children.contains(segment))
				{
					parent = &parent->children.at(segment);
				}
			}
		}

		DirectoryNode node;
		node.name = newName;
		node.fullPath = newRelative;
		parent->children.insert(newName, std::move(node));

		renaming_ = true;
		renameNeedsFocus_ = true;
		renameTargetPath_ = newPath;
		renameBuffer_ = newName;
		renameBuffer_.resize(256);
	}

	void ContentsDrawerPanel::RequestCreateScript(const std::string& parentRelative)
	{
		openCreateScriptPopup_ = true;
		createScriptNeedsFocus_ = true;
		createScriptParentRelative_ = parentRelative;
		createScriptNameBuffer_ = "NewScript";
		createScriptNameBuffer_.resize(256);
	}

	void ContentsDrawerPanel::DrawCreateScriptPopup()
	{
		if (openCreateScriptPopup_)
		{
			ImGui::OpenPopup("新規 C++ スクリプト");
			openCreateScriptPopup_ = false;
		}

		Bool open = true;
		if (ImGui::BeginPopupModal("新規 C++ スクリプト", &open, ImGuiWindowFlags_AlwaysAutoResize))
		{
			if (createScriptNeedsFocus_)
			{
				ImGui::SetKeyboardFocusHere();
				createScriptNeedsFocus_ = false;
			}

			Bool confirmed = ImGui::InputText("スクリプト名", createScriptNameBuffer_.data(), createScriptNameBuffer_.capacity(), ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);

			ImGui::Separator();

			if (ImGui::Button("作成") || confirmed)
			{
				CreateNewScript(createScriptParentRelative_, std::string(createScriptNameBuffer_.c_str()));
				ImGui::CloseCurrentPopup();
			}

			ImGui::SameLine();

			if (ImGui::Button("キャンセル"))
			{
				ImGui::CloseCurrentPopup();
			}

			ImGui::EndPopup();
		}
	}

	void ContentsDrawerPanel::CreateNewScript(const std::string& parentRelative, const std::string& requestedName)
	{
		/// [EN] Sanitize down to a valid C++ identifier: keep only alnum/underscore, and prefix an underscore if the result would otherwise start with a digit (or be empty).
		/// [JP] 有効なC++識別子まで削る: 英数字とアンダースコアのみを残し、結果が数字始まり(または空)になる場合はアンダースコアを前置する。
		std::string sanitized;
		std::ranges::copy_if(requestedName, std::back_inserter(sanitized), [](Char character) { return std::isalnum(static_cast<unsigned char>(character)) || character == '_'; });
		if (sanitized.empty())
		{
			sanitized = "NewScript";
		}
		if (std::isdigit(static_cast<unsigned char>(sanitized.front())))
		{
			sanitized.insert(sanitized.begin(), '_');
		}

		/// [EN] UserProject.vcxproj lives at UserProject/ and its Include paths are relative to that directory, but parentRelative (like every other DirectoryNode path in this panel) is relative to the repository root. Scripts only make sense under UserProject (the only project SeedScript-derived types can be part of), so anything outside it falls back to UserProject/Script instead of failing outright.
		/// [JP] UserProject.vcxproj は UserProject/ にあり、その Include パスは同ディレクトリからの相対パスになる。一方 parentRelative は(このパネルの他のDirectoryNodeパスと同様)リポジトリルートからの相対パス。スクリプトは UserProject 配下でしか意味を持たない(SeedScript 派生型が所属できる唯一のプロジェクトのため)ので、その外側が指定された場合は失敗させずに UserProject/Script へフォールバックする。
		std::string projectRelativeDirectory;
		if (parentRelative == "UserProject")
		{
			projectRelativeDirectory = "";
		}
		else if (parentRelative.rfind("UserProject/", 0) == 0)
		{
			projectRelativeDirectory = parentRelative.substr(std::string("UserProject/").size());
		}
		else
		{
			projectRelativeDirectory = "Script";
		}

		std::filesystem::path targetDirectory = context_.worldContext_.resource_->ProjectRootPath() / "UserProject" / projectRelativeDirectory;

		std::string baseName = sanitized;
		std::string finalName = baseName;
		for (Int index = 2; std::filesystem::exists(targetDirectory / (finalName + ".h")) || std::filesystem::exists(targetDirectory / (finalName + ".cpp")); ++index)
		{
			finalName = baseName + "(" + std::to_string(index) + ")";
		}

		std::error_code errorCode;
		std::filesystem::create_directories(targetDirectory, errorCode);

		std::string projectRelativeHeader = projectRelativeDirectory.empty() ? (finalName + ".h") : (projectRelativeDirectory + "/" + finalName + ".h");
		std::string projectRelativeCpp = projectRelativeDirectory.empty() ? (finalName + ".cpp") : (projectRelativeDirectory + "/" + finalName + ".cpp");

		std::string headerContent =
			"#pragma once\n"
			"#include <FoundationEngine/Prelude.h>\n"
			"#include <FoundationEngine/SeedScript.h>\n"
			"\n"
			"class " + finalName + " :public SeedCore::SeedScript\n"
			"{\n"
			"public:\n"
			"\tvoid OnStart(); // 開始時に呼ばれる初期化処理\n"
			"\n"
			"\tvoid OnTick(float elapsedTime); // 更新処理\n"
			"};\n"
			"REGISTER_COMPONENT(" + finalName + ");\n";

		std::string cppContent =
			"#include \"UserProject/" + projectRelativeHeader + "\"\n"
			"\n"
			"void " + finalName + "::OnStart()\n"
			"{\n"
			"\n"
			"}\n"
			"\n"
			"void " + finalName + "::OnTick(float elapsedTime)\n"
			"{\n"
			"\n"
			"}\n";

		std::filesystem::path headerFullPath = targetDirectory / (finalName + ".h");
		std::filesystem::path cppFullPath = targetDirectory / (finalName + ".cpp");

		/// [EN] Files must exist on disk before registration: both registration paths require it — the Visual Studio automation path's AddFromFile() expects an existing file, and even the Python/XML path, while it wouldn't itself fail on a missing file, would leave the .vcxproj referencing something that isn't there yet if a build raced it.
		/// [JP] 登録の前にファイルをディスクへ書き出しておく必要がある: どちらの登録経路でも要求される — Visual Studio自動化経路の AddFromFile() は既存ファイルを前提とし、Python/XML経路自体はファイルが無くても失敗はしないものの、その隙にビルドが走れば .vcxproj がまだ存在しないファイルを参照する状態になってしまう。
		std::ofstream headerFile(headerFullPath, std::ios::binary);
		headerFile << headerContent;
		headerFile.close();

		std::ofstream cppFile(cppFullPath, std::ios::binary);
		cppFile << cppContent;
		cppFile.close();

		if (!RegisterScriptInProject(headerFullPath, cppFullPath))
		{
			SC_LOG_WARNING("ContentsDrawerPanel: スクリプトのプロジェクトへの登録に失敗しました: {}", finalName);
			return;
		}

		needsRebuild_ = true;

		SC_LOG_NOTICE("ContentsDrawerPanel: スクリプトを作成しました: {}", finalName);

		ShellExecuteW(NULL, L"open", headerFullPath.wstring().c_str(), NULL, NULL, SW_SHOWNORMAL);
		ShellExecuteW(NULL, L"open", cppFullPath.wstring().c_str(), NULL, NULL, SW_SHOWNORMAL);
	}

	Bool ContentsDrawerPanel::RegisterScriptInProject(const std::filesystem::path& headerFullPath, const std::filesystem::path& cppFullPath)
	{
		std::filesystem::path projectRoot = context_.worldContext_.resource_->ProjectRootPath();

		/// [EN] Tried first: if Visual Studio has Runtime.sln open, letting it add the files itself keeps Solution Explorer in sync immediately and never triggers the "project modified outside the editor" reload prompt (see VisualStudioAutomation's own doc comment). Falls through to editing UserProject.vcxproj directly — the only path available when Visual Studio isn't running this solution at all.
		/// [JP] まずこちらを試す: Visual Studio が Runtime.sln を開いていれば、ファイルの追加自体をVSにやらせることで Solution Explorer が即座に同期され、「プロジェクトが外部で変更されました」という再読み込み確認も一切発生しない(詳細は VisualStudioAutomation 自身のドキュメントコメントを参照)。Visual Studio がこのソリューションを開いていない場合にのみ、UserProject.vcxproj を直接編集する経路へフォールバックする。
		if (VisualStudioAutomation::TryAddFilesToProject(projectRoot / "Runtime" / "Runtime.sln", "UserProject", headerFullPath, cppFullPath))
		{
			return true;
		}

		std::filesystem::path scriptPath = projectRoot / "Tools" / "Python" / "CreateScript.py";

		std::string headerRelative = std::filesystem::relative(headerFullPath, projectRoot / "UserProject").string();
		std::string cppRelative = std::filesystem::relative(cppFullPath, projectRoot / "UserProject").string();

		std::wstring commandLine = std::format(L"py \"{}\" \"{}\" \"{}\" \"{}\"", scriptPath.wstring(), projectRoot.wstring(), std::filesystem::path(headerRelative).wstring(), std::filesystem::path(cppRelative).wstring());

		STARTUPINFOW startupInfo{};
		startupInfo.cb = sizeof(startupInfo);
		PROCESS_INFORMATION processInfo{};

		DynamicArray<Wchar> mutableCommandLine(commandLine.begin(), commandLine.end());
		mutableCommandLine.push_back(L'\0');

		Bool created = CreateProcessW(nullptr, mutableCommandLine.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &startupInfo, &processInfo);
		if (!created)
		{
			return false;
		}

		WaitForSingleObject(processInfo.hProcess, INFINITE);

		DWORD exitCode = 1;
		GetExitCodeProcess(processInfo.hProcess, &exitCode);

		CloseHandle(processInfo.hProcess);
		CloseHandle(processInfo.hThread);

		return exitCode == 0;
	}

	void ContentsDrawerPanel::ExecuteDelete(const std::filesystem::path& fullPath)
	{
		if (context_.resourceSync_ && context_.resourceSync_->Managed(fullPath))
		{
			SC_LOG_WARNING("Shared content cannot be deleted or renamed through local file operations.");
			return;
		}
		std::error_code errorCode;
		std::filesystem::remove_all(fullPath, errorCode);

		std::filesystem::path metaPath = fullPath;
		metaPath += ".meta";
		if (std::filesystem::exists(metaPath))
		{
			std::filesystem::remove(metaPath, errorCode);
		}

		if (clipboardPath_ == fullPath)
		{
			clipboardAction_ = ClipboardAction::None;
		}

		needsRebuild_ = true;
	}

	void ContentsDrawerPanel::ExecuteRename(const std::filesystem::path& oldPath, const std::string& newName)
	{
		if (context_.resourceSync_ && context_.resourceSync_->Managed(oldPath))
		{
			SC_LOG_WARNING("Shared content cannot be deleted or renamed through local file operations.");
			return;
		}
		std::filesystem::path newPath = oldPath.parent_path() / newName;
		std::error_code errorCode;
		std::filesystem::rename(oldPath, newPath, errorCode);

		std::filesystem::path oldMeta = oldPath;
		oldMeta += ".meta";
		if (std::filesystem::exists(oldMeta))
		{
			std::filesystem::path newMeta = newPath;
			newMeta += ".meta";
			std::filesystem::rename(oldMeta, newMeta, errorCode);
		}

		if (clipboardPath_ == oldPath)
		{
			clipboardPath_ = newPath;
		}

		needsRebuild_ = true;
	}

	void ContentsDrawerPanel::ExecutePaste(const std::string& destinationRelative)
	{
		if (clipboardAction_ == ClipboardAction::None || !std::filesystem::exists(clipboardPath_))
		{
			clipboardAction_ = ClipboardAction::None;
			return;
		}

		std::filesystem::path destDirectory = ResolveFullPath(destinationRelative);
		std::filesystem::path destPath = destDirectory / clipboardPath_.filename();
		if (context_.resourceSync_ && (context_.resourceSync_->Managed(clipboardPath_) || context_.resourceSync_->Managed(destPath)))
		{
			SC_LOG_WARNING("Shared content cannot be moved or copied through local clipboard operations.");
			return;
		}
		std::error_code errorCode;

		if (clipboardAction_ == ClipboardAction::Cut)
		{
			std::filesystem::rename(clipboardPath_, destPath, errorCode);

			std::filesystem::path metaPath = clipboardPath_;
			metaPath += ".meta";
			if (std::filesystem::exists(metaPath))
			{
				std::filesystem::path destMeta = destPath;
				destMeta += ".meta";
				std::filesystem::rename(metaPath, destMeta, errorCode);
			}

			clipboardAction_ = ClipboardAction::None;
		}
		else if (clipboardAction_ == ClipboardAction::Copy)
		{
			if (clipboardIsDirectory_)
			{
				std::filesystem::copy(clipboardPath_, destPath, std::filesystem::copy_options::recursive, errorCode);
			}
			else
			{
				std::filesystem::copy_file(clipboardPath_, destPath, std::filesystem::copy_options::skip_existing, errorCode);
			}
		}

		needsRebuild_ = true;
	}

	void ContentsDrawerPanel::NavigateTo(const std::string& directory)
	{
		if (selectedDirectory_ == directory)
		{
			return;
		}

		if (historyIndex_ >= 0 && historyIndex_ < static_cast<Int>(directoryHistory_.size()) - 1)
		{
			directoryHistory_.erase(directoryHistory_.begin() + historyIndex_ + 1, directoryHistory_.end());
		}

		directoryHistory_.push_back(directory);
		historyIndex_ = static_cast<Int>(directoryHistory_.size()) - 1;
		selectedDirectory_ = directory;
	}
}
