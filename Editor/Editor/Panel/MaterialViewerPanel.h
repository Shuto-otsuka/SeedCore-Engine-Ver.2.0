#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	struct EditorContext;
	class Material;
	struct Surface;

	/**
	* [EN]
	* Material Viewer: a 3D preview of one of the selected actor's material
	* slots on that actor's mesh, with an orbit camera and a slot combo. The
	* Surface parameter editor (shading model, KHR, save) is drawn into the
	* shared Inspector window via DrawDetails() while this panel is focused -
	* same pattern as TimelinePanel/AnimatorControllerPanel.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* マテリアルビューア: 選択アクターのマテリアルスロット1つを、その
	* アクターのメッシュ上でオービットカメラ + スロットコンボ付き3D
	* プレビューする。Surface パラメータエディタ(シェーディングモデル/
	* KHR/保存)は、このパネルがフォーカス中の間 DrawDetails() で共有
	* Inspector ウィンドウへ描画する - TimelinePanel/AnimatorControllerPanel
	* と同じ方式。
	*/
	class MaterialViewerPanel
	{
	public:
		MaterialViewerPanel(EditorContext& context);
		~MaterialViewerPanel();

		void Draw();

		void Open();

		void SetPreviewHandle(D3D12_GPU_DESCRIPTOR_HANDLE previewHandle);

		void DrawDetails();

		[[nodiscard]] Bool IsFocused()const;

	private:
		void EnsureEditingSurface();

		EditorContext& context_;

		Bool show_ = false;
		Bool isFocused_ = false;
		Material* target_ = nullptr;
		Size selectedSlot_ = 0;

		ResourcePtr<Surface> editingSurface_;
		Uint32 editingSurfaceAssetId_ = 0;
		std::string surfaceNewNameBuffer_;

		D3D12_GPU_DESCRIPTOR_HANDLE previewHandle_{};
	};
}
