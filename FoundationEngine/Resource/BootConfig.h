#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/Math/Vector.h>

namespace SeedCore
{
	/**
	* [EN]
	* The point of the screen, and the matching point of the progress bar,
	* that the bar is placed by. The bar's own point of the same name sits
	* on the screen's point, then BootConfig::offset_ moves it from there -
	* so BottomRight keeps the bar's bottom-right corner a fixed distance
	* from the screen's bottom-right corner at any aspect ratio.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 進捗バーを配置する基準となる、画面上の点とバー上の対応する点。バーの
	* 同名の点を画面の点に合わせ、そこから BootConfig::offset_ だけ動かす。
	* そのため BottomRight なら、どの画面比率でもバーの右下隅が画面の右下隅
	* から一定の距離に保たれる。
	*/
	enum class BootAnchor
	{
		TopLeft,
		Top,
		TopRight,
		Left,
		Center,
		Right,
		BottomLeft,
		Bottom,
		BottomRight,
	};

	/**
	* [EN]
	* How the progress bar fills as loading advances, matching Unity's
	* Image.FillMethod. Horizontal/Vertical reveal the bar in a straight
	* line; Radial90/180/360 sweep it around a pivot at a corner, an edge
	* center or the middle of the bar.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* ロードの進行に合わせた進捗バーの塗り方。Unity の Image.FillMethod と
	* 同じ。Horizontal/Vertical はバーを直線的に見せていき、Radial90/180/360
	* はバーの角・辺の中点・中心を軸に扇状に塗っていく。
	*/
	enum class BootFillMethod
	{
		Horizontal,
		Vertical,
		Radial90,
		Radial180,
		Radial360,
	};

	/**
	* [EN]
	* The look of the loading screen shown while the game starts up (after
	* the splash logos, until the first scene is ready). It is made of three
	* images - a background filling the screen, a progress bar revealed by
	* the load progress, and a frame drawn over the bar in the same
	* rectangle - plus their placement, colors and fill motion. Images are
	* stored as the picked file's bytes inside BootConfig.scg, so the export
	* does not depend on the source files staying where they were picked
	* from. An empty image means the engine's default image
	* (Runtime/Logo/ProgressBackground/ProgressBar/ProgressFrame.sub.logo).
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* ゲーム起動時（スプラッシュのロゴの後、最初のシーンの準備ができるまで）
	* に表示するローディング画面の見た目。画面いっぱいの背景、ロードの進捗で
	* 見えていく進捗バー、バーと同じ矩形に重ねる枠の3枚の画像と、その配置・
	* 色・塗りの動きからなる。画像は選択したファイルの中身を BootConfig.scg
	* の中に保存するので、書き出しは元ファイルが選択した場所に残っているか
	* どうかに依存しない。画像が空ならエンジン既定の画像
	* （Runtime/Logo/ProgressBackground/ProgressBar/ProgressFrame.sub.logo）
	* を使う。
	*/
	struct SEEDCORE_API BootConfig
	{
		/// [EN] The background image file's bytes, or empty for the default image. Scaled to cover the whole screen, cropping whichever side overflows.
		/// [JP] 背景画像ファイルの中身。空なら既定の画像。画面全体を覆うように拡大し、はみ出す側は切り取る。
		DynamicArray<Byte> backgroundImage_;

		/// [EN] The progress bar image file's bytes, or empty for the default image. Its aspect ratio decides the bar's height.
		/// [JP] 進捗バー画像ファイルの中身。空なら既定の画像。この縦横比でバーの高さが決まる。
		DynamicArray<Byte> barImage_;

		/// [EN] The frame image file's bytes, or empty for the default image. Stretched onto the bar's rectangle, so it should share the bar image's aspect ratio.
		/// [JP] 枠画像ファイルの中身。空なら既定の画像。バーの矩形に合わせて伸縮するので、バー画像と同じ縦横比で作る。
		DynamicArray<Byte> frameImage_;

		/// [EN] Whether the background image is drawn. When false the background is backgroundColor_ alone.
		/// [JP] 背景画像を描くか。false なら背景は backgroundColor_ の単色になる。
		Bool useBackgroundImage_ = true;

		/// [EN] Whether the frame image is drawn over the bar.
		/// [JP] バーの上に枠画像を描くか。
		Bool useFrame_ = true;

		/// [EN] The solid background color, shown alone when useBackgroundImage_ is false and under the background image's transparent parts otherwise.
		/// [JP] 背景の単色。useBackgroundImage_ が false ならこの色だけを表示し、それ以外では背景画像の透明部分の下に見える。
		Color backgroundColor_ = Color(0.0f, 0.0f, 0.0f, 1.0f);

