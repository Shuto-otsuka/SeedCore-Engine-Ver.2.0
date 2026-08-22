#include <GraphicsEngine/Effect/Effekseer/EffekseerLoader.h>
#include <GraphicsEngine/Effect/Effekseer/EffekseerManager.h>
#include <GraphicsEngine/Effect/Effekseer/EffekseerCache.h>
#include <FoundationEngine/Resource/Gateway.h>
#include <FoundationEngine/Log/Warning.h>
#include <FoundationEngine/Log/Error.h>
#include <FoundationEngine/Serialization/Binary/BinaryArchive.h>

namespace SeedCore
{
	EffekseerEffectHandle::EffekseerEffectHandle(EffekseerEffectHandle&& other)noexcept :effect_(std::move(other.effect_))
	{
		/// No Code
	}

	EffekseerEffectHandle& EffekseerEffectHandle::operator=(EffekseerEffectHandle&& other)noexcept
	{
		effect_ = std::move(other.effect_);
		return *this;
	}

	namespace
	{
		/// [EN] Mirrors Effekseer's internal PathHelper::Combine exactly (append a
		///      separator if path1 doesn't already end in one, then normalise every
		///      backslash in the result to a forward slash) so the keys this loader
		///      computes match the paths Effekseer itself passes to FileInterface::OpenRead.
		/// [JP] Effekseer内部のPathHelper::Combineを正確に再現する(path1が区切り文字で
		///      終わっていなければ追加し、結果全体のバックスラッシュをスラッシュへ正規化)。
		///      これによりこのローダーが計算するキーが、Effekseer自身が
		///      FileInterface::OpenReadへ渡すパスと一致する。
		std::u16string CombinePath(const std::u16string& path1, const std::u16string& path2)
		{
			std::u16string result = path1;
			if (!result.empty() && result.back() != u'/' && result.back() != u'\\')
			{
				result += u'/';
			}
			result += path2;

			for (Char16& c : result)
			{
				if (c == u'\\')
				{
					c = u'/';
				}
			}

			return result;
		}

		/// [EN] Reads a dependency's bytes from disk and registers it (dedup by
		///      full path) into the collector, unless the path is empty.
		/// [JP] 依存関係のバイト列をディスクから読み込み、collectorへ登録する
		///      (フルパスで重複排除)。パスが空なら何もしない。
		void CollectDependency(const std::u16string& materialPath, const Char16* relativePath, FlatMap<std::u16string, DynamicArray<Byte>>& collector)
		{
			if (relativePath == nullptr || relativePath[0] == u'\0')
			{
				return;
			}

			std::u16string fullPath = CombinePath(materialPath, relativePath);
			if (collector.contains(fullPath))
			{
				return;
			}

			std::filesystem::path diskPath(fullPath);
			if (!std::filesystem::exists(diskPath))
			{
				return;
			}

			std::ifstream ifs(diskPath, std::ios::binary);
			DynamicArray<Byte> bytes((std::istreambuf_iterator<Char>(ifs)), std::istreambuf_iterator<Char>());
			collector.insert({ fullPath, std::move(bytes) });
		}

		/// [EN] Enumerates every texture/model/material/wave/curve an already-created
		///      effect references and collects their bytes, deduplicated by full path.
		/// [JP] 既に生成済みのエフェクトが参照する全テクスチャ/モデル/マテリアル/
		///      wave/curveを列挙し、フルパスで重複排除しながらバイト列を集める。
		FlatMap<std::u16string, DynamicArray<Byte>> CollectDependencies(const Effekseer::EffectRef& effect, const std::u16string& materialPath)
		{
			FlatMap<std::u16string, DynamicArray<Byte>> collector;

			for (Int32 index = 0; index < effect->GetColorImageCount(); index++)
			{
				CollectDependency(materialPath, effect->GetColorImagePath(index), collector);
			}
			for (Int32 index = 0; index < effect->GetNormalImageCount(); index++)
			{
				CollectDependency(materialPath, effect->GetNormalImagePath(index), collector);
			}
			for (Int32 index = 0; index < effect->GetDistortionImageCount(); index++)
			{
				CollectDependency(materialPath, effect->GetDistortionImagePath(index), collector);
			}
			for (Int32 index = 0; index < effect->GetWaveCount(); index++)
			{
				CollectDependency(materialPath, effect->GetWavePath(index), collector);
			}
			for (Int32 index = 0; index < effect->GetModelCount(); index++)
			{
				CollectDependency(materialPath, effect->GetModelPath(index), collector);
			}
			for (Int32 index = 0; index < effect->GetMaterialCount(); index++)
			{
				CollectDependency(materialPath, effect->GetMaterialPath(index), collector);
			}
			for (Int32 index = 0; index < effect->GetCurveCount(); index++)
			{
				CollectDependency(materialPath, effect->GetCurvePath(index), collector);
			}

			return collector;
		}
	}

