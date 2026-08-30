#include <FoundationEngine/Resource/ActorSerialization.h>
#include <FoundationEngine/Resource/Prefab.h>
#include <FoundationEngine/Resource/ResourceCache.h>
#include <FoundationEngine/ECS/Actor.h>
#include <FoundationEngine/ECS/World.h>
#include <FoundationEngine/ECS/Component.h>
#include <FoundationEngine/ECS/ComponentRegistry.h>
#include <FoundationEngine/ECS/PayloadRegistry.h>
#include <FoundationEngine/ECS/Component/Name.h>
#include <FoundationEngine/ECS/Component/Position.h>
#include <FoundationEngine/ECS/Component/Rotation.h>
#include <FoundationEngine/ECS/Component/Scale.h>

namespace SeedCore
{
	namespace
	{
		/**
		* [EN]
		* Returns whether name is one of the built-in components
		* (Name/Position/Rotation/Scale/Velocity/Active) that every actor
		* gets automatically and that are captured as dedicated
		* SerializedActorNode fields instead of as a generic SerializedComponent.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* name が、全ての actor に自動的に付与され、汎用的な
		* SerializedComponent としてではなく専用の SerializedActorNode
		* フィールドとして取得される組み込みコンポーネント
		* （Name/Position/Rotation/Scale/Velocity/Active）のいずれかで
		* あるかどうかを返す。
		*/
		Bool IsBuiltinComponent(const String& name)
		{
			static const String builtin[] =
			{
				String("Name"),
				String("Position"),
				String("Rotation"),
				String("Scale"),
				String("Velocity"),
				String("Active"),
			};

			return std::ranges::contains(builtin, name);
		}

		/**
		* [EN]
		* Fills fields with typeName's reflected fields (from
		* ReflectionRegistry) and, if it's a payload type, its
		* asset-reference fields too (from PayloadRegistry).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* typeName のリフレクションされたフィールド（ReflectionRegistry
		* から）を fields へ書き込む。それがペイロード型であれば、その
		* アセット参照フィールド（PayloadRegistry から）も書き込む。
		*/
		void CollectFieldInfos(const String& typeName, void* data, DynamicArray<FieldInfo>& fields)
		{
			auto& reflectionRegistry = ReflectionRegistry::GetRegistry();
			auto reflectionIt = reflectionRegistry.find(typeName);
			if (reflectionIt != reflectionRegistry.end())
			{
				reflectionIt->second(data, fields);
			}

			auto& payloadRegistry = PayloadRegistry::GetRegistry();
			auto payloadIt = payloadRegistry.find(typeName);
			if (payloadIt != payloadRegistry.end())
			{
				payloadIt->second(data, fields);
			}
		}

		/**
		* [EN]
		* Captures a single non-array, non-nested field's current value
		* from ptr into a new SerializedField, based on field's AttributeType.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* field の AttributeType に基づき、ptr から単一の非配列・
		* 非ネストフィールドの現在の値を、新しい SerializedField へ
		* 取得する。
		*/
		SerializedField CaptureScalarField(const FieldInfo& field, void* ptr)
		{
			SerializedField capturedField;
			capturedField.name_ = field.name_;
			capturedField.type_ = field.type_;

			switch (field.type_)
			{
			case AttributeType::Int:
			case AttributeType::Enum:
				capturedField.intValue_ = *static_cast<Int*>(ptr);
				break;
			case AttributeType::Float:
				capturedField.floatValue_ = *static_cast<Float*>(ptr);
				break;
			case AttributeType::Bool:
				capturedField.boolValue_ = *static_cast<Bool*>(ptr);
				break;
			case AttributeType::Vector2:
				capturedField.vector2Value_ = *static_cast<Vector2*>(ptr);
				break;
			case AttributeType::Vector3:
				capturedField.vector3Value_ = *static_cast<Vector3*>(ptr);
				break;
			case AttributeType::String:
				capturedField.stringValue_ = *static_cast<String*>(ptr);
				break;
			case AttributeType::Color:
				capturedField.colorValue_ = *static_cast<Color*>(ptr);
				break;
			default:
				break;
			}

			return capturedField;
		}

