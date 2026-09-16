#pragma once
#include <FoundationEngine/Prelude.h>

#include <GraphicsEngine/Texture/TextureLoader.h>
#include <GraphicsEngine/Model/ModelLoader.h>
#include <GraphicsEngine/Model/Animation/AnimationLoader.h>
#include <GraphicsEngine/Model/Collision/MeshCollisionLoader.h>
#include <GraphicsEngine/Model/Material/MaterialLoader.h>
#include <GraphicsEngine/Model/Skeleton/SkeletonLoader.h>
#include <GraphicsEngine/Font/FontLoader.h>
#include <GraphicsEngine/Movie/MovieLoader.h>
#include <GraphicsEngine/Sky/SkymapLoader.h>
#include <GraphicsEngine/Effect/Effekseer/EffekseerLoader.h>
#include <AudioEngine/Audio/AudioLoader.h>

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
		/// [EN] Loads texture assets from disk and uploads them to the GPU.
		/// [JP] ディスクからテクスチャアセットを読み込み、GPU へアップロードする。
		ResourcePtr<TextureLoader> textureLoader_;

		/// [EN] Loads 3D model assets (geometry/materials).
		/// [JP] 3Dモデルアセット（ジオメトリ/マテリアル）を読み込む。
		ResourcePtr<ModelLoader> modelLoader_;

		/// [EN] Loads and splits animation clip data.
		/// [JP] アニメーションクリップデータを読み込み、分割する。
		ResourcePtr<AnimationLoader> animationLoader_;

		/// [EN] Loads and bakes mesh collision geometry.
		/// [JP] 衝突ジオメトリを読み込み、焼き込む。
		ResourcePtr<MeshCollisionLoader> meshCollisionLoader_;

		/// [EN] Loads and saves standalone ".material" assets.
		/// [JP] 単体 ".material" アセットの読み書きを行う。
		ResourcePtr<MaterialLoader> materialLoader_;

		/// [EN] Loads and saves standalone ".skeleton" assets (rig: sockets + root bone).
		/// [JP] 単体 ".skeleton" アセット（リグ: ソケット + ルートボーン）の読み書きを行う。
		ResourcePtr<SkeletonLoader> skeletonLoader_;

		/// [EN] Loads font assets (FreeType/HarfBuzz/MTSDF atlas).
		/// [JP] フォントアセット（FreeType/HarfBuzz/MTSDF アトラス）を読み込む。
		ResourcePtr<FontLoader> fontLoader_;

		/// [EN] Loads movie assets (Media Foundation decoder + frame texture).
		/// [JP] ムービーアセット（Media Foundation デコーダ + フレームテクスチャ）を読み込む。
		ResourcePtr<MovieLoader> movieLoader_;

		/// [EN] Loads skybox/environment map assets.
		/// [JP] スカイボックス/環境マップアセットを読み込む。
		ResourcePtr<SkymapLoader> skymapLoader_;

		/// [EN] Loads Effekseer effect (.efkefc) assets.
		/// [JP] Effekseerエフェクト(.efkefc)アセットを読み込む。
		ResourcePtr<EffekseerLoader> effekseerLoader_;

		/// [EN] Loads ".audio" assets (cue sheet or waveform).
		/// [JP] ".audio" アセット（キューシートまたは波形）を読み込む。
		ResourcePtr<AudioLoader> audioLoader_;

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