		/// [EN] Color multiplied onto the background image; its alpha fades the image toward backgroundColor_.
		/// [JP] 背景画像に乗算する色。アルファで画像を backgroundColor_ へ向けて薄くする。
		Color backgroundTint_ = Color(1.0f, 1.0f, 1.0f, 1.0f);

		/// [EN] Color multiplied onto the progress bar image.
		/// [JP] 進捗バー画像に乗算する色。
		Color barTint_ = Color(1.0f, 1.0f, 1.0f, 1.0f);

		/// [EN] Color multiplied onto the frame image.
		/// [JP] 枠画像に乗算する色。
		Color frameTint_ = Color(1.0f, 1.0f, 1.0f, 1.0f);

		/// [EN] The screen point, and matching bar point, the bar is placed by.
		/// [JP] バーを配置する基準の、画面上の点とバー上の対応する点。
		BootAnchor anchor_ = BootAnchor::BottomRight;

		/// [EN] How far the bar's anchor point sits from the screen's anchor point, as a fraction of the screen's width (x) and height (y). +x is right, +y is down.
		/// [JP] バーの基準点を画面の基準点からどれだけ離すか。画面の幅（x）と高さ（y）に対する割合。+x が右、+y が下。
		Vector2 offset_ = Vector2(-0.028125f, -0.05f);

		/// [EN] The bar's width as a fraction of the screen's width. Its height follows from the bar image's aspect ratio.
		/// [JP] 画面の幅に対するバーの幅の割合。高さはバー画像の縦横比から決まる。
		Float width_ = 0.4f;

		/// [EN] How the bar fills as loading advances.
		/// [JP] ロードの進行に合わせたバーの塗り方。
		BootFillMethod fillMethod_ = BootFillMethod::Horizontal;

		/// [EN] Where the fill starts, as Unity's Image.fillOrigin. Horizontal: 0 Left, 1 Right. Vertical: 0 Bottom, 1 Top. Radial90: 0 BottomLeft, 1 TopLeft, 2 TopRight, 3 BottomRight. Radial180: 0 Bottom, 1 Left, 2 Top, 3 Right. Radial360: 0 Bottom, 1 Right, 2 Top, 3 Left.
		/// [JP] 塗りの開始位置。Unity の Image.fillOrigin と同じ。Horizontal: 0 左、1 右。Vertical: 0 下、1 上。Radial90: 0 左下、1 左上、2 右上、3 右下。Radial180: 0 下、1 左、2 上、3 右。Radial360: 0 下、1 右、2 上、3 左。
		Int32 fillOrigin_ = 0;

		/// [EN] Whether a radial fill sweeps clockwise. Ignored by Horizontal/Vertical.
		/// [JP] 扇状の塗りを時計回りに進めるか。Horizontal/Vertical では使わない。
		Bool clockwise_ = true;

		/// [EN] Whether the leading edge of a Horizontal/Vertical fill ripples like a water surface. Ignored by radial fills.
		/// [JP] Horizontal/Vertical の塗りの先端を水面のように波打たせるか。扇状の塗りでは使わない。
		Bool wave_ = true;

		/**
		* [EN]
		* Loads the settings from path. Leaves every field at its current
		* value if the file does not exist yet (the loading screen has never
		* been edited).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* path から設定を読み込む。ファイルがまだ存在しない（ローディング
		* 画面を一度も編集していない）場合は、どのフィールドも変更しない。
		*/
		void Load(const std::filesystem::path& path = "../UserProject/Assets/Config/BootConfig.scg");

		/**
		* [EN]
		* Writes the settings to path, creating the parent directory if needed.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 設定を path へ書き込む。必要なら親ディレクトリを作成する。
		*/
		void Save(const std::filesystem::path& path = "../UserProject/Assets/Config/BootConfig.scg")const;

		/**
		* [EN]
		* Reads imagePath into image after checking that it is an image the
		* engine can decode (DDS, or anything WIC reads such as PNG/JPEG/BMP).
		* Returns false and leaves image untouched otherwise.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* imagePath がエンジンで読める画像（DDS、または PNG/JPEG/BMP など WIC
		* で読めるもの）であることを確認したうえで、その中身を image へ読み
		* 込む。そうでなければ false を返し、image は変更しない。
		*/
		static Bool Import(const std::filesystem::path& imagePath, DynamicArray<Byte>& image);
	};
}
