#pragma once
#include <FoundationEngine/Prelude.h>
#include <GraphicsEngine/Avatar/Human/HumanCharacterModel.h>
#include <GraphicsEngine/Avatar/Human/HumanCharacterEvaluator.h>

namespace SeedCore
{
	struct EditorContext;
	class AvatarMesh;

	class AvatarPanel
	{
	public:
		AvatarPanel(EditorContext& context);
		~AvatarPanel();

		void Draw();

		void DrawDetails();

		void Open();

		void SetPreviewHandle(D3D12_GPU_DESCRIPTOR_HANDLE previewHandle);

		[[nodiscard]] Bool IsFocused()const;

	private:
		void EnsureLoaded();

		void Bake(Bool binary);

		EditorContext& context_;

		Bool show_ = false;
		Bool isFocused_ = false;
		Bool loadAttempted_ = false;

		HumanCharacterModel model_;
		HumanCharacterEvaluator evaluator_;
		ResourcePtr<AvatarMesh> mesh_;

		D3D12_GPU_DESCRIPTOR_HANDLE previewHandle_{};
	};
}