		/**
		* [EN]
		* Writes match's saved value into ptr, based on field's
		* AttributeType (the restore-side counterpart of CaptureScalarField).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* field の AttributeType に基づき、match の保存済みの値を ptr へ
		* 書き込む（CaptureScalarField の復元側に対応する処理）。
		*/
		void ApplyScalarField(const FieldInfo& field, void* ptr, const SerializedField& match)
		{
			/// [EN] Refuse a saved value whose type no longer matches the
			///      field's. SerializedField keeps every value kind in its own
			///      member and records which one was written, so switching on
			///      the CURRENT type alone reads whichever member happens to
			///      sit there - which for a field whose type changed is the
			///      one nothing was ever written to. A Color field later
			///      redeclared as Float therefore loaded as 0 rather than
			///      keeping its default, and a 0 landing in something used as
			///      an exponent (a grading gamma, say) is enough to black out
			///      the frame. Leaving the field alone lets its default stand,
			///      which is the only sane answer when the saved data no
			///      longer describes it.
			/// [JP] 保存時の型が現在のフィールドの型と一致しない値は適用
			///      しない。SerializedField は値の種類ごとに別のメンバを
			///      持ち、どれに書いたかを記録しているので、現在の型だけで
			///      分岐するとたまたまそこにあるメンバを読むことになる —
			///      型が変わったフィールドでは、それは【一度も書かれて
			///      いない】メンバ。Color だったフィールドを後から Float に
			///      すると既定値を保つのではなく0が入り、その0が指数として
			///      使われる値(グレーディングのガンマなど)だと画面全体が
			///      黒くなるには十分だった。触らずに置けば既定値が残る。
			///      保存データがそのフィールドを説明しなくなった以上、
			///      それが唯一まともな答え。
			if (match.type_ != field.type_)
			{
				return;
			}

			switch (field.type_)
			{
			case AttributeType::Int:
			case AttributeType::Enum:
				*static_cast<Int*>(ptr) = match.intValue_;
				break;
			case AttributeType::Float:
				*static_cast<Float*>(ptr) = match.floatValue_;
				break;
			case AttributeType::Bool:
				*static_cast<Bool*>(ptr) = match.boolValue_;
				break;
			case AttributeType::Vector2:
				*static_cast<Vector2*>(ptr) = match.vector2Value_;
				break;
			case AttributeType::Vector3:
				*static_cast<Vector3*>(ptr) = match.vector3Value_;
				break;
			case AttributeType::String:
				*static_cast<String*>(ptr) = match.stringValue_;
				break;
			case AttributeType::Color:
				*static_cast<Color*>(ptr) = match.colorValue_;
				break;
			default:
				break;
			}
		}

