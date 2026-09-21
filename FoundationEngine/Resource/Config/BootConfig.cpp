#include <FoundationEngine/Resource/Config/BootConfig.h>
#include <FoundationEngine/Serialization/Binary/BinaryArchive.h>
#include <FoundationEngine/Log/Warning.h>

namespace SeedCore
{
	/**
	* [EN]
	* Loads the settings from path. Leaves every field at its current value
	* if the file does not exist yet (the loading screen has never been
	* edited).
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* path から設定を読み込む。ファイルがまだ存在しない（ローディング画面を
	* 一度も編集していない）場合は、どのフィールドも変更しない。
	*/
	void BootConfig::Load(const std::filesystem::path& path)
	{
		/// [EN] Checked up front so a project that never edited the loading screen does not log a missing-file warning.
		/// [JP] 一度もローディング画面を編集していないプロジェクトでファイル無しの警告を出さないよう、先に確認する。
		if (!std::filesystem::exists(path))
		{
			return;
		}

		BinaryInputArchive archive;
		if (!archive.Read(String(path.string())))
		{
			return;
		}

		/// [EN] Enums are stored as Int32, so the file layout does not depend on the enum's underlying type.
		/// [JP] 列挙型は Int32 として保存する。ファイルの形式が列挙型の基底型に左右されないようにするため。
		Int32 anchorValue = static_cast<Int32>(anchor_);
		Int32 fillMethodValue = static_cast<Int32>(fillMethod_);

		archive.TryField("backgroundImage", backgroundImage_);
		archive.TryField("barImage", barImage_);
		archive.TryField("frameImage", frameImage_);
		archive.TryField("useBackgroundImage", useBackgroundImage_);
		archive.TryField("useFrame", useFrame_);
		archive.TryField("backgroundColor", backgroundColor_);
		archive.TryField("backgroundTint", backgroundTint_);
		archive.TryField("barTint", barTint_);
		archive.TryField("frameTint", frameTint_);
		archive.TryField("anchor", anchorValue);
		archive.TryField("offset", offset_);
		archive.TryField("width", width_);
		archive.TryField("fillMethod", fillMethodValue);
		archive.TryField("fillOrigin", fillOrigin_);
		archive.TryField("clockwise", clockwise_);
		archive.TryField("wave", wave_);

		anchor_ = static_cast<BootAnchor>(anchorValue);
		fillMethod_ = static_cast<BootFillMethod>(fillMethodValue);
	}

	/**
	* [EN]
	* Writes the settings to path, creating the parent directory if needed.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 設定を path へ書き込む。必要なら親ディレクトリを作成する。
	*/
	void BootConfig::Save(const std::filesystem::path& path)const
	{
		if (path.has_parent_path())
		{
			std::filesystem::create_directories(path.parent_path());
		}

		Int32 anchorValue = static_cast<Int32>(anchor_);
		Int32 fillMethodValue = static_cast<Int32>(fillMethod_);

		BinaryOutputArchive archive;
		archive.Field("backgroundImage", backgroundImage_);
		archive.Field("barImage", barImage_);
		archive.Field("frameImage", frameImage_);
		archive.Field("useBackgroundImage", useBackgroundImage_);
		archive.Field("useFrame", useFrame_);
		archive.Field("backgroundColor", backgroundColor_);
		archive.Field("backgroundTint", backgroundTint_);
		archive.Field("barTint", barTint_);
		archive.Field("frameTint", frameTint_);
		archive.Field("anchor", anchorValue);
		archive.Field("offset", offset_);
		archive.Field("width", width_);
		archive.Field("fillMethod", fillMethodValue);
		archive.Field("fillOrigin", fillOrigin_);
		archive.Field("clockwise", clockwise_);
		archive.Field("wave", wave_);
		archive.Write(String(path.string()));
	}

	/**
	* [EN]
	* Reads imagePath into image after checking that the engine can decode
	* it (DDS first, then WIC). Returns false and leaves image untouched
	* otherwise.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* エンジンで読める画像か（DDS、次に WIC の順）を確認したうえで、
	* imagePath の中身を image へ読み込む。読めなければ false を返し、
	* image は変更しない。
	*/
	Bool BootConfig::Import(const std::filesystem::path& imagePath, DynamicArray<Byte>& image)
	{
		std::ifstream stream(imagePath, std::ios::binary);
		if (!stream)
		{
			SC_LOG_WARNING("ローディング画面用の画像を開けませんでした: {}", imagePath.string());
			return false;
		}

		DynamicArray<Byte> data((std::istreambuf_iterator<Char>(stream)), std::istreambuf_iterator<Char>());
		if (data.empty())
		{
			SC_LOG_WARNING("ローディング画面用の画像が空です: {}", imagePath.string());
			return false;
		}

		/// [EN] The same two decoders the texture loader tries, in the same order, so anything accepted here is guaranteed to load at runtime.
		/// [JP] テクスチャローダーが試すのと同じ2つのデコーダを同じ順で試す。ここで通った画像は実行時にも必ず読める。
		DirectX::TexMetadata metadata{};
		Bool decodable = SUCCEEDED(DirectX::GetMetadataFromDDSMemory(reinterpret_cast<const Uint8*>(data.data()), data.size(), DirectX::DDS_FLAGS_NONE, metadata));

		if (!decodable)
		{
			/// [EN] WIC is a COM API; initialize COM on this thread only if nobody has yet, and balance only our own initialization.
			/// [JP] WIC は COM API なので、このスレッドでまだ誰も COM を初期化していない場合だけ初期化し、自分が行った初期化分だけ後で解除する。
			Bool comInitializedHere = SUCCEEDED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED));
			decodable = SUCCEEDED(DirectX::GetMetadataFromWICMemory(reinterpret_cast<const Uint8*>(data.data()), data.size(), DirectX::WIC_FLAGS_NONE, metadata));
			if (comInitializedHere)
			{
				CoUninitialize();
			}
		}

		if (!decodable)
		{
			SC_LOG_WARNING("ローディング画面用の画像として読み込めない形式です: {}", imagePath.string());
			return false;
		}

		image = std::move(data);
		return true;
	}
}
