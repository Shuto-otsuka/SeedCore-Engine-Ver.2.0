#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/ECS/EcsID.h>
#include <FoundationEngine/ECS/Entity.h>
#include <FoundationEngine/ECS/ReflectionRegistry.h>
#include <FoundationEngine/ECS/PayloadRegistry.h>
#include <Editor/Editor/Panel/AddComponentPanel.h>

namespace SeedCore
{
	struct EditorContext;
	class ImGuiTexture;
	class Actor;

	class InspectorPanel
	{
	public:
		InspectorPanel(EditorContext& context, ImGuiTexture& imguiTexture);
		~InspectorPanel() = default;

		void Draw();

	private:
		void DrawName(Actor* actor);

		void DrawTags(Actor* actor);

		void DrawLayer(Actor* actor);

		void DrawPrefabControls(Actor* actor);

		void DrawComponents(Actor* actor);

		/// [EN] Shared header/popup/fields block for one component - see the
		///      .cpp definition for why it exists (archetype-layout and
		///      sparse-set components are discovered two different ways but
		///      drawn identically). Returns true if the component was removed
		///      this frame.
		/// [JP] 1コンポーネントぶんのヘッダー/ポップアップ/フィールドの共有
		///      ブロック。存在理由は .cpp の定義コメント参照(アーキタイプ一覧と
		///      スパースセットは見つけ方が違うが描画は同一)。このフレームで
		///      削除されたら true。
		Bool DrawComponentEntry(Actor* actor, ComponentID componentID, const String& componentName, void* componentData);

		void DrawReflectedFields(String componentName, void* componentData, ComponentID componentID, Entity entity);

		/// [EN] baseOffset is baseData's own byte offset (0 at the top level) from the start of the component named by componentID - used to build the undo Command for each field drawn, unless that field's own FieldInfo::directPtr_ is set (not part of the component's fixed-offset POD layout, e.g. a DynamicArray element), in which case its pointer is used directly instead.
		/// [JP] baseOffsetは、componentIDで指されるコンポーネント先頭からの、baseData自身のバイトオフセット(トップレベルでは0) - 描画する各フィールドのundo Commandを組み立てるために使う。ただしそのフィールド自身のFieldInfo::directPtr_が設定されている場合(コンポーネントの固定オフセットPODレイアウトの一部でない場合。例: DynamicArrayの要素)は、代わりにそのポインタを直接使う。
		void DrawFieldList(DynamicArray<FieldInfo>& fields, void* baseData, Entity entity, ComponentID componentID, Size baseOffset);

		void DrawField(const FieldInfo& field, void* pointer, Entity entity, ComponentID componentID, Size fieldOffset);

		void DrawPayloadField(const FieldInfo& field, void* pointer, Entity entity, ComponentID componentID, Size fieldOffset);

		void DrawPayloadArrayRow(const FieldInfo& field, void* pointer);

		void DrawPayloadArrayAppendSlot(const FieldInfo& field, const DynamicArray<Int>& existingValues);

		const Char* GetPayloadDropType(PayloadAssetType assetType)const;

		/// [EN] Maps a component's registered name to its Inspector header
		///      icon (Unity-style). Falls back to IconType::ComponentCustom
		///      for anything not explicitly listed — covers UserProject
		///      scripts and any built-in component without a dedicated icon.
		/// [JP] コンポーネントの登録名を Inspector ヘッダー用アイコン
		///      （Unity 風）へ対応付ける。明示的に列挙されていないものは
		///      IconType::ComponentCustom にフォールバックする —
		///      UserProject のスクリプトや、専用アイコンを持たない組み込み
		///      コンポーネントをカバーする。
		[[nodiscard]] ImTextureID GetComponentIcon(const String& componentName)const;

		void DrawTransform(Float* data, const Char* label, Bool& linked, Float* previousValues, Entity entity, ComponentID componentID);

	private:
		EditorContext& context_;

		AddComponentPanel addComponentPanel_;

		Bool positionLinked_ = false;
		Bool rotationLinked_ = false;
		Bool scaleLinked_ = false;

		Float previousPosition_[3] = {};
		Float previousRotation_[3] = {};
		Float previousScale_[3] = {};

		Bool locked_ = false;
		Actor* lockedActor_ = nullptr;

		std::string newTagBuffer_;

		DynamicArray<std::string> layerNameBuffers_;

		ImGuiTexture& imguiTexture_;

		/// [EN] Value of the field currently being dragged/typed into, captured on ImGui::IsItemActivated() and diffed against the field's value on ImGui::IsItemDeactivatedAfterEdit() to build an undo Command. Only one of these is meaningful at a time, since ImGui allows at most one active item.
		/// [JP] 現在ドラッグ/入力中のフィールドの値。ImGui::IsItemActivated()時点で捕捉し、ImGui::IsItemDeactivatedAfterEdit()時点のフィールド値と比較してundo Commandを組み立てる。ImGuiのアクティブアイテムは常に高々1つのため、これらのうち意味を持つのは同時に1つだけ。
		Int pendingOldInt_ = 0;
		Float pendingOldFloat_ = 0.0f;
		Vector2 pendingOldVector2_ = Vector2::Zero;
		Vector3 pendingOldVector3_ = Vector3::Zero;
		Color pendingOldColor_ = Color(0.0f, 0.0f, 0.0f, 0.0f);
		String pendingOldString_;
	};
}