		/**
		* [EN]
		* Recursively captures every reflected field of the type named
		* typeName (rooted at data) into outFields: array fields become
		* one SerializedField per element, nested-struct fields recurse
		* via their own CaptureFields call, and everything else is
		* captured as a plain scalar.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* typeName という名前の型（data を起点とする）の、リフレクション
		* された全フィールドを outFields へ再帰的に取得する: 配列
		* フィールドは要素ごとに1つの SerializedField になり、ネストされた
		* 構造体フィールドは自身の CaptureFields 呼び出しを介して再帰し、
		* それ以外は単純なスカラーとして取得される。
		*/
		void CaptureFields(const String& typeName, void* data, DynamicArray<SerializedField>& outFields)
		{
			DynamicArray<FieldInfo> fields;
			CollectFieldInfos(typeName, data, fields);

			for (Size index = 0; index < fields.size(); ++index)
			{
				const FieldInfo& field = fields[index];

				if (field.array_.size_ > 0 || field.array_.add_)
				{
					/// [EN] An array field: its elements are laid out as the following N FieldInfo entries in the flat list, so consume and skip them here.
					/// [JP] 配列フィールド: その要素はフラットなリスト内で続く N 個の FieldInfo エントリとして並んでいる。ここでそれらを消費し、スキップする。
					Size count = field.array_.size_;

					SerializedField arrayField;
					arrayField.name_ = field.name_;
					arrayField.isArray_ = true;

					for (Size element = 0; element < count && (index + 1 + element) < fields.size(); ++element)
					{
						const FieldInfo& elementField = fields[index + 1 + element];
						void* ptr = elementField.directPtr_ ? elementField.directPtr_ : (static_cast<Uint8*>(data) + elementField.offset_);

						if (!elementField.nestedTypeName_.view().empty())
						{
							SerializedField structField;
							CaptureFields(elementField.nestedTypeName_, ptr, structField.children_);
							arrayField.children_.push_back(std::move(structField));
						}
						else
						{
							arrayField.children_.push_back(CaptureScalarField(elementField, ptr));
						}
					}

					outFields.push_back(std::move(arrayField));
					index += count;
					continue;
				}

				void* ptr = field.directPtr_ ? field.directPtr_ : (static_cast<Uint8*>(data) + field.offset_);

				if (!field.nestedTypeName_.view().empty())
				{
					/// [EN] Nested struct field: recurse, capturing its own fields as children rather than treating it as a scalar.
					/// [JP] ネストされた構造体フィールド: スカラーとして扱う代わりに、その自身のフィールドを子として再帰的に取得する。
					SerializedField structField;
					structField.name_ = field.name_;
					CaptureFields(field.nestedTypeName_, ptr, structField.children_);
					outFields.push_back(std::move(structField));
					continue;
				}

				outFields.push_back(CaptureScalarField(field, ptr));
			}
		}

		/**
		* [EN]
		* Recursively restores every reflected field of the type named
		* typeName (rooted at data) from savedFields, matching by field
		* name. Runs a first pass that grows every saved array field to
		* its saved element count before any per-element pointer is
		* read, then a second pass that actually applies each field's value.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* typeName という名前の型（data を起点とする）の、リフレクション
		* された全フィールドを savedFields から復元する。フィールド名で
		* 対応付ける。要素ごとのポインタを読む前に、保存済みの各配列
		* フィールドをその保存済み要素数まで伸ばす第1パスを実行し、その後
		* 実際に各フィールドの値を適用する第2パスを実行する。
		*/
		void ApplyFields(const String& typeName, void* data, const DynamicArray<SerializedField>& savedFields)
		{
			if (savedFields.empty())
			{
				return;
			}

			/// [EN] First pass: grow every saved array field to its saved
			///      element count. Must finish growing before reading any
			///      per-element pointer — push_back can reallocate and
			///      invalidate previously fetched directPtr_ values.
			/// [JP] 第1パス: 保存済みの配列フィールドを、保存されていた要素数
			///      まで先に伸ばす。要素ポインタを読む前に伸ばし切ること —
			///      push_backは再確保を起こし、それ以前に取得したdirectPtr_を
			///      無効化しうる。
			{
				DynamicArray<FieldInfo> fields;
				CollectFieldInfos(typeName, data, fields);

				for (const FieldInfo& field : fields)
				{
					if (!field.array_.add_)
					{
						continue;
					}

					auto matchIt = std::ranges::find(savedFields, field.name_, &SerializedField::name_);
					const SerializedField* match = matchIt != savedFields.end() ? &*matchIt : nullptr;
					if (!match)
					{
						continue;
					}

					Size currentCount = field.array_.size_;
					while (currentCount < match->children_.size())
					{
						field.array_.add_();
						++currentCount;
					}
				}
			}

			/// [EN] Second pass: re-collect field infos (now with grown arrays and therefore stable pointers) and actually apply each saved value.
			/// [JP] 第2パス: フィールド情報を再収集し（配列は伸ばされ済みで、ポインタは安定している）、実際に各保存済みの値を適用する。
			DynamicArray<FieldInfo> fields;
			CollectFieldInfos(typeName, data, fields);

			for (Size index = 0; index < fields.size(); ++index)
			{
				const FieldInfo& field = fields[index];

				auto matchIt = std::ranges::find(savedFields, field.name_, &SerializedField::name_);
				const SerializedField* match = matchIt != savedFields.end() ? &*matchIt : nullptr;

				if (field.array_.size_ > 0 || field.array_.add_)
				{
					Size count = field.array_.size_;

					if (match)
					{
						for (Size element = 0; element < count && element < match->children_.size() && (index + 1 + element) < fields.size(); ++element)
						{
							const FieldInfo& elementField = fields[index + 1 + element];
							void* ptr = elementField.directPtr_ ? elementField.directPtr_ : (static_cast<Uint8*>(data) + elementField.offset_);
							const SerializedField& savedElement = match->children_[element];

							if (!elementField.nestedTypeName_.view().empty())
							{
								ApplyFields(elementField.nestedTypeName_, ptr, savedElement.children_);
							}
							else
							{
								ApplyScalarField(elementField, ptr, savedElement);
							}
						}
					}

					index += count;
					continue;
				}

				if (!match)
				{
					continue;
				}

				void* ptr = field.directPtr_ ? field.directPtr_ : (static_cast<Uint8*>(data) + field.offset_);

				if (!field.nestedTypeName_.view().empty())
				{
					ApplyFields(field.nestedTypeName_, ptr, match->children_);
					continue;
				}

				ApplyScalarField(field, ptr, *match);
			}
		}
	}

