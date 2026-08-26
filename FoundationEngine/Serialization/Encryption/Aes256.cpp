#include <FoundationEngine/Serialization/Encryption/Aes256.h>
#include <bcrypt.h>
#pragma comment(lib, "bcrypt.lib")

namespace SeedCore
{
	namespace
	{
		/// [EN] Rijndael's fixed S-box: the multiplicative inverse over
		///      GF(2^8) followed by a fixed affine transform, used by
		///      SubBytes/ExpandKey.
		/// [JP] Rijndael固定のS-box: GF(2^8)上の乗法逆元に固定のアフィン
		///      変換を施したもの。SubBytes/ExpandKeyで使う。
		constexpr Uint8 sbox[256] =
		{
			0x63,0x7c,0x77,0x7b,0xf2,0x6b,0x6f,0xc5,0x30,0x01,0x67,0x2b,0xfe,0xd7,0xab,0x76,
			0xca,0x82,0xc9,0x7d,0xfa,0x59,0x47,0xf0,0xad,0xd4,0xa2,0xaf,0x9c,0xa4,0x72,0xc0,
			0xb7,0xfd,0x93,0x26,0x36,0x3f,0xf7,0xcc,0x34,0xa5,0xe5,0xf1,0x71,0xd8,0x31,0x15,
			0x04,0xc7,0x23,0xc3,0x18,0x96,0x05,0x9a,0x07,0x12,0x80,0xe2,0xeb,0x27,0xb2,0x75,
			0x09,0x83,0x2c,0x1a,0x1b,0x6e,0x5a,0xa0,0x52,0x3b,0xd6,0xb3,0x29,0xe3,0x2f,0x84,
			0x53,0xd1,0x00,0xed,0x20,0xfc,0xb1,0x5b,0x6a,0xcb,0xbe,0x39,0x4a,0x4c,0x58,0xcf,
			0xd0,0xef,0xaa,0xfb,0x43,0x4d,0x33,0x85,0x45,0xf9,0x02,0x7f,0x50,0x3c,0x9f,0xa8,
			0x51,0xa3,0x40,0x8f,0x92,0x9d,0x38,0xf5,0xbc,0xb6,0xda,0x21,0x10,0xff,0xf3,0xd2,
			0xcd,0x0c,0x13,0xec,0x5f,0x97,0x44,0x17,0xc4,0xa7,0x7e,0x3d,0x64,0x5d,0x19,0x73,
			0x60,0x81,0x4f,0xdc,0x22,0x2a,0x90,0x88,0x46,0xee,0xb8,0x14,0xde,0x5e,0x0b,0xdb,
			0xe0,0x32,0x3a,0x0a,0x49,0x06,0x24,0x5c,0xc2,0xd3,0xac,0x62,0x91,0x95,0xe4,0x79,
			0xe7,0xc8,0x37,0x6d,0x8d,0xd5,0x4e,0xa9,0x6c,0x56,0xf4,0xea,0x65,0x7a,0xae,0x08,
			0xba,0x78,0x25,0x2e,0x1c,0xa6,0xb4,0xc6,0xe8,0xdd,0x74,0x1f,0x4b,0xbd,0x8b,0x8a,
			0x70,0x3e,0xb5,0x66,0x48,0x03,0xf6,0x0e,0x61,0x35,0x57,0xb9,0x86,0xc1,0x1d,0x9e,
			0xe1,0xf8,0x98,0x11,0x69,0xd9,0x8e,0x94,0x9b,0x1e,0x87,0xe9,0xce,0x55,0x28,0xdf,
			0x8c,0xa1,0x89,0x0d,0xbf,0xe6,0x42,0x68,0x41,0x99,0x2d,0x0f,0xb0,0x54,0xbb,0x16,
		};

