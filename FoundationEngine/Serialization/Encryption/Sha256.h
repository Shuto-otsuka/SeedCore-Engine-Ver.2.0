#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	/**
	* [EN]
	* From-scratch SHA-256 (FIPS 180-4) cryptographic hash. Used solely to
	* derive the fixed 32-byte AES-256 key from SC_ENCRYPTION_KEY_SEED (see
	* Prelude.h) - hashing a short GUID-like string into a full-length key
	* rather than truncating/repeating it. No external crypto library or OS
	* API dependency, matching Aes256's own from-scratch implementation.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 自前実装のSHA-256(FIPS 180-4)暗号学的ハッシュ。SC_ENCRYPTION_KEY_SEED
	* (Prelude.h参照)から固定32バイトのAES-256鍵を導出する目的だけに使う -
	* 短いGUID風文字列を、切り詰めたり繰り返したりせずフル長の鍵へハッシュ
	* 化する。Aes256自身の自前実装と同様、外部の暗号ライブラリやOS APIには
	* 依存しない。
	*/
	class SEEDCORE_API Sha256
	{
	public:
		/**
		* [EN]
		* Hashes size bytes starting at data and returns the 32-byte digest.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* dataから始まるsizeバイトをハッシュ化し、32バイトのダイジェストを
		* 返す。
		*/
		static DynamicArray<Byte> Hash(const Byte* data, Size size);

		/**
		* [EN]
		* Hashes the entire contents of data and returns the 32-byte digest.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* dataの内容全体をハッシュ化し、32バイトのダイジェストを返す。
		*/
		static DynamicArray<Byte> Hash(const DynamicArray<Byte>& data);

	private:
		/**
		* [EN]
		* Compresses one 64-byte message block into the running 8-word (32
		* bytes) hash state, per FIPS 180-4's message schedule expansion and
		* 64-round compression function.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* FIPS 180-4のメッセージスケジュール拡張と64ラウンドの圧縮関数に
		* 従い、64バイトのメッセージブロック1つを、進行中の8ワード(32バイト)
		* ハッシュ状態へ圧縮する。
		*/
		static void ProcessBlock(Uint32 state[8], const Byte block[64]);
	};
}