	/**
	* [EN]
	* Recursively captures actor and every descendant into outNodes as
	* a flat, parent-index-linked array (used by both Scene::Capture and
	* Prefab::Capture). Returns actor's own index within outNodes.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* actor とその全子孫を、フラットな親インデックス連結配列として
	* outNodes へ再帰的に取得する（Scene::Capture と Prefab::Capture の
	* 両方から使われる）。actor 自身の outNodes 内でのインデックスを返す。
	*/
	Int CaptureActorNode(Actor* actor, Int parentIndex, DynamicArray<SerializedActorNode>& outNodes)
	{
		SerializedActorNode node;

		const Name* nameComponent = actor->GetComponent<Name>();
		node.name_ = nameComponent ? nameComponent->name_ : String("Actor");

		node.tags_ = actor->GetTagList();
		node.layerName_ = actor->GetLayerName();
		node.active_ = actor->IsActive();
		node.persistentId_ = actor->GetPersistentID();

		const Position* position = actor->GetComponent<Position>();
		if (position)
		{
			node.position_ = Vector3(position->x_, position->y_, position->z_);
		}

		const Rotation* rotation = actor->GetComponent<Rotation>();
		if (rotation)
		{
			node.rotation_ = Vector3(rotation->x_, rotation->y_, rotation->z_);
		}

		const Scale* scale = actor->GetComponent<Scale>();
		if (scale)
		{
			node.scale_ = Vector3(scale->x_, scale->y_, scale->z_);
		}

		node.parentIndex_ = parentIndex;

		/// [EN] A non-root actor whose own subtree came from a prefab: record only the prefab reference, not its individual components, so re-instantiating stays in sync with the source prefab.
		/// [JP] 自身のサブツリーがプレハブ由来である、非ルート actor: 個々のコンポーネントではなく、プレハブへの参照のみを記録する。これにより、再インスタンス化が元のプレハブと同期した状態を保つ。
		Bool isNestedInstance = (parentIndex != -1) && (actor->GetSourcePrefabAssetID() != 0);

		if (isNestedInstance)
		{
			node.nestedPrefabAssetID_ = actor->GetSourcePrefabAssetID();
		}
		else
		{
			World& world = actor->GetWorld();
			Entity entity = actor->GetEntity();

			/// [EN] Capture every archetype-stored component except the built-in transform/lifecycle ones (those are captured as dedicated node fields above).
			/// [JP] 組み込みのトランスフォーム/ライフサイクルコンポーネントを除く、全アーキタイプ格納コンポーネントを取得する（それらは上記で専用のノードフィールドとして取得済み）。
			const DynamicArray<ComponentID>& layout = world.GetLayout(entity);
			for (ComponentID id : layout)
			{
				String name = ComponentRegistry::GetName(id);
				if (IsBuiltinComponent(name))
				{
					continue;
				}

				SerializedComponent serializedComponent;
				serializedComponent.componentName_ = name;

				void* componentData = world.GetComponent(entity, id);
				if (componentData)
				{
					CaptureFields(name, componentData, serializedComponent.fields_);
				}

				node.components_.push_back(std::move(serializedComponent));
			}

			/// [EN] Also capture every ComponentBase-derived (sparse-set-stored) component the actor holds.
			/// [JP] actor が保持する、全 ComponentBase 派生（スパースセット格納）コンポーネントも取得する。
			for (ComponentID id : actor->ComponentBaseIDList())
			{
				String name = ComponentRegistry::GetName(id);

				SerializedComponent serializedComponent;
				serializedComponent.componentName_ = name;

				void* componentData = world.GetComponent(entity, id);
				if (componentData)
				{
					CaptureFields(name, componentData, serializedComponent.fields_);
				}

				node.components_.push_back(std::move(serializedComponent));
			}

			/// [EN] Also capture every plain-struct component (NOT ComponentBase-
			///      derived, e.g. PostProcess) registered with
			///      ComponentStorage::SparseSet - these are a third category the
			///      two loops above both miss: GetLayout() above only returns
			///      the entity's ARCHETYPE component list (World.cpp:
			///      "it->second.archetype_->Layout()"), and ComponentBaseIDList()
			///      above only tracks ComponentBase-derived (SeedScript) types.
			///      A plain struct registered SparseSet falls through both, so
			///      without this loop it is silently never written to the scene/
			///      prefab file at all (same gap InspectorPanel::DrawComponents
			///      had for display - see its fix for the identical reasoning).
			/// [JP] ComponentBase 派生でない、ComponentStorage::SparseSet で
			///      登録された素の struct コンポーネント(例: PostProcess)も
			///      取得する — これは上の2つのループがどちらも取りこぼす
			///      第3のカテゴリ: 上の GetLayout() はエンティティの
			///      【アーキタイプ】のコンポーネント一覧しか返さず(World.cpp:
			///      "it->second.archetype_->Layout()")、上の
			///      ComponentBaseIDList() は ComponentBase 派生(SeedScript)型
			///      しか追跡しない。SparseSet 登録の素の struct はそのどちらの
			///      網にも掛からないため、このループが無いと黙って一度も
			///      シーン/プレハブファイルへ書かれない(表示側で
			///      InspectorPanel::DrawComponents が抱えていたのと同じ穴 —
			///      理由も同一なのでそちらの修正も参照)。
			for (const auto& [id, metadata] : ComponentRegistry::GetRegistry())
			{
				if (metadata.storage_ != ComponentStorage::SparseSet || metadata.isComponentBase_)
				{
					continue;
				}

				void* componentData = world.GetComponent(entity, id);
				if (!componentData)
				{
					continue;
				}

				String name = ComponentRegistry::GetName(id);

				SerializedComponent serializedComponent;
				serializedComponent.componentName_ = name;
				CaptureFields(name, componentData, serializedComponent.fields_);

				node.components_.push_back(std::move(serializedComponent));
			}
		}

		Int myIndex = static_cast<Int>(outNodes.size());
		outNodes.push_back(std::move(node));

		if (!isNestedInstance)
		{
			for (Actor* child : actor->GetChildren())
			{
				CaptureActorNode(child, myIndex, outNodes);
			}
		}

		return myIndex;
	}

