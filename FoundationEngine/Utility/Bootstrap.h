#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	/**
	* [EN]
	* Startup configuration handed to the engine before the main window
	* and graphics device are created.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* メインウィンドウとグラフィックスデバイスが作成される前に、
	* エンジンへ渡される起動時設定。
	*/
	struct Bootstrap
	{
		/**
		* [EN]
		* Initial main window parameters.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* メインウィンドウの初期パラメータ。
		*/
		struct Window
		{
			/// [EN] Initial client width, in pixels.
			/// [JP] 初期クライアント幅（ピクセル）。
			Uint32 Width_ = 1920;

			/// [EN] Initial client height, in pixels.
			/// [JP] 初期クライアント高さ（ピクセル）。
			Uint32 Height_ = 1080;

			/// [EN] Whether to start in fullscreen mode.
			/// [JP] フルスクリーンモードで開始するかどうか。
			Bool Fullscreen_ = false;

			/// [EN] Window title text.
			/// [JP] ウィンドウタイトルの文字列。
			const Wchar* Title_ = L"SeedCore Engine";
		};

		/// [EN] Initial main window configuration.
		/// [JP] メインウィンドウの初期設定。
		Window WindowDesc_;
	};
}