		/// [EN] The inverse of sbox, used by InvSubBytes.
		/// [JP] sboxの逆写像。InvSubBytesで使う。
		constexpr Uint8 invSbox[256] =
		{
			0x52,0x09,0x6a,0xd5,0x30,0x36,0xa5,0x38,0xbf,0x40,0xa3,0x9e,0x81,0xf3,0xd7,0xfb,
			0x7c,0xe3,0x39,0x82,0x9b,0x2f,0xff,0x87,0x34,0x8e,0x43,0x44,0xc4,0xde,0xe9,0xcb,
			0x54,0x7b,0x94,0x32,0xa6,0xc2,0x23,0x3d,0xee,0x4c,0x95,0x0b,0x42,0xfa,0xc3,0x4e,
			0x08,0x2e,0xa1,0x66,0x28,0xd9,0x24,0xb2,0x76,0x5b,0xa2,0x49,0x6d,0x8b,0xd1,0x25,
			0x72,0xf8,0xf6,0x64,0x86,0x68,0x98,0x16,0xd4,0xa4,0x5c,0xcc,0x5d,0x65,0xb6,0x92,
			0x6c,0x70,0x48,0x50,0xfd,0xed,0xb9,0xda,0x5e,0x15,0x46,0x57,0xa7,0x8d,0x9d,0x84,
			0x90,0xd8,0xab,0x00,0x8c,0xbc,0xd3,0x0a,0xf7,0xe4,0x58,0x05,0xb8,0xb3,0x45,0x06,
			0xd0,0x2c,0x1e,0x8f,0xca,0x3f,0x0f,0x02,0xc1,0xaf,0xbd,0x03,0x01,0x13,0x8a,0x6b,
			0x3a,0x91,0x11,0x41,0x4f,0x67,0xdc,0xea,0x97,0xf2,0xcf,0xce,0xf0,0xb4,0xe6,0x73,
			0x96,0xac,0x74,0x22,0xe7,0xad,0x35,0x85,0xe2,0xf9,0x37,0xe8,0x1c,0x75,0xdf,0x6e,
			0x47,0xf1,0x1a,0x71,0x1d,0x29,0xc5,0x89,0x6f,0xb7,0x62,0x0e,0xaa,0x18,0xbe,0x1b,
			0xfc,0x56,0x3e,0x4b,0xc6,0xd2,0x79,0x20,0x9a,0xdb,0xc0,0xfe,0x78,0xcd,0x5a,0xf4,
			0x1f,0xdd,0xa8,0x33,0x88,0x07,0xc7,0x31,0xb1,0x12,0x10,0x59,0x27,0x80,0xec,0x5f,
			0x60,0x51,0x7f,0xa9,0x19,0xb5,0x4a,0x0d,0x2d,0xe5,0x7a,0x9f,0x93,0xc9,0x9c,0xef,
			0xa0,0xe0,0x3b,0x4d,0xae,0x2a,0xf5,0xb0,0xc8,0xeb,0xbb,0x3c,0x83,0x53,0x99,0x61,
			0x17,0x2b,0x04,0x7e,0xba,0x77,0xd6,0x26,0xe1,0x69,0x14,0x63,0x55,0x21,0x0c,0x7d,
		};

		/// [EN] Round constants for ExpandKey's key schedule (rcon[0] is
		///      unused padding; only indices 1..7 are needed since AES-256's
		///      60-word schedule only crosses an 8-word boundary 7 times).
		/// [JP] ExpandKeyの鍵スケジュール用ラウンド定数(rcon[0]は未使用の
		///      パディング。AES-256の60ワードのスケジュールは8ワード境界を
		///      7回しか跨がないため、インデックス1..7しか使わない)。
		constexpr Uint8 rcon[8] = { 0x00,0x01,0x02,0x04,0x08,0x10,0x20,0x40 };
	}

