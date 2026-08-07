#pragma once
#include <FoundationEngine/Prelude.h>

#include <GraphicsEngine/Resource/TextureLoader.h>
#include <GraphicsEngine/Texture/ImageLoader.h>
#include <GraphicsEngine/Model/ModelLoader.h>
#include <GraphicsEngine/Model/Animation/AnimationLoader.h>
#include <GraphicsEngine/Model/Collision/MeshCollisionLoader.h>
#include <GraphicsEngine/Sky/SkymapLoader.h>
#include <GraphicsEngine/Effect/Effekseer/EffekseerLoader.h>

namespace SeedCore
{
	/**
	* [EN]
	* Bundles the low-level, format-specific loader objects (texture,
	* sprite, model, animation, skymap) that ResourceCache's resource
	* managers delegate actual file-format parsing/GPU-upload work to.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* ResourceCache のリソースマネージャが、実際のファイルフォーマット
	* 解析/GPU アップロード処理を委譲する、低水準でフォーマット固有の
	* ローダーオブジェクト（テクスチャ、スプライト、モデル、
	* アニメーション、スカイマップ）をまとめて保持する。
	*/
	struct SEEDCORE_API LoaderSystem
	{
		/// [EN] Loads raw texture data from disk into GPU-uploadable form.
		/// [JP] ディスクから生のテクスチャデータを読み込み、GPU へアップロード可能な形式にする。
		ResourcePtr<TextureLoader> textureLoader_;

		/// [EN] Loads sprite/2D-texture assets.
		/// [JP] スプライト/2Dテクスチャアセットを読み込む。
		ResourcePtr<ImageLoader> imageLoader_;

		/// [EN] Loads 3D model assets (geometry/materials).
		/// [JP] 3Dモデルアセット（ジオメトリ/マテリアル）を読み込む。
		ResourcePtr<ModelLoader> modelLoader_;

		/// [EN] Loads and splits animation clip data.
		/// [JP] アニメーションクリップデータを読み込み、分割する。
		ResourcePtr<AnimationLoader> animationLoader_;

		/// [EN] Loads and bakes mesh collision geometry.
		/// [JP] 衝突ジオメトリを読み込み、焼き込む。
		ResourcePtr<MeshCollisionLoader> meshCollisionLoader_;

		/// [EN] Loads skybox/environment map assets.
		/// [JP] スカイボックス/環境マップアセットを読み込む。
		ResourcePtr<SkymapLoader> skymapLoader_;

		/// [EN] Loads Effekseer effect (.efkefc) assets.
		/// [JP] Effekseerエフェクト(.efkefc)アセットを読み込む。
		ResourcePtr<EffekseerLoader> effekseerLoader_;

		/**
		* [EN]
		* Constructs the loader system, creating each format-specific loader.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* ローダーシステムを構築し、各フォーマット固有のローダーを生成する。
		*/
		LoaderSystem(ID3D12Device* device);
	};
}
