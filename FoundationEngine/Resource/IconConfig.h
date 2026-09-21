#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	/**
	* [EN]
	* The application icon embedded into the exported game's executables
	* (Launcher.exe and Runtime.exe). Holds a complete .ico file image and
	* persists it inside IconBindings.scg, so the export does not depend
	* on the source image staying where it was picked from. An empty icon_
	* means no icon was chosen, and the export falls back to the engine's
	* default icon (Runtime/Logo/SeedCore.ico).
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 書き出したゲームの実行ファイル（Launcher.exe と Runtime.exe）に
	* 埋め込むアプリケーションアイコン。.ico ファイルの中身をまるごと
	* 保持し、IconBindings.scg の中に保存する。これにより、書き出しは
	* 元画像が選択した場所に残っているかどうかに依存しない。icon_ が空の
	* 場合はアイコン未指定を意味し、書き出しはエンジン既定のアイコン
	* （Runtime/Logo/SeedCore.ico）を使う。
	*/
	struct SEEDCORE_API IconConfig
	{
		/// [EN] The whole .ico file image (ICONDIR + entries + PNG payloads), or empty when no icon has been chosen.
		/// [JP] .ico ファイルの中身全体（ICONDIR + エントリ + PNG 本体）。アイコン未指定なら空。
		DynamicArray<Byte> icon_;

		/**
		* [EN]
		* Loads icon_ from path. Leaves icon_ untouched if the file does
		* not exist yet (no icon has ever been chosen).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* path から icon_ を読み込む。ファイルがまだ存在しない（一度も
		* アイコンが選ばれていない）場合は icon_ を変更しない。
		*/
		void Load(const std::filesystem::path& path = "../UserProject/Assets/Config/IconBindings.scg");

		/**
		* [EN]
		* Writes icon_ to path, creating the parent directory if needed.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* icon_ を path へ書き込む。必要なら親ディレクトリを作成する。
		*/
		void Save(const std::filesystem::path& path = "../UserProject/Assets/Config/IconBindings.scg")const;

		/**
		* [EN]
		* Replaces icon_ with an icon built from imagePath. A .ico file is
		* taken as-is after checking its header. Any other image readable
		* by WIC (.png/.jpg/.bmp, ...) is centered on a transparent square
		* canvas (so non-square images keep their aspect ratio), scaled to
		* 256/128/64/48/32/24/16 pixels, and each size is stored as a PNG
		* entry of a new .ico - the form Windows uses to pick a crisp image
		* for every place an icon is shown. Returns false and leaves icon_
		* untouched if the image cannot be read or converted.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* icon_ を imagePath から作ったアイコンで置き換える。.ico ファイルは
		* ヘッダを確認したうえでそのまま取り込む。それ以外の WIC で読める
		* 画像（.png/.jpg/.bmp など）は、透明な正方形キャンバスの中央に
		* 配置し（正方形でない画像も縦横比を保つ）、256/128/64/48/32/24/16
		* ピクセルへ縮小して、各サイズを新しい .ico の PNG エントリとして
		* 格納する - Windows がアイコンを表示するそれぞれの場所で、くっきり
		* した画像を選べる形式。画像を読めない/変換できない場合は false を
		* 返し、icon_ は変更しない。
		*/
		Bool Import(const std::filesystem::path& imagePath);
	};
}