	/**
	* [EN]
	* Expands the 32-byte AES-256 key into all 15 round keys (16 bytes each)
	* via the Rijndael key schedule (RotWord/SubWord every 8 words, SubWord
	* alone every 4th word in between).
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* Rijndaelの鍵スケジュール(8ワードごとにRotWord/SubWord、その間の4
	* ワード目はSubWordのみ)で、32バイトのAES-256鍵を全15ラウンド鍵
	* (各16バイト)に展開する。
	*/
	void Aes256::ExpandKey(const Byte* key, Uint8 roundKeys[15][16])
	{
		/// [EN] The key schedule's first 8 words are just the raw key itself
		///      (Nk=8 for AES-256), copied in 4-byte groups.
		/// [JP] 鍵スケジュールの最初の8ワードは、生の鍵そのもの(AES-256は
		///      Nk=8)を4バイトずつコピーしたもの。
		Uint8 words[60][4];
		for (Uint32 wordIndex = 0; wordIndex < 8; ++wordIndex)
		{
			for (Uint32 byteIndex = 0; byteIndex < 4; ++byteIndex)
			{
				words[wordIndex][byteIndex] = static_cast<Uint8>(key[wordIndex * 4 + byteIndex]);
			}
		}

		/// [EN] Words 8..59: each new word XORs the word 8 back with a
		///      transform of the previous word. Every 8th word (wordIndex%8
		///      ==0) gets RotWord+SubWord+Rcon; AES-256's extra step (absent
		///      in AES-128) is that the word 4 positions later also gets a
		///      lone SubWord, since Nk=8 means SubWord alone is needed midway
		///      through each 8-word group to keep diffusion strong.
		/// [JP] ワード8..59: 新しい各ワードは、8つ前のワードと直前のワードの
		///      変換結果をXORして作る。8ワードごと(wordIndex%8==0)には
		///      RotWord+SubWord+Rconを適用。AES-256特有の追加ステップ
		///      (AES-128にはない)として、その4ワード後にも単独のSubWordが
		///      入る - Nk=8では各8ワードグループの中間でもSubWordだけを
		///      挟まないと拡散が弱くなるため。
		Uint8 rconIndex = 1;
		for (Uint32 wordIndex = 8; wordIndex < 60; ++wordIndex)
		{
			Uint8 temp[4];
			std::memcpy(temp, words[wordIndex - 1], 4);

			if (wordIndex % 8 == 0)
			{
				/// [EN] RotWord (rotate left by one byte) fused with SubWord
				///      (S-box each byte), then XOR the round constant into
				///      the first byte only.
				/// [JP] RotWord(1バイト左ローテート)とSubWord(各バイトに
				///      S-boxを適用)を同時に行い、最初のバイトにだけラウンド
				///      定数をXORする。
				Uint8 rotated0 = sbox[temp[1]];
				Uint8 rotated1 = sbox[temp[2]];
				Uint8 rotated2 = sbox[temp[3]];
				Uint8 rotated3 = sbox[temp[0]];
				temp[0] = static_cast<Uint8>(rotated0 ^ rcon[rconIndex]);
				temp[1] = rotated1;
				temp[2] = rotated2;
				temp[3] = rotated3;
				++rconIndex;
			}
			else if (wordIndex % 8 == 4)
			{
				temp[0] = sbox[temp[0]];
				temp[1] = sbox[temp[1]];
				temp[2] = sbox[temp[2]];
				temp[3] = sbox[temp[3]];
			}

			for (Uint32 byteIndex = 0; byteIndex < 4; ++byteIndex)
			{
				words[wordIndex][byteIndex] = static_cast<Uint8>(words[wordIndex - 8][byteIndex] ^ temp[byteIndex]);
			}
		}

		/// [EN] Regroups the 60 key-schedule words into 15 round keys of 16
		///      bytes each (4 words per round key), in the same column-major
		///      byte layout EncryptBlock/DecryptBlock use for their state.
		/// [JP] 60個の鍵スケジュールワードを、EncryptBlock/DecryptBlockの
		///      state と同じ列優先のバイト配置で、各16バイトの15ラウンド鍵
		///      (1ラウンド鍵あたり4ワード)へ再編成する。
		for (Uint32 roundIndex = 0; roundIndex < 15; ++roundIndex)
		{
			for (Uint32 columnIndex = 0; columnIndex < 4; ++columnIndex)
			{
				for (Uint32 rowIndex = 0; rowIndex < 4; ++rowIndex)
				{
					roundKeys[roundIndex][rowIndex + 4 * columnIndex] = words[roundIndex * 4 + columnIndex][rowIndex];
				}
			}
		}
	}

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
	void Aes256::EncryptBlock(const Byte* input, Byte* output, const Uint8 roundKeys[15][16])
	{
		/// [EN] state is the 4x4 byte matrix in column-major order
		///      (state[row + 4*column]), matching Rijndael's own layout.
		/// [JP] stateは列優先(state[row + 4*column])の4x4バイト行列で、
		///      Rijndael本来の配置に合わせている。
		Uint8 state[16];
		for (Uint32 byteIndex = 0; byteIndex < 16; ++byteIndex)
		{
			state[byteIndex] = static_cast<Uint8>(input[byteIndex]);
		}

		/// [EN] Initial AddRoundKey (round 0) before any substitution round.
		/// [JP] どの置換ラウンドの前にも、最初のAddRoundKey(ラウンド0)を行う。
		for (Uint32 byteIndex = 0; byteIndex < 16; ++byteIndex)
		{
			state[byteIndex] = static_cast<Uint8>(state[byteIndex] ^ roundKeys[0][byteIndex]);
		}

		for (Uint32 roundIndex = 1; roundIndex < 14; ++roundIndex)
		{
			/// [EN] SubBytes: substitute every byte through the S-box.
			/// [JP] SubBytes: 全バイトをS-boxで置換する。
			for (Uint32 byteIndex = 0; byteIndex < 16; ++byteIndex)
			{
				state[byteIndex] = sbox[state[byteIndex]];
			}

			/// [EN] ShiftRows: row r is cyclically shifted left by r columns.
			/// [JP] ShiftRows: 行rをr列分、左に巡回シフトする。
			Uint8 shifted[16];
			std::memcpy(shifted, state, 16);
			for (Uint32 rowIndex = 0; rowIndex < 4; ++rowIndex)
			{
				for (Uint32 columnIndex = 0; columnIndex < 4; ++columnIndex)
				{
					state[rowIndex + 4 * columnIndex] = shifted[rowIndex + 4 * ((columnIndex + rowIndex) % 4)];
				}
			}

			/// [EN] MixColumns: each column is multiplied by the fixed GF(2^8)
			///      matrix [[2,3,1,1],[1,2,3,1],[1,1,2,3],[3,1,1,2]]. d0..d3
			///      are each byte doubled in GF(2^8) (xtime); multiplying by
			///      3 is xtime(x)^x, so no general multiply routine is needed.
			/// [JP] MixColumns: 各列を固定のGF(2^8)行列
			///      [[2,3,1,1],[1,2,3,1],[1,1,2,3],[3,1,1,2]]で乗算する。
			///      d0..d3は各バイトをGF(2^8)で2倍(xtime)した値 - 3倍は
			///      xtime(x)^xで表せるため、汎用の乗算処理は不要。
			for (Uint32 columnIndex = 0; columnIndex < 4; ++columnIndex)
			{
				Uint8 a0 = state[4 * columnIndex + 0];
				Uint8 a1 = state[4 * columnIndex + 1];
				Uint8 a2 = state[4 * columnIndex + 2];
				Uint8 a3 = state[4 * columnIndex + 3];
				Uint8 d0 = static_cast<Uint8>((a0 << 1) ^ ((a0 & 0x80) ? 0x1B : 0));
				Uint8 d1 = static_cast<Uint8>((a1 << 1) ^ ((a1 & 0x80) ? 0x1B : 0));
				Uint8 d2 = static_cast<Uint8>((a2 << 1) ^ ((a2 & 0x80) ? 0x1B : 0));
				Uint8 d3 = static_cast<Uint8>((a3 << 1) ^ ((a3 & 0x80) ? 0x1B : 0));
				state[4 * columnIndex + 0] = static_cast<Uint8>(d0 ^ d1 ^ a1 ^ a2 ^ a3);
				state[4 * columnIndex + 1] = static_cast<Uint8>(a0 ^ d1 ^ d2 ^ a2 ^ a3);
				state[4 * columnIndex + 2] = static_cast<Uint8>(a0 ^ a1 ^ d2 ^ d3 ^ a3);
				state[4 * columnIndex + 3] = static_cast<Uint8>(d0 ^ a0 ^ a1 ^ a2 ^ d3);
			}

			/// [EN] AddRoundKey for this round.
			/// [JP] このラウンドのAddRoundKey。
			for (Uint32 byteIndex = 0; byteIndex < 16; ++byteIndex)
			{
				state[byteIndex] = static_cast<Uint8>(state[byteIndex] ^ roundKeys[roundIndex][byteIndex]);
			}
		}

		/// [EN] Final round (14): SubBytes + ShiftRows + AddRoundKey, with no
		///      MixColumns - standard Rijndael final-round structure.
		/// [JP] 最終ラウンド(14): SubBytes + ShiftRows + AddRoundKeyのみで、
		///      MixColumnsは行わない - Rijndael本来の最終ラウンド構造。
		for (Uint32 byteIndex = 0; byteIndex < 16; ++byteIndex)
		{
			state[byteIndex] = sbox[state[byteIndex]];
		}

		Uint8 shifted[16];
		std::memcpy(shifted, state, 16);
		for (Uint32 rowIndex = 0; rowIndex < 4; ++rowIndex)
		{
			for (Uint32 columnIndex = 0; columnIndex < 4; ++columnIndex)
			{
				state[rowIndex + 4 * columnIndex] = shifted[rowIndex + 4 * ((columnIndex + rowIndex) % 4)];
			}
		}

		for (Uint32 byteIndex = 0; byteIndex < 16; ++byteIndex)
		{
			state[byteIndex] = static_cast<Uint8>(state[byteIndex] ^ roundKeys[14][byteIndex]);
		}

		for (Uint32 byteIndex = 0; byteIndex < 16; ++byteIndex)
		{
			output[byteIndex] = static_cast<Byte>(state[byteIndex]);
		}
	}

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
	void Aes256::DecryptBlock(const Byte* input, Byte* output, const Uint8 roundKeys[15][16])
	{
		Uint8 state[16];
		for (Uint32 byteIndex = 0; byteIndex < 16; ++byteIndex)
		{
			state[byteIndex] = static_cast<Uint8>(input[byteIndex]);
		}

		/// [EN] Undo the final round's AddRoundKey first, since decryption
		///      walks the round keys in reverse.
		/// [JP] 復号はラウンド鍵を逆順にたどるため、まず最終ラウンドの
		///      AddRoundKeyを打ち消す。
		for (Uint32 byteIndex = 0; byteIndex < 16; ++byteIndex)
		{
			state[byteIndex] = static_cast<Uint8>(state[byteIndex] ^ roundKeys[14][byteIndex]);
		}

		for (Uint32 roundIndex = 13; roundIndex >= 1; --roundIndex)
		{
			/// [EN] InvShiftRows: row r is cyclically shifted right by r
			///      columns (the inverse of EncryptBlock's left shift).
			/// [JP] InvShiftRows: 行rをr列分、右に巡回シフトする
			///      (EncryptBlockの左シフトの逆)。
			Uint8 shifted[16];
			std::memcpy(shifted, state, 16);
			for (Uint32 rowIndex = 0; rowIndex < 4; ++rowIndex)
			{
				for (Uint32 columnIndex = 0; columnIndex < 4; ++columnIndex)
				{
					state[rowIndex + 4 * columnIndex] = shifted[rowIndex + 4 * ((columnIndex - rowIndex + 4) % 4)];
				}
			}

			/// [EN] InvSubBytes: substitute every byte through the inverse
			///      S-box.
			/// [JP] InvSubBytes: 全バイトを逆S-boxで置換する。
			for (Uint32 byteIndex = 0; byteIndex < 16; ++byteIndex)
			{
				state[byteIndex] = invSbox[state[byteIndex]];
			}

			for (Uint32 byteIndex = 0; byteIndex < 16; ++byteIndex)
			{
				state[byteIndex] = static_cast<Uint8>(state[byteIndex] ^ roundKeys[roundIndex][byteIndex]);
			}

			/// [EN] InvMixColumns: each column is multiplied by the inverse
			///      GF(2^8) matrix [[14,11,13,9],[9,14,11,13],[13,9,14,11],
			///      [11,13,9,14]]. Since 9/11/13/14 are all sums of powers of
			///      two (8+1, 8+2+1, 8+4+1, 8+4+2), each is built from three
			///      successive GF(2^8) doublings (aXd1/aXd2/aXd3 = *2/*4/*8)
			///      XORed together per the binary decomposition of the
			///      multiplier, rather than a general multiply routine.
			/// [JP] InvMixColumns: 各列を逆行列GF(2^8)
			///      [[14,11,13,9],[9,14,11,13],[13,9,14,11],[11,13,9,14]]で
			///      乗算する。9/11/13/14はいずれも2の累乗の和(8+1, 8+2+1,
			///      8+4+1, 8+4+2)で表せるため、汎用の乗算処理ではなく、
			///      3回連続のGF(2^8)倍算(aXd1/aXd2/aXd3 = *2/*4/*8)を
			///      乗数の2進分解通りにXORして組み立てている。
			for (Uint32 columnIndex = 0; columnIndex < 4; ++columnIndex)
			{
				Uint8 a0 = state[4 * columnIndex + 0];
				Uint8 a1 = state[4 * columnIndex + 1];
				Uint8 a2 = state[4 * columnIndex + 2];
				Uint8 a3 = state[4 * columnIndex + 3];

				Uint8 a0d1 = static_cast<Uint8>((a0 << 1) ^ ((a0 & 0x80) ? 0x1B : 0));
				Uint8 a0d2 = static_cast<Uint8>((a0d1 << 1) ^ ((a0d1 & 0x80) ? 0x1B : 0));
				Uint8 a0d3 = static_cast<Uint8>((a0d2 << 1) ^ ((a0d2 & 0x80) ? 0x1B : 0));

				Uint8 a1d1 = static_cast<Uint8>((a1 << 1) ^ ((a1 & 0x80) ? 0x1B : 0));
				Uint8 a1d2 = static_cast<Uint8>((a1d1 << 1) ^ ((a1d1 & 0x80) ? 0x1B : 0));
				Uint8 a1d3 = static_cast<Uint8>((a1d2 << 1) ^ ((a1d2 & 0x80) ? 0x1B : 0));

				Uint8 a2d1 = static_cast<Uint8>((a2 << 1) ^ ((a2 & 0x80) ? 0x1B : 0));
				Uint8 a2d2 = static_cast<Uint8>((a2d1 << 1) ^ ((a2d1 & 0x80) ? 0x1B : 0));
				Uint8 a2d3 = static_cast<Uint8>((a2d2 << 1) ^ ((a2d2 & 0x80) ? 0x1B : 0));

				Uint8 a3d1 = static_cast<Uint8>((a3 << 1) ^ ((a3 & 0x80) ? 0x1B : 0));
				Uint8 a3d2 = static_cast<Uint8>((a3d1 << 1) ^ ((a3d1 & 0x80) ? 0x1B : 0));
				Uint8 a3d3 = static_cast<Uint8>((a3d2 << 1) ^ ((a3d2 & 0x80) ? 0x1B : 0));

				state[4 * columnIndex + 0] = static_cast<Uint8>((a0d3 ^ a0d2 ^ a0d1) ^ (a1d3 ^ a1d1 ^ a1) ^ (a2d3 ^ a2d2 ^ a2) ^ (a3d3 ^ a3));
				state[4 * columnIndex + 1] = static_cast<Uint8>((a0d3 ^ a0) ^ (a1d3 ^ a1d2 ^ a1d1) ^ (a2d3 ^ a2d1 ^ a2) ^ (a3d3 ^ a3d2 ^ a3));
				state[4 * columnIndex + 2] = static_cast<Uint8>((a0d3 ^ a0d2 ^ a0) ^ (a1d3 ^ a1) ^ (a2d3 ^ a2d2 ^ a2d1) ^ (a3d3 ^ a3d1 ^ a3));
				state[4 * columnIndex + 3] = static_cast<Uint8>((a0d3 ^ a0d1 ^ a0) ^ (a1d3 ^ a1d2 ^ a1) ^ (a2d3 ^ a2) ^ (a3d3 ^ a3d2 ^ a3d1));
			}
		}

		/// [EN] Final round (round 0's key): InvShiftRows + InvSubBytes +
		///      AddRoundKey, with no InvMixColumns - the mirror image of
		///      EncryptBlock's initial round.
		/// [JP] 最終ラウンド(ラウンド0の鍵): InvShiftRows + InvSubBytes +
		///      AddRoundKeyのみで、InvMixColumnsは行わない -
		///      EncryptBlockの最初のラウンドと対になる構造。
		Uint8 shifted[16];
		std::memcpy(shifted, state, 16);
		for (Uint32 rowIndex = 0; rowIndex < 4; ++rowIndex)
		{
			for (Uint32 columnIndex = 0; columnIndex < 4; ++columnIndex)
			{
				state[rowIndex + 4 * columnIndex] = shifted[rowIndex + 4 * ((columnIndex - rowIndex + 4) % 4)];
			}
		}

		for (Uint32 byteIndex = 0; byteIndex < 16; ++byteIndex)
		{
			state[byteIndex] = invSbox[state[byteIndex]];
		}

		for (Uint32 byteIndex = 0; byteIndex < 16; ++byteIndex)
		{
			state[byteIndex] = static_cast<Uint8>(state[byteIndex] ^ roundKeys[0][byteIndex]);
		}

		for (Uint32 byteIndex = 0; byteIndex < 16; ++byteIndex)
		{
			output[byteIndex] = static_cast<Byte>(state[byteIndex]);
		}
	}

