#include <Editor/Editor/Panel/ContentsDrawerPanel.h>
#include <Editor/Editor/EditorContext.h>
#include <Editor/Editor/ImGui/ImGuiTexture.h>
#include <FoundationEngine/Resource/ResourceCache.h>
#include <FoundationEngine/Resource/Prefab.h>
#include <FoundationEngine/ECS/Actor.h>
#include <GraphicsEngine/Texture/ImageResource.h>
#include <GraphicsEngine/Texture/Texture.h>
#include <GraphicsEngine/D3D12/Descriptor/BindlessHeap.h>
#include <GraphicsEngine/D3D12/Descriptor/DescriptorHeap.h>
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
		directoryWatchHandle_ = FindFirstChangeNotificationW(projectRoot.wstring().c_str(), TRUE, FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE);
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
		ImGuiID dockspaceID = ImGui::GetID("ScDockSpace");
		ImGui::SetNextWindowDockID(dockspaceID, ImGuiCond_FirstUseEver);

		if (directoryWatchHandle_ != INVALID_HANDLE_VALUE && WaitForSingleObject(directoryWatchHandle_, 0) == WAIT_OBJECT_0)
		{
			context_.worldContext_.resource_->Reload(*context_.worldContext_.loader_, context_.graphicsContext_.device_, context_.graphicsContext_.cmdQueue_, *context_.graphicsContext_.bc7Shader_);
			needsRebuild_ = true;
			FindNextChangeNotification(directoryWatchHandle_);
		}

		if (ImGui::Begin("コンテンツドロワー"))
		{
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
				auto results = context_.worldContext_.resource_->Search(String(searchKey));
				for (auto* asset : results)
				{
					ImGui::PushID(asset->assetID_);

					ImTextureID icon = GetAssetIcon(*asset);
					ImGui::Image(icon, ImVec2(16.0f, 16.0f));
					ImGui::SameLine();
					ImGui::Selectable(asset->path_.c_str(), false, ImGuiSelectableFlags_SpanAvailWidth | ImGuiSelectableFlags_AllowDoubleClick);

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
						Actor* dropped = *static_cast<Actor* const*>(payload->Data);
						std::filesystem::path directory = ResolveFullPath(selectedDirectory_);
						std::filesystem::path savedPath = Prefab::SaveToDirectory(dropped, directory);
						if (!savedPath.empty())
						{
							context_.worldContext_.resource_->Reload(*context_.worldContext_.loader_, context_.graphicsContext_.device_, context_.graphicsContext_.cmdQueue_, *context_.graphicsContext_.bc7Shader_);
							needsRebuild_ = true;

							std::string relative = std::filesystem::relative(savedPath, context_.worldContext_.resource_->ProjectRootPath()).string();
							std::replace(relative.begin(), relative.end(), '\\', '/');
							Uint32 newAssetID = context_.worldContext_.resource_->GetAssetID(String(relative));
							if (newAssetID != 0)
							{
								dropped->SetSourcePrefabAssetID(newAssetID);
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
			"AIEngine", "AudioEngine", "CompiledShaderObject",
			"Editor", "External", "FoundationEngine", "GraphicsEngine",
			"Package", "PhysicsEngine", "Runtime", "SeedCore", "Tools",
			".vs", "x64", ".git",
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
			std::replace(relativePath.begin(), relativePath.end(), '\\', '/');

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
		for (const auto& asset : allAssets | std::ranges::views::values)
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

		for (const auto* asset : target->assets)
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

			if (ImGui::BeginDragDropSource())
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
		Float cellWidth = gridIconSize_ + ImGui::GetStyle().ItemSpacing.x;
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

		for (const auto* asset : target->assets)
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

			if (ImGui::BeginDragDropSource())
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

	ImTextureID ContentsDrawerPanel::GetAssetIcon(const Asset& asset)const
	{
		if (asset.type_ == AssetType::Texture && asset.isLoaded_)
		{
			if (thumbnailCache_.contains(asset.assetID_))
			{
				return thumbnailCache_.at(asset.assetID_);
			}

			ImageResource* imageResource = context_.worldContext_.resource_->GetImageResource();
			Handle<Texture> handle = imageResource->GetHandle(asset.assetID_);
			Texture* texture = imageResource->Resolve(*context_.worldContext_.loader_, context_.graphicsContext_.bindlessHeap_, handle, context_.uiFrame_);
			if (texture && texture->resource_)
			{
				texture->pinned_ = true;

				/// [EN] Shader-visible heaps are CPU write-only, so they cannot be a
				///      CopyDescriptorsSimple source. Create the SRV directly into the
				///      ImGui heap from the texture resource instead (null desc =
				///      default view covering the whole resource).
				/// [JP] shader-visible ヒープは CPU 書き込み専用のため CopyDescriptorsSimple の
				///      コピー元にできない。代わりにテクスチャリソースから ImGui ヒープへ
				///      SRV を直接作成する（desc null = リソース全体のデフォルトビュー）。
				Uint descIndex = context_.graphicsContext_.descHeap_->AllocateIndex();
				D3D12_CPU_DESCRIPTOR_HANDLE dest = context_.graphicsContext_.descHeap_->CPUHandle(descIndex);
				context_.graphicsContext_.device_->CreateShaderResourceView(texture->resource_.Get(), nullptr, dest);
				ImTextureID textureID = static_cast<ImTextureID>(context_.graphicsContext_.descHeap_->GPUHandle(descIndex).ptr);
				thumbnailCache_.insert({ asset.assetID_, textureID });
				return textureID;
			}
		}

		std::string ext = std::filesystem::path(asset.path_.c_str()).extension().string();
		if (ext == ".h")
		{
			return imguiTexture_.Icon(IconType::Text);
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

	void ContentsDrawerPanel::DrawAssetTooltip(const Asset& asset)
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
			ImGui::Image(icon, ImVec2(16.0f, 16.0f));
			ImGui::SameLine();
		}

		ImGui::Text("%s", asset.path_.c_str());
		ImGui::Text("ID: %u", asset.assetID_);

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
			ImageResource* imageResource = context_.worldContext_.resource_->GetImageResource();
			Handle<Texture> handle = imageResource->GetHandle(asset.assetID_);
			Texture* texture = imageResource->Resolve(*context_.worldContext_.loader_, context_.graphicsContext_.bindlessHeap_, handle, context_.uiFrame_);
			if (texture && texture->resource_)
			{
				D3D12_RESOURCE_DESC desc = texture->resource_->GetDesc();
				ImGui::Text("%llu x %u", desc.Width, desc.Height);
			}
		}

		ImGui::EndTooltip();
	}

	void ContentsDrawerPanel::OpenAssetExternal(const Asset& asset)
	{
		if (asset.type_ == AssetType::Scene)
		{
			context_.sceneContext_.requestedSceneAssetID_ = asset.assetID_;
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

	void ContentsDrawerPanel::DrawAssetContextMenu(const Asset& asset)
	{
		if (ImGui::BeginPopupContextItem("##AssetContext"))
		{
			if (ImGui::MenuItem("開く"))
			{
				OpenAssetExternal(asset);
				ImGui::CloseCurrentPopup();
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
		for (Char character : requestedName)
		{
			if (std::isalnum(static_cast<unsigned char>(character)) || character == '_')
			{
				sanitized.push_back(character);
			}
		}
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
