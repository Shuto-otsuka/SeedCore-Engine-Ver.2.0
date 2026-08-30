#include <Editor/Editor/ImGui/ImGuiTexture.h>
#include <Editor/Editor/EditorContext.h>
#include <Editor/Editor/ImGui/ImGuiRenderer.h>
#include <GraphicsEngine/D3D12/Descriptor/DescriptorHeap.h>
#include <GraphicsEngine/D3D12/Context/D3D12CommandQueue.h>
#include <GraphicsEngine/Graphics.h>
#include <FoundationEngine/Resource/LoaderSystem.h>

namespace SeedCore
{
	ImGuiTexture::ImGuiTexture(EditorContext& context)
	{
		TextureLoader* loader = context.worldContext_.loader_->textureLoader_.get();

		D3D12Context* d3d12Context = context.graphicsContext_.graphics_->GetContext();
		DescriptorHeap* descHeap = context.graphicsContext_.imgui_->GetDescriptorHeap();

		auto load = [&](IconType type, const Char* folder, const Char* name)
		{
			Uint index = descHeap->AllocateIndex();

			Microsoft::WRL::ComPtr<ID3D12Resource> resource;
			String filePath = String(std::string("Icon/") + folder + "/" + name + ".icon");
			loader->CreateTexture(d3d12Context->GetDevice(), d3d12Context->GetDirectQueue(), descHeap->Get(), filePath, resource, index);

			icons_[static_cast<Uint>(type)] = static_cast<ImTextureID>(descHeap->GPUHandle(index).ptr);
			resources_.push_back(std::move(resource));
		};

		load(IconType::FolderInItem, "Folder", "FolderInItem");
		load(IconType::FolderNoItem, "Folder", "FolderNoItem");

		load(IconType::Model,         "Asset", "Model");
		load(IconType::Effect,        "Asset", "Effect");
		load(IconType::Audio,         "Asset", "Audio");
		load(IconType::Font,          "Asset", "Font");
		load(IconType::Sky,           "Asset", "Sky");
		load(IconType::Animation,     "Asset", "Animation");
		load(IconType::MeshCollision, "Asset", "MeshCollision");
		load(IconType::Material,      "Asset", "Material");
		load(IconType::Movie,         "Asset", "Movie");

		load(IconType::Text,   "Asset", "Text");
		load(IconType::Cpp,    "Asset", "CPlusPlus");
		load(IconType::Header, "Asset", "Header");
		load(IconType::Hlsl,   "Asset", "Hlsl");

		load(IconType::Search, "Misc", "Search");

		load(IconType::Play,  "Toolbar", "Play");
		load(IconType::Pause, "Toolbar", "Pause");
		load(IconType::Stop,  "Toolbar", "Stop");

		load(IconType::Actor,       "Hierarchy", "Actor");
		load(IconType::ActorChild,  "Hierarchy", "ActorChild");
		load(IconType::Prefab,      "Hierarchy", "Prefab");
		load(IconType::PrefabChild, "Hierarchy", "PrefabChild");
		load(IconType::Scene,       "Asset", "Scene");

		load(IconType::Lock,     "Misc", "Lock");
		load(IconType::LockFree, "Misc", "LockFree");

		load(IconType::Guizmo,      "Viewport", "Guizmo");
		load(IconType::NonSelected, "Viewport", "NonSelected");
		load(IconType::Translate,   "Viewport", "Translate");
		load(IconType::Rotate,      "Viewport", "Rotate");
		load(IconType::Scale,       "Viewport", "Scale");
		load(IconType::Camera,      "Viewport", "Camera");
		load(IconType::ViewMode,    "Viewport", "ViewMode");

		load(IconType::LogError,   "Log", "Error");
		load(IconType::LogWarning, "Log", "Warning");
		load(IconType::LogNotice,  "Log", "Notice");

		load(IconType::NonCameraWarning, "Warning", "NonCamera");

		load(IconType::ActorActive,    "Hierarchy", "ActorActive");
		load(IconType::ActorNonActive, "Hierarchy", "ActorNonActive");

		load(IconType::ComponentTransform,             "Component", "Transform");
		load(IconType::ComponentCamera,                "Component", "Camera");
		load(IconType::ComponentFreeCameraController,  "Component", "FreeCameraController");
		load(IconType::ComponentOrbitCameraController, "Component", "OrbitCameraController");
		load(IconType::ComponentPointLight,       "Component", "PointLight");
		load(IconType::ComponentDirectionalLight, "Component", "DirectionalLight");
		load(IconType::ComponentSpotLight,        "Component", "SpotLight");
		load(IconType::ComponentRectangleLight,   "Component", "RectangleLight");
		load(IconType::ComponentSkyLight,         "Component", "SkyLight");
		load(IconType::ComponentBoxCollider,      "Component", "BoxCollider");
		load(IconType::ComponentSphereCollider,   "Component", "SphereCollider");
		load(IconType::ComponentCapsuleCollider,  "Component", "CapsuleCollider");
		load(IconType::ComponentCylinderCollider, "Component", "CylinderCollider");
		load(IconType::ComponentRectCollider,     "Component", "RectCollider");
		load(IconType::ComponentCircleCollider,   "Component", "CircleCollider");
		load(IconType::ComponentMeshCollider,     "Component", "MeshCollider");
		load(IconType::ComponentRigidbody, "Component", "Rigidbody");
		load(IconType::ComponentSoftbody,  "Component", "Softbody");
		load(IconType::ComponentCharacterController, "Component", "CharacterController");
		load(IconType::ComponentHingeJoint,  "Component", "HingeJoint");
		load(IconType::ComponentFixedJoint,  "Component", "FixedJoint");
		load(IconType::ComponentSpringJoint, "Component", "SpringJoint");
		load(IconType::ComponentSliderJoint, "Component", "SliderJoint");
		load(IconType::ComponentAudioSource,   "Component", "AudioSource");
		load(IconType::ComponentAudioListener, "Component", "AudioListener");
		load(IconType::ComponentImage, "Component", "Image");
		load(IconType::ComponentText,  "Component", "Text");
		load(IconType::ComponentMovie, "Component", "Movie");
		load(IconType::ComponentMesh,  "Component", "Mesh");
		load(IconType::ComponentMaterial, "Component", "Material");
		load(IconType::ComponentAnimator,           "Component", "Animator");
		load(IconType::ComponentPositionConstraint, "Component", "PositionConstraint");
		load(IconType::ComponentRotationConstraint, "Component", "RotationConstraint");
		load(IconType::ComponentLookAtConstraint,   "Component", "LookAtConstraint");
		load(IconType::ComponentParentConstraint,   "Component", "ParentConstraint");
		load(IconType::ComponentWeather,     "Component", "Weather");
		load(IconType::ComponentEffect,      "Component", "Effect");
		load(IconType::ComponentPostProcess, "Component", "PostProcess");
		load(IconType::ComponentSpawner,     "Component", "Spawner");
		load(IconType::ComponentLifetime,    "Component", "Lifetime");
		load(IconType::ComponentCustom,      "Component", "Custom");
	}

