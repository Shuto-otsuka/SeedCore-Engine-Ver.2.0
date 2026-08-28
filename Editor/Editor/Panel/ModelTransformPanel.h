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

		void Open(Uint32 assetId);

		void SetPreviewHandle(D3D12_GPU_DESCRIPTOR_HANDLE previewHandle);

	private:
		void SetTarget(Uint32 assetId);

		void DrawPreview();

		void DrawAxisInspector(Uint32 assetId);

		void ApplyConversion(Uint32 assetId);

		void DrawBaseTransformInspector(Uint32 assetId);

		void ApplyTransformConversion(Uint32 assetId);

	private:
		EditorContext& context_;

		Bool show_ = false;

		Uint32 targetMeshAssetId_ = 0;

		Uint32 lastSelectionMeshId_ = 0;

		/// [EN] Editable copy of the target asset's axis convention, shown in the inspector until "適用" is pressed.
		/// [JP] 対象アセットの軸コンベンションの編集用コピー。「適用」を押すまでインスペクターに表示される。
		AxisConvention editConvention_;

		/// [EN] Base transform delta, baked directly into the asset by
		///      ApplyTransformConversion and reset to identity afterward - unlike
		///      editConvention_ there is no persisted "current" state to diff
		///      against, each 適用 just bakes whatever is currently entered.
		/// [JP] 基礎トランスフォームの差分。ApplyTransformConversion がアセットへ
		///      直接焼き込み、その後は単位量へリセットされる — editConvention_ と
		///      違い比較対象となる永続的な「現在値」が無いため、適用のたびに
		///      入力中の値をそのまま焼き込む。
		Vector3 baseTransformPosition_ = { 0.0f, 0.0f, 0.0f };
		Vector3 baseTransformRotation_ = { 0.0f, 0.0f, 0.0f };
		Vector3 baseTransformScale_ = { 1.0f, 1.0f, 1.0f };
		Vector3 baseTransformPivot_ = { 0.0f, 0.0f, 0.0f };

		/// [EN] Gizmo manipulates position/rotation/scale (an anchor placed at
		///      pivot + position, oriented/scaled by rotation/scale) - not Pivot
		///      itself, which stays numeric-only.
		/// [JP] ギズモは position/rotation/scale を操作する(pivot + position に
		///      置かれ、rotation/scale で向き/大きさが付くアンカー) — Pivot 自体は
		///      数値入力のみ。
		ImGuizmo::OPERATION baseTransformGizmoOperation_ = ImGuizmo::TRANSLATE;

		D3D12_GPU_DESCRIPTOR_HANDLE previewHandle_{};
	};
}
