#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	struct EditorContext;
	class Crister;

	class SkeletonControllerPanel
	{
	public:
		SkeletonControllerPanel(EditorContext& context);

		void Draw();

		void Open();

		void SetPreviewHandle(D3D12_GPU_DESCRIPTOR_HANDLE previewHandle);

		void DrawDetails();

		[[nodiscard]] Bool IsFocused()const;

	private:
		void DrawBoneList(const Crister& crister);

		void DrawPreview();

	private:
		static constexpr Float boneTreeColumnWidthRatio_ = 0.2f;

		EditorContext& context_;

		Bool show_ = false;

		Bool isFocused_ = false;

		Int selectedNodeIndex_ = -1;

		Crister* currentCrister_ = nullptr;

		D3D12_GPU_DESCRIPTOR_HANDLE previewHandle_{};
	};
}
