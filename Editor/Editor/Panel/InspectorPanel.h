#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/ECS/EcsID.h>
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

		void DrawReflectedFields(String componentName, void* componentData, ComponentID componentID);

		void DrawFieldList(DynamicArray<FieldInfo>& fields, void* baseData);

		void DrawField(const FieldInfo& field, void* pointer);

		void DrawPayloadField(const FieldInfo& field, void* pointer);

		void DrawPayloadArrayRow(const FieldInfo& field, void* pointer);

		void DrawPayloadArrayAppendSlot(const FieldInfo& field, const DynamicArray<Int>& existingValues);

		const Char* GetPayloadDropType(PayloadAssetType assetType)const;

		void DrawTransform(Float* data, const Char* label, Bool& linked, Float* previousValues);

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

		ImGuiTexture& imguiTexture_;
	};
}
