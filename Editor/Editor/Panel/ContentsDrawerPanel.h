#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/Resource/ResourceCache.h>

namespace SeedCore
{
	struct EditorContext;
	class ImGuiTexture;

	class ContentsDrawerPanel
	{
	public:
		ContentsDrawerPanel(EditorContext& context, ImGuiTexture& imguiTexture);
		~ContentsDrawerPanel();

		void Draw();

	private:
		enum class ViewMode
		{
			List,
			Grid,
		};

		enum class ClipboardAction
		{
			None,
			Cut,
			Copy,
		};

		struct DirectoryNode
		{
			std::string name;
			std::string fullPath;
			ArtMap<std::string, DirectoryNode> children;
			DynamicArray<const Asset*> assets;
		};

		void BuildDirectoryTree();

		void DrawDirectoryTree(DirectoryNode& node);

		void DrawAssetList();

		void DrawAssetListMode(DirectoryNode* target);

		void DrawAssetGridMode(DirectoryNode* target);

		ImTextureID GetAssetTypeIcon(AssetType type)const;

		ImTextureID GetAssetIcon(const Asset& asset)const;

		const Char* GetDragDropType(AssetType type)const;

		ImTextureID GetFolderIcon(const DirectoryNode& node)const;

		void OpenAssetExternal(const Asset& asset);

		void DrawAssetTooltip(const Asset& asset);

		void DrawFolderContextMenu(const std::string& relativePath, const std::string& folderName);

		void DrawAssetContextMenu(const Asset& asset);

		void DrawBackgroundContextMenu();

		Bool DrawInlineRename(const std::filesystem::path& itemFullPath, const std::string& displayName, Float width = -1.0f);

		std::filesystem::path ResolveFullPath(const std::string& relativePath)const;

		void CreateNewFolder(const std::string& parentRelative);

		void RequestCreateScript(const std::string& parentRelative);

		void DrawCreateScriptPopup();

		void CreateNewScript(const std::string& parentRelative, const std::string& requestedName);

		Bool RegisterScriptInProject(const std::filesystem::path& headerFullPath, const std::filesystem::path& cppFullPath);

		void ExecuteDelete(const std::filesystem::path& fullPath);

		void ExecuteRename(const std::filesystem::path& oldPath, const std::string& newName);

		void ExecutePaste(const std::string& destinationRelative);

		void NavigateTo(const std::string& directory);

	private:
		EditorContext& context_;

		DirectoryNode root_;

		std::string selectedDirectory_;

		std::string searchBuffer_;

		Bool needsRebuild_ = true;

		ViewMode viewMode_ = ViewMode::Grid;

		Float gridIconSize_ = 64.0f;

		ImGuiTexture& imguiTexture_;

		ClipboardAction clipboardAction_ = ClipboardAction::None;
		std::filesystem::path clipboardPath_;
		Bool clipboardIsDirectory_ = false;

		Bool renaming_ = false;
		Bool renameNeedsFocus_ = false;
		std::filesystem::path renameTargetPath_;
		std::string renameBuffer_;

		Bool openCreateScriptPopup_ = false;
		Bool createScriptNeedsFocus_ = false;
		std::string createScriptParentRelative_;
		std::string createScriptNameBuffer_;

		HANDLE directoryWatchHandle_ = INVALID_HANDLE_VALUE;

		DynamicArray<std::string> directoryHistory_;
		Int historyIndex_ = -1;

		mutable FlatMap<Uint32, ImTextureID> thumbnailCache_;
	};
}
