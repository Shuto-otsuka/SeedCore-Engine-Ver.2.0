#include <FoundationEngine/Resource/IconConfig.h>
#include <FoundationEngine/Serialization/Binary/BinaryArchive.h>
#include <FoundationEngine/Log/Warning.h>

namespace SeedCore
{
	/**
	* [EN]
	* Loads icon_ from path. Leaves icon_ untouched if the file does not
	* exist yet (no icon has ever been chosen).
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* path から icon_ を読み込む。ファイルがまだ存在しない（一度も
	* アイコンが選ばれていない）場合は icon_ を変更しない。
	*/
	void IconConfig::Load(const std::filesystem::path& path)
	{
		/// [EN] Checked up front so a project that never picked an icon does not log a missing-file warning.
		/// [JP] 一度もアイコンを選んでいないプロジェクトでファイル無しの警告を出さないよう、先に確認する。
		if (!std::filesystem::exists(path))
		{
			return;
		}

		BinaryInputArchive archive;
		if (!archive.Read(String(path.string())))
		{
			return;
		}

		archive.TryField("icon", icon_);
	}

	/**
	* [EN]
	* Writes icon_ to path, creating the parent directory if needed.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* icon_ を path へ書き込む。必要なら親ディレクトリを作成する。
	*/
	void IconConfig::Save(const std::filesystem::path& path)const
	{
		if (path.has_parent_path())
		{
			std::filesystem::create_directories(path.parent_path());
		}

		BinaryOutputArchive archive;
		archive.Field("icon", icon_);
		archive.Write(String(path.string()));
	}

