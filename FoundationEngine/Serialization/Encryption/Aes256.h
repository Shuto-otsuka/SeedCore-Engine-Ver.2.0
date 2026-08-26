#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	/**
	* [EN]
	* AES-256 (FIPS-197) block cipher in CBC mode with PKCS7 padding, used by
	* every on-disk cache this engine encrypts (BinaryArchive, TextureLoader's
	* ".icon"/".logo"/".texture" caches, ShaderCompiler's ".dx.cso" cache,
	* MovieByteStream's ".movie" cache). Small buffers (below
	* hardwareThreshold_) run through this class's own from-scratch key
	* schedule/SubBytes/ShiftRows/MixColumns implementation; buffers at or
	* above the threshold are handed to Windows' built-in BCrypt (hardware
	* AES-NI where available) purely for throughput on large assets - both
	* paths are the same standard AES-256-CBC-PKCS7 and produce byte-
	* identical output, so a file encrypted by one path decrypts correctly
	* through the other.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* CBCモード+PKCS7パディングのAES-256(FIPS-197)ブロック暗号。このエンジンが
	* 暗号化する全てのディスク上キャッシュ(BinaryArchive、TextureLoaderの
	* ".icon"/".logo"/".texture"キャッシュ、ShaderCompilerの".dx.cso"
	* キャッシュ、MovieByteStreamの".movie"キャッシュ)から使われる。小さい
	* バッファ(hardwareThreshold_未満)はこのクラス自前の鍵スケジュール/
	* SubBytes/ShiftRows/MixColumns実装を通り、閾値以上のバッファは大きい
	* アセットのスループットのためだけにWindows標準のBCrypt(利用可能なら
	* ハードウェアAES-NI)へ渡す - どちらの経路も同じ標準AES-256-CBC-PKCS7で
	* あり出力はバイト単位で同一なため、片方の経路で暗号化したファイルを
	* もう片方の経路で正しく復号できる。
	*/
	class SEEDCORE_API Aes256
	{
	public:
		/**
		* [EN]
		* CBC-encrypts plaintext with PKCS7 padding. iv must be 16 bytes and
		* key must be 32 bytes (AES-256). Returns the ciphertext, always a
		* multiple of 16 bytes since PKCS7 always adds at least one byte of
		* padding (a full extra block when plaintext is already block-sized).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* PKCS7パディング付きでplaintextをCBC暗号化する。ivは16バイト、keyは
		* 32バイト(AES-256)である必要がある。PKCS7は常に最低1バイトの
		* パディングを追加する(plaintextが既にブロックサイズの倍数なら
		* まるまる1ブロック追加される)ため、返す暗号文は常に16バイトの倍数。
		*/
		static DynamicArray<Byte> Encrypt(const DynamicArray<Byte>& key, const DynamicArray<Byte>& iv, const DynamicArray<Byte>& plaintext);

		/**
		* [EN]
		* CBC-decrypts a whole ciphertext produced by Encrypt() and strips its
		* trailing PKCS7 padding. Returns an empty array if the padding is
		* invalid (wrong key/iv, corrupted data, or ciphertext not a multiple
		* of 16 bytes) - callers treat that as a decrypt failure/cache miss.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* Encrypt()が生成した暗号文全体をCBC復号し、末尾のPKCS7パディングを
		* 除去する。パディングが不正(鍵/IV違い、データ破損、暗号文が16バイトの
		* 倍数でない等)なら空配列を返す - 呼び出し側はこれを復号失敗/
		* キャッシュミス扱いにする。
		*/
		static DynamicArray<Byte> Decrypt(const DynamicArray<Byte>& key, const DynamicArray<Byte>& iv, const DynamicArray<Byte>& ciphertext);

		/**
		* [EN]
		* CBC-decrypts an arbitrary (not necessarily whole-file) block-
		* aligned ciphertext range without stripping PKCS7 padding - for
		* random-access decryption of a sub-range, the final block of the
		* ciphertext passed in isn't necessarily the file's actual final
		* block, so the padding byte at the end can't be trusted.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* PKCS7パディングを除去しない、任意の(ファイル全体とは限らない)
		* ブロック境界揃いの暗号文範囲のCBC復号 - 部分範囲のランダム
		* アクセス復号では、渡された暗号文の最後のブロックが必ずしもファイル
		* 本来の最終ブロックとは限らないため、末尾のパディングバイトを
		* 信用できない。
		*/
		static DynamicArray<Byte> DecryptUnpadded(const DynamicArray<Byte>& key, const DynamicArray<Byte>& iv, const DynamicArray<Byte>& ciphertext);

	private:
		/// [EN] Buffers at or above this size use BCrypt (EncryptHardware/
		///      DecryptUnpaddedHardware) instead of the from-scratch software
		///      path, since AES-NI throughput matters far more than
		///      dependency-freedom once an asset is this large.
		/// [JP] このサイズ以上のバッファは、自前のソフトウェア経路ではなく
		///      BCrypt(EncryptHardware/DecryptUnpaddedHardware)を使う -
		///      アセットがこの大きさになると、依存ゼロであることよりAES-NIの
		///      スループットの方が重要になるため。
		static constexpr Size hardwareThreshold_ = 1024 * 1024;

		/**
		* [EN]
		* CBC-encrypts already-padded, block-aligned data via this class's
		* own from-scratch AES-256 (EncryptBlock).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 既にパディング済み・ブロック境界揃いのデータを、このクラス自前の
		* AES-256(EncryptBlock)でCBC暗号化する。
		*/
		static DynamicArray<Byte> EncryptSoftware(const DynamicArray<Byte>& key, const DynamicArray<Byte>& iv, const DynamicArray<Byte>& paddedPlaintext);

		/**
		* [EN]
		* CBC-encrypts already-padded, block-aligned data via Windows' BCrypt
		* (no BCRYPT_BLOCK_PADDING flag - the data is already padded, so
		* BCrypt is only asked to run the raw cipher).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 既にパディング済み・ブロック境界揃いのデータを、Windows標準のBCrypt
		* でCBC暗号化する(BCRYPT_BLOCK_PADDINGフラグは付けない - データは
		* 既にパディング済みなので、BCryptには生の暗号処理だけを行わせる)。
		*/
		static DynamicArray<Byte> EncryptHardware(const DynamicArray<Byte>& key, const DynamicArray<Byte>& iv, const DynamicArray<Byte>& paddedPlaintext);

		/**
		* [EN]
		* CBC-decrypts a block-aligned ciphertext range via this class's own
		* from-scratch AES-256 (DecryptBlock), without stripping padding.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* ブロック境界揃いの暗号文範囲を、このクラス自前のAES-256
		* (DecryptBlock)でCBC復号する。パディングは除去しない。
		*/
		static DynamicArray<Byte> DecryptUnpaddedSoftware(const DynamicArray<Byte>& key, const DynamicArray<Byte>& iv, const DynamicArray<Byte>& ciphertext);

		/**
		* [EN]
		* CBC-decrypts a block-aligned ciphertext range via Windows' BCrypt,
		* without stripping padding (no BCRYPT_BLOCK_PADDING flag).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* ブロック境界揃いの暗号文範囲を、Windows標準のBCryptでCBC復号する。
		* パディングは除去しない(BCRYPT_BLOCK_PADDINGフラグは付けない)。
		*/
		static DynamicArray<Byte> DecryptUnpaddedHardware(const DynamicArray<Byte>& key, const DynamicArray<Byte>& iv, const DynamicArray<Byte>& ciphertext);

		/**
		* [EN]
		* Expands the 32-byte AES-256 key into all 15 round keys (16 bytes
		* each) via the Rijndael key schedule (RotWord/SubWord every 8 words,
		* SubWord alone every 4th word in between).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* Rijndaelの鍵スケジュール(8ワードごとにRotWord/SubWord、その間の4
		* ワード目はSubWordのみ)で、32バイトのAES-256鍵を全15ラウンド鍵
		* (各16バイト)に展開する。
		*/
		static void ExpandKey(const Byte* key, Uint8 roundKeys[15][16]);

		/**
		* [EN]
		* Encrypts a single 16-byte block: AddRoundKey, then 13 full rounds
		* (SubBytes/ShiftRows/MixColumns/AddRoundKey), then a final round
		* without MixColumns.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 16バイト1ブロックを暗号化する: AddRoundKeyの後、13回のフルラウンド
		* (SubBytes/ShiftRows/MixColumns/AddRoundKey)、最後にMixColumnsを
		* 含まない最終ラウンド。
		*/
		static void EncryptBlock(const Byte* input, Byte* output, const Uint8 roundKeys[15][16]);

		/**
		* [EN]
		* Decrypts a single 16-byte block: the inverse cipher, applying
		* InvShiftRows/InvSubBytes/AddRoundKey/InvMixColumns in reverse round
		* order.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 16バイト1ブロックを復号する: 逆順のラウンドでInvShiftRows/
		* InvSubBytes/AddRoundKey/InvMixColumnsを適用する逆暗号。
		*/
		static void DecryptBlock(const Byte* input, Byte* output, const Uint8 roundKeys[15][16]);
	};
}
