#include <Editor/Editor/ImGui/ImGuiTexture.h>
#include <Editor/Editor/EditorContext.h>
#include <GraphicsEngine/D3D12/Descriptor/DescriptorHeap.h>
#include <GraphicsEngine/D3D12/Context/D3D12CommandQueue.h>
#include <FoundationEngine/Resource/LoaderSystem.h>

namespace SeedCore
{
	ImGuiTexture::ImGuiTexture(EditorContext& context)
	{
		TextureLoader* loader = context.worldContext_.loader_->textureLoader_.get();

		auto load = [&](IconType type, const String& filePath)
		{
			Uint index = context.graphicsContext_.descHeap_->AllocateIndex();

			Microsoft::WRL::ComPtr<ID3D12Resource> resource;
			loader->CreateTexture(context.graphicsContext_.device_, context.graphicsContext_.cmdQueue_, context.graphicsContext_.descHeap_->Get(), filePath, resource, index);

			icons_[static_cast<Uint>(type)] = static_cast<ImTextureID>(context.graphicsContext_.descHeap_->GPUHandle(index).ptr);
			resources_.push_back(std::move(resource));
		};

		load(IconType::FolderInItem, String("Icon/Folder/folder_initem_icon.dds"));
		load(IconType::FolderNoItem, String("Icon/Folder/folder_noitem_icon.dds"));

		load(IconType::Model,         String("Icon/Asset/model_icon.dds"));
		load(IconType::Effect,        String("Icon/Asset/effect_icon.dds"));
		load(IconType::Audio,         String("Icon/Asset/audio_icon.dds"));
		load(IconType::Font,          String("Icon/Asset/font_icon.dds"));
		load(IconType::Sky,           String("Icon/Asset/skymap_icon.dds"));
		load(IconType::Animation,     String("Icon/Asset/animation_icon.dds"));
		load(IconType::MeshCollision, String("Icon/Asset/meshcollision_icon.dds"));
		load(IconType::Movie,         String("Icon/Asset/movie_icon.dds"));

		load(IconType::Text,   String("Icon/Asset/text_icon.dds"));
		load(IconType::Cpp,    String("Icon/Asset/cplusplus_icon.dds"));
		load(IconType::Header, String("Icon/Asset/header_icon.dds"));
		load(IconType::Hlsl,   String("Icon/Asset/hlsl_icon.dds"));

		load(IconType::Search, String("Icon/Misc/search_icon.dds"));

		load(IconType::Play,  String("Icon/Toolbar/play_icon.dds"));
		load(IconType::Pause, String("Icon/Toolbar/pause_icon.dds"));
		load(IconType::Stop,  String("Icon/Toolbar/stop_icon.dds"));

		load(IconType::Actor,       String("Icon/Hierarchy/actor_item_non_prefab_icon.dds"));
		load(IconType::ActorChild,  String("Icon/Hierarchy/actor_item_child_non_prefab_icon.dds"));
		load(IconType::Prefab,      String("Icon/Hierarchy/actor_item_prefab_icon.dds"));
		load(IconType::PrefabChild, String("Icon/Hierarchy/actor_item_child_prefab_icon.dds"));
		load(IconType::Scene,       String("Icon/Asset/scene_icon.dds"));

		load(IconType::Lock,     String("Icon/Misc/lock_icon.dds"));
		load(IconType::LockFree, String("Icon/Misc/lock_free_icon.dds"));

		load(IconType::Guizmo,      String("Icon/Viewport/guizmo_icon.dds"));
		load(IconType::NonSelected, String("Icon/Viewport/non_selected_icon.dds"));
		load(IconType::Translate,   String("Icon/Viewport/translation_icon.dds"));
		load(IconType::Rotate,      String("Icon/Viewport/rotate_icon.dds"));
		load(IconType::Scale,       String("Icon/Viewport/scaling_icon.dds"));
		load(IconType::Camera,      String("Icon/Viewport/camera_icon.dds"));
		load(IconType::ViewMode,    String("Icon/Viewport/view_mode_icon.dds"));

		load(IconType::LogError,   String("Icon/Log/error_icon.dds"));
		load(IconType::LogWarning, String("Icon/Log/warning_icon.dds"));
		load(IconType::LogNotice,  String("Icon/Log/notice_icon.dds"));

		load(IconType::ActorActive,    String("Icon/Hierarchy/actor_active_icon.dds"));
		load(IconType::ActorNonActive, String("Icon/Hierarchy/actor_non_active_icon.dds"));

		load(IconType::ComponentTransform,             String("Icon/Component/transform_icon.dds"));
		load(IconType::ComponentCamera,                String("Icon/Component/camera_icon.dds"));
		load(IconType::ComponentFreeCameraController,  String("Icon/Component/freecameracontroller_icon.dds"));
		load(IconType::ComponentOrbitCameraController, String("Icon/Component/orbitcameracontroller_icon.dds"));
		load(IconType::ComponentPointLight,       String("Icon/Component/pointlight_icon.dds"));
		load(IconType::ComponentDirectionalLight, String("Icon/Component/directionallight_icon.dds"));
		load(IconType::ComponentSpotLight,        String("Icon/Component/spotlight_icon.dds"));
		load(IconType::ComponentRectangleLight,   String("Icon/Component/rectanglelight_icon.dds"));
		load(IconType::ComponentSkyLight,         String("Icon/Component/skylight_icon.dds"));
		load(IconType::ComponentBoxCollider,      String("Icon/Component/boxcollider_icon.dds"));
		load(IconType::ComponentSphereCollider,   String("Icon/Component/spherecollider_icon.dds"));
		load(IconType::ComponentCapsuleCollider,  String("Icon/Component/capsulecollider_icon.dds"));
		load(IconType::ComponentCylinderCollider, String("Icon/Component/cylindercollider_icon.dds"));
		load(IconType::ComponentRectCollider,     String("Icon/Component/rectcollider_icon.dds"));
		load(IconType::ComponentCircleCollider,   String("Icon/Component/circlecollider_icon.dds"));
		load(IconType::ComponentMeshCollider,     String("Icon/Component/meshcollider_icon.dds"));
		load(IconType::ComponentRigidbody, String("Icon/Component/rigidbody_icon.dds"));
		load(IconType::ComponentSoftbody,  String("Icon/Component/softbody_icon.dds"));
		load(IconType::ComponentAudioSource,   String("Icon/Component/audiosource_icon.dds"));
		load(IconType::ComponentAudioListener, String("Icon/Component/audiolistener_icon.dds"));
		load(IconType::ComponentImage, String("Icon/Component/image_icon.dds"));
		load(IconType::ComponentText,  String("Icon/Component/text_icon.dds"));
		load(IconType::ComponentMovie, String("Icon/Component/movie_icon.dds"));
		load(IconType::ComponentMesh,  String("Icon/Component/mesh_icon.dds"));
		load(IconType::ComponentAnimator,           String("Icon/Component/animator_icon.dds"));
		load(IconType::ComponentPositionConstraint, String("Icon/Component/positionconstraint_icon.dds"));
		load(IconType::ComponentRotationConstraint, String("Icon/Component/rotationconstraint_icon.dds"));
		load(IconType::ComponentLookAtConstraint,   String("Icon/Component/lookatconstraint_icon.dds"));
		load(IconType::ComponentParentConstraint,   String("Icon/Component/parentconstraint_icon.dds"));
		load(IconType::ComponentWeather,     String("Icon/Component/weather_icon.dds"));
		load(IconType::ComponentEffect,      String("Icon/Component/effect_icon.dds"));
		load(IconType::ComponentPostProcess, String("Icon/Component/postprocess_icon.dds"));
		load(IconType::ComponentCustom,      String("Icon/Component/customcomponent_icon.dds"));
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
			{ "AudioSource", IconType::ComponentAudioSource },
			{ "AudioListener", IconType::ComponentAudioListener },
			{ "Image", IconType::ComponentImage },
			{ "Text", IconType::ComponentText },
			{ "Movie", IconType::ComponentMovie },
			{ "Mesh", IconType::ComponentMesh },
			{ "Animator", IconType::ComponentAnimator },
			{ "PositionConstraint", IconType::ComponentPositionConstraint },
			{ "RotationConstraint", IconType::ComponentRotationConstraint },
			{ "LookAtConstraint", IconType::ComponentLookAtConstraint },
			{ "ParentConstraint", IconType::ComponentParentConstraint },
			{ "Weather", IconType::ComponentWeather },
			{ "Effect", IconType::ComponentEffect },
			{ "PostProcess", IconType::ComponentPostProcess },
		};

		auto found = table.find(componentName.str());
		if (found != table.end())
		{
			return found->second;
		}

		return IconType::ComponentCustom;
	}
}