	/**
	* [EN]
	* CBC-encrypts plaintext with PKCS7 padding. iv must be 16 bytes and key
	* must be 32 bytes (AES-256). Returns the ciphertext, always a multiple
	* of 16 bytes since PKCS7 always adds at least one byte of padding (a
	* full extra block when plaintext is already block-sized).
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* PKCS7パディング付きでplaintextをCBC暗号化する。ivは16バイト、keyは32
	* バイト(AES-256)である必要がある。PKCS7は常に最低1バイトのパディングを
	* 追加する(plaintextが既にブロックサイズの倍数ならまるまる1ブロック
	* 追加される)ため、返す暗号文は常に16バイトの倍数。
	*/
	DynamicArray<Byte> Aes256::Encrypt(const DynamicArray<Byte>& key, const DynamicArray<Byte>& iv, const DynamicArray<Byte>& plaintext)
	{
		/// [EN] PKCS7: pad with N bytes each holding the value N, where N is
		///      chosen so the total becomes a multiple of 16 (1..16, never 0
		///      - hence the "+1" instead of rounding down when already
		///      block-sized). Padding happens once here regardless of which
		///      path (software/hardware) does the actual block cipher below.
		/// [JP] PKCS7: 全体が16の倍数になるよう、値Nを持つバイトをN個
		///      (1..16、0にはならない - すでにブロックサイズの倍数でも
		///      切り捨てず"+1"する理由)追加する。パディングは、この後どちらの
		///      経路(ソフトウェア/ハードウェア)が実際のブロック暗号を行うかに
		///      関わらず、ここで一度だけ行う。
		Size paddedSize = (plaintext.size() / 16 + 1) * 16;
		Uint8 padValue = static_cast<Uint8>(paddedSize - plaintext.size());

		DynamicArray<Byte> padded(paddedSize);
		std::memcpy(padded.data(), plaintext.data(), plaintext.size());
		for (Size byteIndex = plaintext.size(); byteIndex < paddedSize; ++byteIndex)
		{
			padded[byteIndex] = static_cast<Byte>(padValue);
		}

		if (paddedSize >= hardwareThreshold_)
		{
			return EncryptHardware(key, iv, padded);
		}
		return EncryptSoftware(key, iv, padded);
	}

