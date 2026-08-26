#include <FoundationEngine/Serialization/Encryption/Sha256.h>

namespace SeedCore
{
	namespace
	{
		/// [EN] FIPS 180-4's fixed SHA-256 round constants (the first 32
		///      bits of the fractional parts of the cube roots of the first
		///      64 primes), used one per round in ProcessBlock's compression
		///      loop.
		/// [JP] FIPS 180-4で定められたSHA-256の固定ラウンド定数(最初の64個の
		///      素数の立方根の小数部分、先頭32bit)。ProcessBlockの圧縮
		///      ループで1ラウンドにつき1つ使う。
		constexpr Uint32 roundConstants[64] =
		{
			0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
			0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
			0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
			0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
			0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
			0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
			0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
			0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
		};

	}

	/**
	* [EN]
	* Compresses one 64-byte message block into the running 8-word (32
	* bytes) hash state, per FIPS 180-4's message schedule expansion and
	* 64-round compression function.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* FIPS 180-4のメッセージスケジュール拡張と64ラウンドの圧縮関数に従い、
	* 64バイトのメッセージブロック1つを、進行中の8ワード(32バイト)
	* ハッシュ状態へ圧縮する。
	*/
	void Sha256::ProcessBlock(Uint32 state[8], const Byte block[64])
	{
		/// [EN] The first 16 schedule words are just the block's 64 bytes
		///      read as big-endian 32-bit words.
		/// [JP] 最初の16個のスケジュールワードは、ブロックの64バイトを
		///      ビッグエンディアンの32bitワードとして読んだだけの値。
		Uint32 schedule[64]{};
		for (Uint32 index = 0; index < 16; ++index)
		{
			Uint32 byteOffset = index * 4;
			schedule[index] = (static_cast<Uint32>(static_cast<Uint8>(block[byteOffset])) << 24) | (static_cast<Uint32>(static_cast<Uint8>(block[byteOffset + 1])) << 16) | (static_cast<Uint32>(static_cast<Uint8>(block[byteOffset + 2])) << 8) | static_cast<Uint32>(static_cast<Uint8>(block[byteOffset + 3]));
		}

		/// [EN] Words 16..63 extend the schedule: each new word mixes two
		///      earlier words (16 and 7 back) with two rotate-based sigma
		///      functions applied to the words 15 and 2 back.
		/// [JP] ワード16..63でスケジュールを拡張する: 新しい各ワードは、
		///      16個前と7個前の2ワードを、15個前と2個前のワードに
		///      ローテートベースのsigma関数を適用した値と合成して作る。
		for (Uint32 index = 16; index < 64; ++index)
		{
			Uint32 sigma0 = ((schedule[index - 15] >> 7) | (schedule[index - 15] << 25)) ^ ((schedule[index - 15] >> 18) | (schedule[index - 15] << 14)) ^ (schedule[index - 15] >> 3);
			Uint32 sigma1 = ((schedule[index - 2] >> 17) | (schedule[index - 2] << 15)) ^ ((schedule[index - 2] >> 19) | (schedule[index - 2] << 13)) ^ (schedule[index - 2] >> 10);
			schedule[index] = schedule[index - 16] + sigma0 + schedule[index - 7] + sigma1;
		}

		Uint32 a = state[0];
		Uint32 b = state[1];
		Uint32 c = state[2];
		Uint32 d = state[3];
		Uint32 e = state[4];
		Uint32 f = state[5];
		Uint32 g = state[6];
		Uint32 h = state[7];

		/// [EN] The 64-round compression loop: sum1/choice form the "Ch"
		///      path from e/f/g, sum0/majority form the "Maj" path from
		///      a/b/c, and the eight working variables shift down by one
		///      each round (the classic SHA-2 round function).
		/// [JP] 64ラウンドの圧縮ループ: sum1/choiceがe/f/gから"Ch"パスを、
		///      sum0/majorityがa/b/cから"Maj"パスを作り、8つの作業変数が
		///      毎ラウンド1つずつ下にシフトする(SHA-2の標準的なラウンド
		///      関数)。
		for (Uint32 index = 0; index < 64; ++index)
		{
			Uint32 sum1 = ((e >> 6) | (e << 26)) ^ ((e >> 11) | (e << 21)) ^ ((e >> 25) | (e << 7));
			Uint32 choice = (e & f) ^ ((~e) & g);
			Uint32 temp1 = h + sum1 + choice + roundConstants[index] + schedule[index];
			Uint32 sum0 = ((a >> 2) | (a << 30)) ^ ((a >> 13) | (a << 19)) ^ ((a >> 22) | (a << 10));
			Uint32 majority = (a & b) ^ (a & c) ^ (b & c);
			Uint32 temp2 = sum0 + majority;

			h = g;
			g = f;
			f = e;
			e = d + temp1;
			d = c;
			c = b;
			b = a;
			a = temp1 + temp2;
		}

		/// [EN] Feed-forward: add this block's working variables back into
		///      the running hash state.
		/// [JP] フィードフォワード: このブロックの作業変数を、進行中の
		///      ハッシュ状態へ加算する。
		state[0] += a;
		state[1] += b;
		state[2] += c;
		state[3] += d;
		state[4] += e;
		state[5] += f;
		state[6] += g;
		state[7] += h;
	}

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
	DynamicArray<Byte> Sha256::Hash(const Byte* data, Size size)
	{
		/// [EN] FIPS 180-4's fixed SHA-256 initial hash value (the first 32
		///      bits of the fractional parts of the square roots of the
		///      first 8 primes).
		/// [JP] FIPS 180-4で定められたSHA-256の初期ハッシュ値(最初の8個の
		///      素数の平方根の小数部分、先頭32bit)。
		Uint32 state[8] = { 0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19 };

		Size fullBlockCount = size / 64;
		for (Size blockIndex = 0; blockIndex < fullBlockCount; ++blockIndex)
		{
			ProcessBlock(state, data + blockIndex * 64);
		}

		/// [EN] Merkle-Damgard padding: append a single 0x80 byte, then
		///      zero-fill, leaving the final 8 bytes for the original
		///      message's bit length. If the remaining data plus that
		///      terminator doesn't leave room for the 8-byte length (i.e.
		///      remainder >= 56), padding spills into a second block.
		/// [JP] Merkle-Damgardパディング: 0x80を1バイト追加し、ゼロ埋めし、
		///      末尾8バイトを元メッセージのビット長用に空けておく。残り
		///      データ+終端バイトが8バイトの長さフィールド分の余地を
		///      残さない場合(remainder >= 56)、パディングは2ブロック目に
		///      またがる。
		Size remainder = size - fullBlockCount * 64;
		Byte tail[128]{};
		std::memcpy(tail, data + fullBlockCount * 64, remainder);
		tail[remainder] = static_cast<Byte>(0x80);

		Size paddedSize = (remainder < 56) ? 64 : 128;
		Uint64 bitLength = static_cast<Uint64>(size) * 8;
		for (Uint32 index = 0; index < 8; ++index)
		{
			tail[paddedSize - 1 - index] = static_cast<Byte>(bitLength >> (index * 8));
		}

		ProcessBlock(state, tail);
		if (paddedSize == 128)
		{
			ProcessBlock(state, tail + 64);
		}

		/// [EN] Serialize the 8-word state as big-endian bytes to form the
		///      final 32-byte digest.
		/// [JP] 8ワードの状態をビッグエンディアンのバイト列にして、最終的な
		///      32バイトのダイジェストにする。
		DynamicArray<Byte> digest(32);
		for (Uint32 index = 0; index < 8; ++index)
		{
			digest[index * 4] = static_cast<Byte>(state[index] >> 24);
			digest[index * 4 + 1] = static_cast<Byte>(state[index] >> 16);
			digest[index * 4 + 2] = static_cast<Byte>(state[index] >> 8);
			digest[index * 4 + 3] = static_cast<Byte>(state[index]);
		}
		return digest;
	}

	/**
	* [EN]
	* Hashes the entire contents of data and returns the 32-byte digest.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* dataの内容全体をハッシュ化し、32バイトのダイジェストを返す。
	*/
	DynamicArray<Byte> Sha256::Hash(const DynamicArray<Byte>& data)
	{
		return Hash(data.data(), data.size());
	}
}
