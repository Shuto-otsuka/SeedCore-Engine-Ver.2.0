#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	/// [EN] One embedded dependency (texture/model/material/wave/curve), keyed
	///      by the full path Effekseer resolves it to at load time (materialPath
	///      + the path recorded inside the .efkefc).
	/// [JP] 埋め込み済みの依存関係1件(テクスチャ/モデル/マテリアル/wave/curve)。
	///      キーはEffekseerがロード時に解決するフルパス(materialPath + .efkefc
	///      内に記録された相対パス)。
	struct EffekseerDependency
	{
		std::u16string path_;

		DynamicArray<Byte> data_;

		template<class T>
		void serialize(T& archive)
		{
			archive(
				cereal::make_nvp("path", path_), 
				cereal::make_nvp("data", data_)
			);
		}
	};

	/**
	* [EN]
	* Cereal-serialisable wrapper around a raw ".efkefc" byte blob plus every
	* dependency (texture/model/material/wave/curve) it references, written
	* as a ".effekseer" binary cache next to the source file (mirrors the
	* ".crister" cache for models). Ships without exposing the original
	* Effekseer project's raw ".efkefc"/".efkmodel"/texture source layout.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 生の ".efkefc" バイト列と、それが参照する全依存関係(テクスチャ/モデル/
	* マテリアル/wave/curve)をまとめてラップするcerealシリアライズ可能な
	* 構造体。ソースファイルの隣に ".effekseer" バイナリキャッシュとして
	* 書き出す（モデルの ".crister" キャッシュと同じ構図）。元のEffekseer
	* プロジェクトが持つ生の ".efkefc"/".efkmodel"/テクスチャのソース構成を
	* 露出せずに出荷できる。
	*/
	struct EffekseerCache
	{
		DynamicArray<Byte> data_;

		DynamicArray<EffekseerDependency> dependencies_;

		template<class T>
		void serialize(T& archive)
		{
			archive(
				cereal::make_nvp("data", data_), 
				cereal::make_nvp("dependencies", dependencies_)
			);
		}
	};
}
