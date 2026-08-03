#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	struct EditorContext;

	enum class IconType : Uint
	{
		FolderInItem,
		FolderNoItem,

		Model,
		Effect,
		Audio,
		Font,
		Sky,
		Animation,
		Movie,

		Text,
		Cpp,
		Hlsl,
		Search,

		Play,
		Pause,
		Stop,

		Actor,
		ActorChild,
		Prefab,
		PrefabChild,
		Scene,

		Lock,
		LockFree,

		Guizmo,
		NonSelected,
		Translate,
		Rotate,
		Scale,
		Camera,
		ViewMode,

		LogError,
		LogWarning,
		LogNotice,

		ActorActive,
		ActorNonActive,

		Count
	};

	class ImGuiTexture
	{
	public:
		ImGuiTexture(EditorContext& context);
		~ImGuiTexture() = default;

		[[nodiscard]] ImTextureID Icon(IconType type)const;

	private:
		ImTextureID icons_[static_cast<Uint>(IconType::Count)] = {};

		DynamicArray<Microsoft::WRL::ComPtr<ID3D12Resource>> resources_;
	};
}