	/**
	* [EN]
	* Recreates a single captured node as a live Actor in world (used by
	* both Scene::Instantiate and Prefab::Instantiate): either by
	* instantiating a referenced nested prefab, or by creating a fresh
	* actor and restoring its component fields. Applies the node's
	* transform/active/tags and reparents under parentActor. Returns the
	* new actor, or nullptr on failure.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 取得済みの単一ノードを world 内の生きた Actor として再生成する
	* （Scene::Instantiate と Prefab::Instantiate の両方から使われる）:
	* 参照されているネストされたプレハブをインスタンス化するか、新しい
	* actor を生成してそのコンポーネントフィールドを復元する。ノードの
	* トランスフォーム/アクティブ状態/タグを適用し、parentActor の下へ
	* 再親化する。新しい actor を返す。失敗時は nullptr を返す。
	*/
	Actor* InstantiateActorNode(World& world, ResourceCache& cache, const SerializedActorNode& node, Actor* parentActor, Bool markAsPrefabInstance)
	{
		Actor* actor = nullptr;

		if (node.nestedPrefabAssetID_ != 0)
		{
			/// [EN] This node is a nested prefab reference: load and instantiate the referenced prefab instead of building an actor from components_.
			/// [JP] このノードはネストされたプレハブへの参照である: components_ から actor を構築する代わりに、参照されているプレハブを読み込んでインスタンス化する。
			Handle<Prefab> nestedHandle = cache.GetPrefabPool().Load(node.nestedPrefabAssetID_, cache);
			Prefab* nestedPrefab = cache.GetPrefabPool().Get(nestedHandle);
			if (nestedPrefab)
			{
				actor = nestedPrefab->Instantiate(world, cache, parentActor, node.nestedPrefabAssetID_);
			}

			if (!actor)
			{
				return nullptr;
			}

			/// [EN] The outer scene/prefab may have renamed this instance; apply that name on top of whatever the nested prefab itself set.
			/// [JP] 外側のシーン/プレハブがこのインスタンスをリネームしている場合がある。ネストされたプレハブ自体が設定した名前の上から、その名前を適用する。
			Name* nameComponent = const_cast<Name*>(actor->GetComponent<Name>());
			if (nameComponent)
			{
				nameComponent->name_ = node.name_;
			}
		}
		else
		{
			actor = world.CreateActor(node.name_, node.persistentId_);
			actor->SetPrefabInstance(markAsPrefabInstance);

			/// [EN] Recreate every captured component and restore its field values.
			/// [JP] 取得済みの各コンポーネントを再生成し、そのフィールド値を復元する。
			for (const SerializedComponent& component : node.components_)
			{
				ComponentID id = ComponentRegistry::GetComponentID(component.componentName_);
				if (!id)
				{
					continue;
				}

				actor->AddComponent(id);

				void* componentData = world.GetComponent(actor->GetEntity(), id);
				if (componentData)
				{
					ApplyFields(component.componentName_, componentData, component.fields_);
				}
			}
		}

		/// [EN] Apply the captured transform on top of whatever the actor ended up with (freshly created or nested-prefab-instantiated).
		/// [JP] 取得済みのトランスフォームを、actor が最終的に持つことになった状態（新規生成、またはネストされたプレハブからのインスタンス化）の上から適用する。
		Position* position = const_cast<Position*>(actor->GetComponent<Position>());
		if (position)
		{
			position->x_ = node.position_.x;
			position->y_ = node.position_.y;
			position->z_ = node.position_.z;
		}

		Rotation* rotation = const_cast<Rotation*>(actor->GetComponent<Rotation>());
		if (rotation)
		{
			rotation->x_ = node.rotation_.x;
			rotation->y_ = node.rotation_.y;
			rotation->z_ = node.rotation_.z;
		}

		Scale* scale = const_cast<Scale*>(actor->GetComponent<Scale>());
		if (scale)
		{
			scale->x_ = node.scale_.x;
			scale->y_ = node.scale_.y;
			scale->z_ = node.scale_.z;
		}

		actor->SetActive(node.active_);

		for (const String& tag : node.tags_)
		{
			actor->AddTag(tag);
		}

		actor->SetLayer(node.layerName_);

		if (parentActor)
		{
			actor->SetParent(parentActor);
		}

		return actor;
	}

	SerializedComponent CaptureComponent(const String& componentName, void* componentData)
	{
		SerializedComponent serializedComponent{};
		serializedComponent.componentName_ = componentName;
		CaptureFields(componentName, componentData, serializedComponent.fields_);
		return serializedComponent;
	}

	void ApplyComponent(const SerializedComponent& component, void* componentData)
	{
		ApplyFields(component.componentName_, componentData, component.fields_);
	}
}
