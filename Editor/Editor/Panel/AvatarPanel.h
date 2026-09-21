#pragma once
#include <FoundationEngine/Prelude.h>
#include <GraphicsEngine/Avatar/Human/HumanCharacterModel.h>
#include <GraphicsEngine/Avatar/Human/HumanCharacterEvaluator.h>
#include <GraphicsEngine/Avatar/Animal/AnimalCharacterModel.h>
#include <GraphicsEngine/Avatar/Animal/AnimalCharacterEvaluator.h>
#include <GraphicsEngine/Texture/TextureLoader.h>

namespace SeedCore
{
	struct EditorContext;
	class AvatarMesh;
	enum class ExportPreset;

	enum class AvatarKind
	{
		Human,
		Animal
	};

	class AvatarPanel
	{
	public:
		AvatarPanel(EditorContext& context);
		~AvatarPanel();

		void Draw();

		void DrawDetails();

		void Open();

		void SetPreviewHandle(D3D12_GPU_DESCRIPTOR_HANDLE previewHandle);

		[[nodiscard]] Bool Focused()const;

	private:
		static constexpr Uint32 regionSlotCount_ = 4;

		void EnsureLoaded();

		void Bake(ExportPreset preset);

		void LoadRegionTexture(Uint32 regionIndex, String filePath);

		void ClearRegionTextures();

		void ExportUvLayout();

		[[nodiscard]] Uint32 ActiveRegionCount()const;

		[[nodiscard]] std::span<const Char* const> ActiveRegionLabels()const;

		EditorContext& context_;

		Bool show_ = false;
		Bool isFocused_ = false;
		Bool cameraReady_ = false;

		AvatarKind kind_ = AvatarKind::Human;

		/// [EN] Group switch requested from the inspector combo; applied at the top of
		///      Draw() so the preview mesh is never freed while the render still holds it.
		/// [JP] インスペクターのコンボから要求されたグループ切替。プレビューメッシュが
		///      描画中に解放されないよう Draw() の先頭で適用する。
		Int pendingAnimalGroup_ = -1;

		Bool humanLoadAttempted_ = false;
		Bool animalLoadAttempted_ = false;

		HumanCharacterModel humanModel_;
		HumanCharacterEvaluator humanEvaluator_;
		AnimalCharacterModel animalModel_;
		AnimalCharacterEvaluator animalEvaluator_;

		ResourcePtr<AvatarMesh> humanMesh_;
		ResourcePtr<AvatarMesh> animalMesh_;

		Microsoft::WRL::ComPtr<ID3D12Resource> regionTextureResources_[regionSlotCount_];
		Uint32 regionTextureIndices_[regionSlotCount_] = { 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF };
		String regionTexturePaths_[regionSlotCount_];

		D3D12_GPU_DESCRIPTOR_HANDLE previewHandle_{};
	};
}