	/**
	* [EN]
	* Replaces icon_ with an icon built from imagePath: a .ico is taken
	* as-is, any other WIC-readable image is squared, scaled to every icon
	* size and packed into a new .ico with one PNG entry per size.
	* Returns false and leaves icon_ untouched on failure.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* icon_ を imagePath から作ったアイコンで置き換える: .ico はそのまま
	* 取り込み、それ以外の WIC で読める画像は正方形にして各アイコン
	* サイズへ縮小し、サイズごとに1つの PNG エントリを持つ新しい .ico に
	* まとめる。失敗時は false を返し、icon_ は変更しない。
	*/
	Bool IconConfig::Import(const std::filesystem::path& imagePath)
	{
		std::string extension = imagePath.extension().string();
		std::ranges::transform(extension, extension.begin(), [](Char character) { return static_cast<Char>(std::tolower(static_cast<Uint8>(character))); });

		/// [EN] An existing .ico already carries its own set of sizes, so it is kept byte for byte after its ICONDIR header (reserved = 0, type = 1 for icons, at least one entry) is validated.
		/// [JP] 既存の .ico は自身のサイズ一式を既に持っているので、ICONDIR ヘッダ（reserved = 0、アイコンは type = 1、エントリ1つ以上）を確認したうえでバイト単位でそのまま保持する。
		if (extension == ".ico")
		{
			std::ifstream stream(imagePath, std::ios::binary);
			DynamicArray<Byte> data((std::istreambuf_iterator<Char>(stream)), std::istreambuf_iterator<Char>());

			Uint16 reserved = 0;
			Uint16 type = 0;
			Uint16 count = 0;
			if (data.size() >= 6)
			{
				std::memcpy(&reserved, data.data(), sizeof(Uint16));
				std::memcpy(&type, data.data() + 2, sizeof(Uint16));
				std::memcpy(&count, data.data() + 4, sizeof(Uint16));
			}

			if (reserved != 0 || type != 1 || count == 0 || data.size() < 6 + static_cast<Size>(count) * 16)
			{
				SC_LOG_WARNING("アイコンファイルの形式が不正です: {}", imagePath.string());
				return false;
			}

			icon_ = std::move(data);
			return true;
		}

		/// [EN] WIC is a COM API; initialize COM on this thread only if nobody has yet, and balance only our own initialization.
		/// [JP] WIC は COM API なので、このスレッドでまだ誰も COM を初期化していない場合だけ初期化し、自分が行った初期化分だけ後で解除する。
		Bool comInitializedHere = SUCCEEDED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED));

		DynamicArray<Byte> icon;
		Bool success = false;

		do
		{
			/// [EN] IGNORE_SRGB keeps the pixel values as stored; an icon is displayed exactly as authored, so no gamma conversion may happen on the way to the .ico.
			/// [JP] IGNORE_SRGB で保存されている画素値をそのまま扱う。アイコンは作られたとおりに表示されるものなので、.ico へ至る途中でガンマ変換を挟んではならない。
			DirectX::ScratchImage loadedImage;
			if (FAILED(DirectX::LoadFromWICFile(imagePath.wstring().c_str(), DirectX::WIC_FLAGS_IGNORE_SRGB, nullptr, loadedImage)))
			{
				SC_LOG_WARNING("アイコン用の画像を読み込めませんでした: {}", imagePath.string());
				break;
			}

			DirectX::ScratchImage rgbaImage;
			const DirectX::Image* sourceImage = loadedImage.GetImage(0, 0, 0);
			if (sourceImage->format != DXGI_FORMAT_R8G8B8A8_UNORM)
			{
				if (FAILED(DirectX::Convert(*sourceImage, DXGI_FORMAT_R8G8B8A8_UNORM, DirectX::TEX_FILTER_DEFAULT, DirectX::TEX_THRESHOLD_DEFAULT, rgbaImage)))
				{
					SC_LOG_WARNING("アイコン用の画像を RGBA に変換できませんでした: {}", imagePath.string());
					break;
				}
				sourceImage = rgbaImage.GetImage(0, 0, 0);
			}

			/// [EN] Icons are square. The image is centered on a fully transparent square whose side is its longer edge, so nothing is cropped or stretched.
			/// [JP] アイコンは正方形。画像を、長い方の辺を一辺とする完全に透明な正方形の中央に置くので、切り取りも引き伸ばしも起きない。
			Size side = Max(sourceImage->width, sourceImage->height);

			DirectX::ScratchImage squareImage;
			if (FAILED(squareImage.Initialize2D(DXGI_FORMAT_R8G8B8A8_UNORM, side, side, 1, 1)))
			{
				break;
			}
			std::memset(squareImage.GetPixels(), 0, squareImage.GetPixelsSize());

			if (FAILED(DirectX::CopyRectangle(*sourceImage, DirectX::Rect(0, 0, sourceImage->width, sourceImage->height), *squareImage.GetImage(0, 0, 0), DirectX::TEX_FILTER_DEFAULT, (side - sourceImage->width) / 2, (side - sourceImage->height) / 2)))
			{
				break;
			}

			/// [EN] The standard Windows icon sizes: 16/24/32/48 for small views, title bars and the taskbar, 64/128/256 for large Explorer views and high-DPI displays.
			/// [JP] Windows の標準アイコンサイズ: 16/24/32/48 は小さい表示・タイトルバー・タスクバー、64/128/256 はエクスプローラーの大きい表示と高 DPI 向け。
			constexpr Uint32 iconSizes[] = { 256, 128, 64, 48, 32, 24, 16 };

			DynamicArray<DynamicArray<Byte>> pngImages;
			for (Uint32 iconSize : iconSizes)
			{
				/// [EN] Fant is WIC's area-averaging filter, which stays clean even when a large source is reduced to 16 pixels in one step.
				/// [JP] Fant は WIC の面積平均フィルタで、大きな元画像を一度に 16 ピクセルまで縮小してもきれいに保てる。
				DirectX::ScratchImage resizedImage;
				if (FAILED(DirectX::Resize(*squareImage.GetImage(0, 0, 0), iconSize, iconSize, DirectX::TEX_FILTER_FANT, resizedImage)))
				{
					break;
				}

				DirectX::Blob pngBlob;
				if (FAILED(DirectX::SaveToWICMemory(*resizedImage.GetImage(0, 0, 0), DirectX::WIC_FLAGS_NONE, DirectX::GetWICCodec(DirectX::WIC_CODEC_PNG), pngBlob)))
				{
					break;
				}

				const Byte* pngBegin = reinterpret_cast<const Byte*>(pngBlob.GetBufferPointer());
				pngImages.push_back(DynamicArray<Byte>(pngBegin, pngBegin + pngBlob.GetBufferSize()));
			}

			if (pngImages.size() != std::size(iconSizes))
			{
				SC_LOG_WARNING("アイコンの縮小/PNG 化に失敗しました: {}", imagePath.string());
				break;
			}

			/// [EN] Appends a little-endian integer of the given width, the byte order of every .ico header field.
			/// [JP] 指定幅のリトルエンディアン整数を追加する。.ico のヘッダの各フィールドはすべてこのバイト順。
			auto append = [&icon](Uint32 value, Size byteCount)
			{
				for (Size byteIndex = 0; byteIndex < byteCount; ++byteIndex)
				{
					icon.push_back(static_cast<Byte>((value >> (byteIndex * 8)) & 0xFF));
				}
			};

			/// [EN] ICONDIR: reserved (0), resource type (1 = icon), image count.
			/// [JP] ICONDIR: reserved（0）、リソース種別（1 = アイコン）、画像数。
			append(0, 2);
			append(1, 2);
			append(static_cast<Uint32>(pngImages.size()), 2);

			/// [EN] One 16-byte ICONDIRENTRY per image. A width/height byte of 0 means 256, and the image data follows right after the whole directory.
			/// [JP] 画像ごとに 16 バイトの ICONDIRENTRY を1つ。幅/高さのバイトが 0 なら 256 を意味し、画像データはディレクトリ全体の直後に続く。
			Uint32 imageOffset = static_cast<Uint32>(6 + pngImages.size() * 16);
			for (Size imageIndex = 0; imageIndex < pngImages.size(); ++imageIndex)
			{
				Uint32 iconSize = iconSizes[imageIndex];
				append(iconSize >= 256 ? 0 : iconSize, 1);
				append(iconSize >= 256 ? 0 : iconSize, 1);
				append(0, 1);
				append(0, 1);
				append(1, 2);
				append(32, 2);
				append(static_cast<Uint32>(pngImages[imageIndex].size()), 4);
				append(imageOffset, 4);
				imageOffset += static_cast<Uint32>(pngImages[imageIndex].size());
			}

			for (const DynamicArray<Byte>& pngImage : pngImages)
			{
				icon.insert(icon.end(), pngImage.begin(), pngImage.end());
			}

			success = true;
		} while (false);

		if (comInitializedHere)
		{
			CoUninitialize();
		}

		if (!success)
		{
			return false;
		}

		icon_ = std::move(icon);
		return true;
	}
}