	ImTextureID ImGuiTexture::Icon(IconType type)const
	{
		return icons_[static_cast<Uint>(type)];
	}

	/// [EN] Maps a component's registered name to its Inspector/Add
	///      Component header icon (Unity-style). Falls back to
	///      IconType::ComponentCustom for anything not explicitly listed —
	///      covers UserProject scripts and any built-in component without
	///      a dedicated icon. Static (no instance state needed) so both
	///      InspectorPanel and AddComponentPanel can share one lookup table
	///      instead of each keeping their own.
	/// [JP] コンポーネントの登録名を Inspector/Add Component ヘッダー用
	///      アイコン（Unity 風）へ対応付ける。明示的に列挙されていないもの
	///      は IconType::ComponentCustom にフォールバックする —
	///      UserProject のスクリプトや、専用アイコンを持たない組み込み
	///      コンポーネントをカバーする。static（インスタンス状態不要）に
	///      することで、InspectorPanel と AddComponentPanel がそれぞれ
	///      別の対応表を持たず、1つを共有できる。
	IconType ImGuiTexture::ComponentIconType(const String& componentName)
	{
		/// [EN] Keyed by std::string content rather than String itself:
		///      String's hash/equality compare interned pointers, and this
		///      table's literals intern in Editor.exe's own copy of the
		///      intern pool while ComponentRegistry's names intern in
		///      SeedCore.dll's — same text, different pointers, so a
		///      String-keyed lookup silently missed every entry and fell
		///      back to ComponentCustom for everything. Comparing actual
		///      characters sidesteps that entirely.
		/// [JP] String 自体ではなく std::string の中身をキーにする:
		///      String のハッシュ/比較はインターン済みポインタを見るが、
		///      この対応表のリテラルは Editor.exe 自身が持つインターン
		///      プールに、ComponentRegistry の名前は SeedCore.dll 側の
		///      プールにそれぞれインターンされる — 同じ文字列でもポインタが
		///      異なるため、String をキーにした検索は全項目で静かに
		///      不一致となり、常に ComponentCustom にフォールバックして
		///      いた。実際の文字を比較すればこれを完全に回避できる。
		static const std::unordered_map<std::string, IconType> table =
		{
			{ "Camera", IconType::ComponentCamera },
			{ "FreeCameraController", IconType::ComponentFreeCameraController },
			{ "OrbitCameraController", IconType::ComponentOrbitCameraController },
			{ "PointLight", IconType::ComponentPointLight },
			{ "DirectionalLight", IconType::ComponentDirectionalLight },
			{ "SpotLight", IconType::ComponentSpotLight },
			{ "RectangleLight", IconType::ComponentRectangleLight },
			{ "SkyLight", IconType::ComponentSkyLight },
			{ "BoxCollider", IconType::ComponentBoxCollider },
			{ "SphereCollider", IconType::ComponentSphereCollider },
			{ "CapsuleCollider", IconType::ComponentCapsuleCollider },
			{ "CylinderCollider", IconType::ComponentCylinderCollider },
			{ "RectCollider", IconType::ComponentRectCollider },
			{ "CircleCollider", IconType::ComponentCircleCollider },
			{ "MeshCollider", IconType::ComponentMeshCollider },
			{ "Rigidbody", IconType::ComponentRigidbody },
			{ "Softbody", IconType::ComponentSoftbody },
			{ "CharacterController", IconType::ComponentCharacterController },
			{ "HingeJoint", IconType::ComponentHingeJoint },
			{ "FixedJoint", IconType::ComponentFixedJoint },
			{ "SpringJoint", IconType::ComponentSpringJoint },
			{ "SliderJoint", IconType::ComponentSliderJoint },
			{ "AudioSource", IconType::ComponentAudioSource },
			{ "AudioListener", IconType::ComponentAudioListener },
			{ "Image", IconType::ComponentImage },
			{ "Text", IconType::ComponentText },
			{ "Movie", IconType::ComponentMovie },
			{ "Mesh", IconType::ComponentMesh },
			{ "Material", IconType::ComponentMaterial },
			{ "Animator", IconType::ComponentAnimator },
			{ "PositionConstraint", IconType::ComponentPositionConstraint },
			{ "RotationConstraint", IconType::ComponentRotationConstraint },
			{ "LookAtConstraint", IconType::ComponentLookAtConstraint },
			{ "ParentConstraint", IconType::ComponentParentConstraint },
			{ "Weather", IconType::ComponentWeather },
			{ "Effect", IconType::ComponentEffect },
			{ "PostProcess", IconType::ComponentPostProcess },
			{ "Spawner", IconType::ComponentSpawner },
			{ "Lifetime", IconType::ComponentLifetime },
		};

		auto found = table.find(componentName.str());
		if (found != table.end())
		{
			return found->second;
		}

		return IconType::ComponentCustom;
	}
}