	/**
	* [EN]
	* Loads an Effekseer effect and returns a handle to the resulting
	* EffectRef.
	*
	* Flow:
	*   1. If a pre-built ".effekseer" binary cache exists alongside the
	*      source file, deserialise the raw ".efkefc" bytes plus every
	*      embedded dependency (texture/model/material/wave/curve) from
	*      cache (fast path, and the only form shipped in a release
	*      build), registering each dependency into the shared
	*      EffekseerFileInterface before creating the effect so texture/
	*      model/material loading resolves from memory instead of disk.
	*   2. Otherwise, read the ".efkefc" source bytes directly, create the
	*      effect from disk (dependencies resolve normally via the on-
	*      disk fallback), then enumerate and read every dependency the
	*      freshly-created effect references and serialise the source
	*      bytes plus all of them together as a ".effekseer" cache for
	*      future loads.
	*
	* In both cases the byte-buffer overload of Effekseer::Effect::Create
	* is used, explicitly passing the source file's parent directory as
	* materialPath — unlike the path overload, the byte-buffer overload
	* does not derive it automatically.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* Effekseerエフェクトを読み込み、結果のEffectRefへのハンドルを返す。
	*
	* フロー:
	*   1. ソースファイルと同じ場所に ".effekseer" バイナリキャッシュが
	*      あれば、生の ".efkefc" バイト列と埋め込み済みの全依存関係
	*      （テクスチャ/モデル/マテリアル/wave/curve）をキャッシュから
	*      デシリアライズする（高速パス。リリースビルドで出荷するのも
	*      この形式のみ）。エフェクト生成前に各依存関係を共有の
	*      EffekseerFileInterfaceへ登録し、テクスチャ/モデル/マテリアルの
	*      読み込みがディスクではなくメモリから解決されるようにする。
	*   2. なければ ".efkefc" ソースのバイト列を直接読み込み、ディスクから
	*      エフェクトを生成し（依存関係はディスクフォールバック経由で通常
	*      通り解決される）、生成直後のエフェクトが参照する全依存関係を
	*      列挙・読み込みし、ソースのバイト列と合わせて ".effekseer"
	*      キャッシュとしてシリアライズし次回のロードに備える。
	*
	* いずれの場合も Effekseer::Effect::Create のバイトバッファ版に渡す際、
	* ソースファイルの親ディレクトリを materialPath として明示的に渡す —
	* pathバージョンと違いバイトバッファ版は自動で解決してくれないため。
	*/
	Handle<EffekseerEffectHandle> EffekseerLoader::Load(String filePath)
	{
		std::filesystem::path path(filePath.c_str());
		std::filesystem::path cachePath = path;
		cachePath.replace_extension(".effekseer");

		std::u16string materialPath = path.parent_path().u16string();
		for (Char16& c : materialPath)
		{
			if (c == u'\\')
			{
				c = u'/';
			}
		}
		if (!materialPath.empty() && materialPath.back() != u'/')
		{
			materialPath += u'/';
		}

		EffekseerManager& manager = Gateway::GetEffekseerManager();
		Effekseer::EffectRef effect;

		if (path.extension() == ".effekseer" && std::filesystem::exists(cachePath))
		{
			BinaryInputArchive archive;
			if (archive.Read(String(cachePath.string())))
			{
				EffekseerCache cache;
				cache.Serialize(archive);

				for (EffekseerDependency& dependency : cache.dependencies_)
				{
					manager.GetFileInterface().Register(dependency.path_, std::move(dependency.data_));
				}

				effect = Effekseer::Effect::Create(manager.GetManager(), cache.data_.data(), static_cast<Int32>(cache.data_.size()), 1.0f, materialPath.c_str());
			}
			else
			{
				SC_LOG_WARNING("Effekseerキャッシュの読み込みに失敗しました。ソースから再生成します: {}", filePath.c_str());
				effect = nullptr;
			}
		}

		if (effect == nullptr)
		{
			if (path.extension() != ".efkefc")
			{
				SC_LOG_ERROR("Effekseerエフェクトのロードに失敗しました。拡張子が.efkefc/.effekseerではありません: {}", filePath.c_str());
				return Handle<EffekseerEffectHandle>::null();
			}
			if (!std::filesystem::exists(path))
			{
				SC_LOG_ERROR("Effekseerエフェクトのロードに失敗しました。ファイルが存在しません: {}", filePath.c_str());
				return Handle<EffekseerEffectHandle>::null();
			}

			std::ifstream ifs(path, std::ios::binary);
			DynamicArray<Byte> bytes((std::istreambuf_iterator<Char>(ifs)), std::istreambuf_iterator<Char>());
			if (bytes.empty())
			{
				SC_LOG_ERROR("Effekseerエフェクトのロードに失敗しました。ファイルの読み込みに失敗、または空です: {}", filePath.c_str());
				return Handle<EffekseerEffectHandle>::null();
			}

			effect = Effekseer::Effect::Create(manager.GetManager(), bytes.data(), static_cast<Int32>(bytes.size()), 1.0f, materialPath.c_str());
			if (effect == nullptr)
			{
				SC_LOG_ERROR("Effekseer::Effect::Createに失敗しました: {} (materialPath: {})", filePath.c_str(), std::filesystem::path(materialPath).string());
				return Handle<EffekseerEffectHandle>::null();
			}

			FlatMap<std::u16string, DynamicArray<Byte>> collected = CollectDependencies(effect, materialPath);

			EffekseerCache cache;
			cache.data_ = bytes;
			for (auto& [dependencyPath, dependencyData] : collected)
			{
				EffekseerDependency dependency;
				dependency.path_ = dependencyPath;
				dependency.data_ = dependencyData;
				cache.dependencies_.push_back(std::move(dependency));
			}

			BinaryOutputArchive archive;
			cache.Serialize(archive);
			archive.Write(String(cachePath.string()));
		}

		Handle<EffekseerEffectHandle> handle = pool_.Create();
		EffekseerEffectHandle* slot = pool_.Get(handle);
		if (!slot)
		{
			return Handle<EffekseerEffectHandle>::null();
		}

		slot->effect_ = effect;
		return handle;
	}

	Effekseer::EffectRef* EffekseerLoader::Get(const Handle<EffekseerEffectHandle>& handle)
	{
		EffekseerEffectHandle* slot = pool_.Get(handle);
		if (!slot)
		{
			return nullptr;
		}
		return &slot->effect_;
	}

	void EffekseerLoader::Clear(Handle<EffekseerEffectHandle>& handle)noexcept
	{
		pool_.Destroy(handle);
	}
}
