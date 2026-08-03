#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/Resource/AxisConvention.h>

namespace SeedCore
{
	struct EditorContext;

	class ModelTransformPanel
	{
	public:
		ModelTransformPanel(EditorContext& context);

		void Draw();

		void Open();

		void SetPreviewHandle(D3D12_GPU_DESCRIPTOR_HANDLE previewHandle);

	private:
		void DrawPreview();

		void DrawAxisInspector(Uint32 assetId);

		void ApplyConversion(Uint32 assetId);

	private:
		EditorContext& context_;

		Bool show_ = false;

		Uint32 targetMeshAssetId_ = 0;

		/// [EN] Editable copy of the target asset's axis convention, shown in the inspector until "適用" is pressed.
		/// [JP] 対象アセットの軸コンベンションの編集用コピー。「適用」を押すまでインスペクターに表示される。
		AxisConvention editConvention_;

		D3D12_GPU_DESCRIPTOR_HANDLE previewHandle_{};
	};
}