	/**
	* [EN]
	* CBC-encrypts already-padded, block-aligned data via this class's own
	* from-scratch AES-256 (EncryptBlock).
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 既にパディング済み・ブロック境界揃いのデータを、このクラス自前の
	* AES-256(EncryptBlock)でCBC暗号化する。
	*/
	DynamicArray<Byte> Aes256::EncryptSoftware(const DynamicArray<Byte>& key, const DynamicArray<Byte>& iv, const DynamicArray<Byte>& paddedPlaintext)
	{
		Uint8 roundKeys[15][16];
		ExpandKey(key.data(), roundKeys);

		/// [EN] CBC chaining: each block is XORed with the previous
		///      ciphertext block (the IV for the first block) before being
		///      AES-encrypted.
		/// [JP] CBC連鎖: 各ブロックは、AES暗号化する前に直前の暗号文ブロック
		///      (最初のブロックはIV)とXORされる。
		DynamicArray<Byte> ciphertext(paddedPlaintext.size());
		Byte previousBlock[16];
		std::memcpy(previousBlock, iv.data(), 16);

		for (Size blockIndex = 0; blockIndex < paddedPlaintext.size() / 16; ++blockIndex)
		{
			Byte xored[16];
			for (Uint32 byteIndex = 0; byteIndex < 16; ++byteIndex)
			{
				xored[byteIndex] = static_cast<Byte>(paddedPlaintext[blockIndex * 16 + byteIndex] ^ previousBlock[byteIndex]);
			}

			Byte encrypted[16];
			EncryptBlock(xored, encrypted, roundKeys);

			std::memcpy(&ciphertext[blockIndex * 16], encrypted, 16);
			std::memcpy(previousBlock, encrypted, 16);
		}

		return ciphertext;
	}

