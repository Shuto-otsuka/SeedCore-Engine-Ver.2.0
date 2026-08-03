#include <Editor/Editor/ImGui/ImGuiTexture.h>
#include <Editor/Editor/EditorContext.h>
#include <GraphicsEngine/D3D12/Descriptor/DescriptorHeap.h>
#include <FoundationEngine/Resource/LoaderSystem.h>

namespace SeedCore
{
	ImGuiTexture::ImGuiTexture(EditorContext& context)
	{
		TextureLoader* loader = context.loader_->textureLoader_.get();

		auto load = [&](IconType type, const String& filePath)
		{
			Uint index = context.descHeap_->AllocateIndex();

			Microsoft::WRL::ComPtr<ID3D12Resource> resource;
			loader->CreateTexture(context.device_, context.cmdQueue_, context.descHeap_->Get(), filePath, resource, index);

			icons_[static_cast<Uint>(type)] = static_cast<ImTextureID>(context.descHeap_->GPUHandle(index).ptr);
			resources_.push_back(std::move(resource));
		};

		load(IconType::FolderInItem, String("Icon/folder_initem.dds"));
		load(IconType::FolderNoItem, String("Icon/folder_noitem.dds"));

		load(IconType::Model,        String("Icon/model_icon.dds"));
		load(IconType::Effect,       String("Icon/effect_icon.dds"));
		load(IconType::Audio,        String("Icon/audio_icon.dds"));
		load(IconType::Font,         String("Icon/font_icon.dds"));
		load(IconType::Sky,          String("Icon/skymap_icon.dds"));
		load(IconType::Animation,    String("Icon/animation_icon.dds"));
		load(IconType::Movie,        String("Icon/movie_icon.dds"));

		load(IconType::Text,         String("Icon/text_icon.dds"));
		load(IconType::Cpp,          String("Icon/cplusplus_icon.dds"));
		load(IconType::Hlsl,         String("Icon/hlsl_icon.dds"));
		load(IconType::Search,       String("Icon/search_icon.dds"));

		load(IconType::Play,  String("Icon/play_icon.dds"));
		load(IconType::Pause, String("Icon/pause_icon.dds"));
		load(IconType::Stop,  String("Icon/stop_icon.dds"));

		load(IconType::Actor,       String("Icon/actor_item_non_prefab_icon.dds"));
		load(IconType::ActorChild,  String("Icon/actor_item_child_non_prefab_icon.dds"));
		load(IconType::Prefab,      String("Icon/actor_item_prefab_icon.dds"));
		load(IconType::PrefabChild, String("Icon/actor_item_child_prefab_icon.dds"));
		load(IconType::Scene,       String("Icon/scene_icon.dds"));

		load(IconType::Lock,     String("Icon/lock_icon.dds"));
		load(IconType::LockFree, String("Icon/lock_free_icon.dds"));

		load(IconType::Guizmo,      String("Icon/guizmo_icon.dds"));
		load(IconType::NonSelected, String("Icon/non_selected_icon.dds"));
		load(IconType::Translate,   String("Icon/translation_icon.dds"));
		load(IconType::Rotate,      String("Icon/rotate_icon.dds"));
		load(IconType::Scale,       String("Icon/scaling_icon.dds"));
		load(IconType::Camera,      String("Icon/camera_icon.dds"));
		load(IconType::ViewMode,    String("Icon/view_mode_icon.dds"));

		load(IconType::LogError,   String("Icon/error_icon.dds"));
		load(IconType::LogWarning, String("Icon/warning_icon.dds"));
		load(IconType::LogNotice,  String("Icon/notice_icon.dds"));

		load(IconType::ActorActive,    String("Icon/actor_active_icon.dds"));
		load(IconType::ActorNonActive, String("Icon/actor_non_active_icon.dds"));
	}

	ImTextureID ImGuiTexture::Icon(IconType type)const
	{
		return icons_[static_cast<Uint>(type)];
	}
}