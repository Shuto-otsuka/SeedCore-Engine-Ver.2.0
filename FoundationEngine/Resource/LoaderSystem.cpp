#include <FoundationEngine/Resource/LoaderSystem.h>

namespace SeedCore
{
	/**
	* [EN]
	* Constructs the loader system, creating each format-specific loader.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* ローダーシステムを構築し、各フォーマット固有のローダーを生成する。
	*/
	LoaderSystem::LoaderSystem(ID3D12Device* device)
	{
		textureLoader_ = MakePtr<TextureLoader>();
		imageLoader_ = MakePtr<ImageLoader>();
		modelLoader_ = MakePtr<ModelLoader>();
		animationLoader_ = MakePtr<AnimationLoader>();
		skymapLoader_ = MakePtr<SkymapLoader>();
		effekseerLoader_ = MakePtr<EffekseerLoader>();
	}
}