	/**
	* [EN]
	* CBC-encrypts already-padded, block-aligned data via Windows' BCrypt (no
	* BCRYPT_BLOCK_PADDING flag - the data is already padded, so BCrypt is
	* only asked to run the raw cipher).
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 既にパディング済み・ブロック境界揃いのデータを、Windows標準のBCryptで
	* CBC暗号化する(BCRYPT_BLOCK_PADDINGフラグは付けない - データは既に
	* パディング済みなので、BCryptには生の暗号処理だけを行わせる)。
	*/
	DynamicArray<Byte> Aes256::EncryptHardware(const DynamicArray<Byte>& key, const DynamicArray<Byte>& iv, const DynamicArray<Byte>& paddedPlaintext)
	{
		BCRYPT_ALG_HANDLE algorithm = nullptr;
		if (!BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_AES_ALGORITHM, nullptr, 0)))
		{
			return EncryptSoftware(key, iv, paddedPlaintext);
		}

		BCryptSetProperty(algorithm, BCRYPT_CHAINING_MODE, reinterpret_cast<PUCHAR>(const_cast<wchar_t*>(BCRYPT_CHAIN_MODE_CBC)), sizeof(BCRYPT_CHAIN_MODE_CBC), 0);

		BCRYPT_KEY_HANDLE keyHandle = nullptr;
		NTSTATUS keyStatus = BCryptGenerateSymmetricKey(algorithm, &keyHandle, nullptr, 0, reinterpret_cast<PUCHAR>(const_cast<Byte*>(key.data())), static_cast<ULONG>(key.size()), 0);

		DynamicArray<Byte> ciphertext;
		if (BCRYPT_SUCCESS(keyStatus))
		{
			Byte ivBuffer[16];
			std::memcpy(ivBuffer, iv.data(), 16);

			ULONG resultLength = 0;
			BCryptEncrypt(keyHandle, reinterpret_cast<PUCHAR>(const_cast<Byte*>(paddedPlaintext.data())), static_cast<ULONG>(paddedPlaintext.size()), nullptr, reinterpret_cast<PUCHAR>(ivBuffer), 16, nullptr, 0, &resultLength, 0);

			ciphertext.resize(resultLength);
			std::memcpy(ivBuffer, iv.data(), 16);

			ULONG written = 0;
			NTSTATUS encryptStatus = BCryptEncrypt(keyHandle, reinterpret_cast<PUCHAR>(const_cast<Byte*>(paddedPlaintext.data())), static_cast<ULONG>(paddedPlaintext.size()), nullptr, reinterpret_cast<PUCHAR>(ivBuffer), 16, reinterpret_cast<PUCHAR>(ciphertext.data()), resultLength, &written, 0);
			if (!BCRYPT_SUCCESS(encryptStatus))
			{
				ciphertext.clear();
			}

			BCryptDestroyKey(keyHandle);
		}

		BCryptCloseAlgorithmProvider(algorithm, 0);

		if (ciphertext.empty())
		{
			return EncryptSoftware(key, iv, paddedPlaintext);
		}
		return ciphertext;
	}

	/**
	* [EN]
	* CBC-decrypts a whole ciphertext produced by Encrypt() and strips its
	* trailing PKCS7 padding. Returns an empty array if the padding is
	* invalid (wrong key/iv, corrupted data, or ciphertext not a multiple of
	* 16 bytes) - callers treat that as a decrypt failure/cache miss.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* Encrypt()が生成した暗号文全体をCBC復号し、末尾のPKCS7パディングを
	* 除去する。パディングが不正(鍵/IV違い、データ破損、暗号文が16バイトの
	* 倍数でない等)なら空配列を返す - 呼び出し側はこれを復号失敗/
	* キャッシュミス扱いにする。
	*/
	DynamicArray<Byte> Aes256::Decrypt(const DynamicArray<Byte>& key, const DynamicArray<Byte>& iv, const DynamicArray<Byte>& ciphertext)
	{
		DynamicArray<Byte> plaintext = DecryptUnpadded(key, iv, ciphertext);
		if (plaintext.empty())
		{
			return {};
		}

		/// [EN] Validate the PKCS7 padding before trusting it: the last byte
		///      must be in [1,16] and no larger than the whole plaintext -
		///      anything else means the key/iv was wrong or the data is
		///      corrupted, not a real padding value.
		/// [JP] 信用する前にPKCS7パディングを検証する: 最後のバイトは
		///      [1,16]の範囲かつplaintext全体のサイズ以下でなければならない
		///      - それ以外は鍵/IVが違うかデータが破損しているだけで、
		///      本物のパディング値ではない。
		Uint8 padValue = static_cast<Uint8>(plaintext.back());
		if (padValue == 0 || padValue > 16 || static_cast<Size>(padValue) > plaintext.size())
		{
			return {};
		}
		plaintext.resize(plaintext.size() - padValue);
		return plaintext;
	}

	/**
	* [EN]
	* CBC-decrypts an arbitrary (not necessarily whole-file) block-aligned
	* ciphertext range without stripping PKCS7 padding - for random-access
	* decryption of a sub-range, the final block of the ciphertext passed in
	* isn't necessarily the file's actual final block, so the padding byte
	* at the end can't be trusted.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* PKCS7パディングを除去しない、任意の(ファイル全体とは限らない)ブロック
	* 境界揃いの暗号文範囲のCBC復号 - 部分範囲のランダムアクセス復号では、
	* 渡された暗号文の最後のブロックが必ずしもファイル本来の最終ブロックとは
	* 限らないため、末尾のパディングバイトを信用できない。
	*/
	DynamicArray<Byte> Aes256::DecryptUnpadded(const DynamicArray<Byte>& key, const DynamicArray<Byte>& iv, const DynamicArray<Byte>& ciphertext)
	{
		if (ciphertext.empty() || ciphertext.size() % 16 != 0)
		{
			return {};
		}

		if (ciphertext.size() >= hardwareThreshold_)
		{
			return DecryptUnpaddedHardware(key, iv, ciphertext);
		}
		return DecryptUnpaddedSoftware(key, iv, ciphertext);
	}

	/**
	* [EN]
	* CBC-decrypts a block-aligned ciphertext range via this class's own
	* from-scratch AES-256 (DecryptBlock), without stripping padding.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* ブロック境界揃いの暗号文範囲を、このクラス自前のAES-256(DecryptBlock)
	* でCBC復号する。パディングは除去しない。
	*/
	DynamicArray<Byte> Aes256::DecryptUnpaddedSoftware(const DynamicArray<Byte>& key, const DynamicArray<Byte>& iv, const DynamicArray<Byte>& ciphertext)
	{
		Uint8 roundKeys[15][16];
		ExpandKey(key.data(), roundKeys);

		/// [EN] CBC decrypt only needs the previous ciphertext block (not the
		///      previous plaintext) to XOR against each decrypted block, so
		///      any block range can be decrypted independently as long as
		///      the caller supplies the correct chain-input block as iv -
		///      the true stream IV for the first block, or the raw
		///      ciphertext bytes immediately preceding the range otherwise.
		/// [JP] CBC復号は各復号ブロックとXORする直前の暗号文ブロック(直前の
		///      平文ではない)さえあれば成立するため、呼び出し側が正しい
		///      連鎖入力ブロックをivとして渡す限り、任意のブロック範囲を
		///      独立に復号できる - 最初のブロックならストリーム本来のIV、
		///      それ以外ならその範囲の直前にある生の暗号文バイト。
		DynamicArray<Byte> plaintext(ciphertext.size());
		Byte previousBlock[16];
		std::memcpy(previousBlock, iv.data(), 16);

		for (Size blockIndex = 0; blockIndex < ciphertext.size() / 16; ++blockIndex)
		{
			Byte decrypted[16];
			DecryptBlock(&ciphertext[blockIndex * 16], decrypted, roundKeys);

			for (Uint32 byteIndex = 0; byteIndex < 16; ++byteIndex)
			{
				plaintext[blockIndex * 16 + byteIndex] = static_cast<Byte>(decrypted[byteIndex] ^ previousBlock[byteIndex]);
			}

			std::memcpy(previousBlock, &ciphertext[blockIndex * 16], 16);
		}

		return plaintext;
	}

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
	DynamicArray<Byte> Aes256::DecryptUnpaddedHardware(const DynamicArray<Byte>& key, const DynamicArray<Byte>& iv, const DynamicArray<Byte>& ciphertext)
	{
		BCRYPT_ALG_HANDLE algorithm = nullptr;
		if (!BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_AES_ALGORITHM, nullptr, 0)))
		{
			return DecryptUnpaddedSoftware(key, iv, ciphertext);
		}

		BCryptSetProperty(algorithm, BCRYPT_CHAINING_MODE, reinterpret_cast<PUCHAR>(const_cast<wchar_t*>(BCRYPT_CHAIN_MODE_CBC)), sizeof(BCRYPT_CHAIN_MODE_CBC), 0);

		BCRYPT_KEY_HANDLE keyHandle = nullptr;
		NTSTATUS keyStatus = BCryptGenerateSymmetricKey(algorithm, &keyHandle, nullptr, 0, reinterpret_cast<PUCHAR>(const_cast<Byte*>(key.data())), static_cast<ULONG>(key.size()), 0);

		DynamicArray<Byte> plaintext;
		if (BCRYPT_SUCCESS(keyStatus))
		{
			Byte ivBuffer[16];
			std::memcpy(ivBuffer, iv.data(), 16);

			ULONG resultLength = 0;
			BCryptDecrypt(keyHandle, reinterpret_cast<PUCHAR>(const_cast<Byte*>(ciphertext.data())), static_cast<ULONG>(ciphertext.size()), nullptr, reinterpret_cast<PUCHAR>(ivBuffer), 16, nullptr, 0, &resultLength, 0);

			plaintext.resize(resultLength);
			std::memcpy(ivBuffer, iv.data(), 16);

			ULONG written = 0;
			NTSTATUS decryptStatus = BCryptDecrypt(keyHandle, reinterpret_cast<PUCHAR>(const_cast<Byte*>(ciphertext.data())), static_cast<ULONG>(ciphertext.size()), nullptr, reinterpret_cast<PUCHAR>(ivBuffer), 16, reinterpret_cast<PUCHAR>(plaintext.data()), resultLength, &written, 0);
			if (!BCRYPT_SUCCESS(decryptStatus))
			{
				plaintext.clear();
			}

			BCryptDestroyKey(keyHandle);
		}

		BCryptCloseAlgorithmProvider(algorithm, 0);

		if (plaintext.empty())
		{
			return DecryptUnpaddedSoftware(key, iv, ciphertext);
		}
		return plaintext;
	}
}
