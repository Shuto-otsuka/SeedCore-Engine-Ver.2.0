#pragma once
#include <cstddef>
#include <cstdint>

namespace SeedCore
{
	/// [EN] Signed integer aliases. Int is the default choice; the fixed-width
	///      variants pin down an exact bit width (e.g. wire/file formats).
	/// [JP] 符号付き整数のエイリアス。Int が既定の選択肢で、固定幅版は
	///      ビット幅を明示したい場合（通信/ファイルフォーマットなど）に使う。
	using Int = int;
	using Int8 = std::int8_t;
	using Int16 = std::int16_t;
	using Int32 = std::int32_t;
	using Int64 = std::int64_t;

	/// [EN] Unsigned integer aliases, mirroring the Int group.
	/// [JP] Int 群に対応する符号なし整数のエイリアス。
	using Uint = unsigned int;
	using Uint8 = std::uint8_t;
	using Uint16 = std::uint16_t;
	using Uint32 = std::uint32_t;
	using Uint64 = std::uint64_t;

	/// [EN] Floating-point aliases.
	/// [JP] 浮動小数点数のエイリアス。
	using Float = float;
	using Double = double;

	/// [EN] Alias for unsigned, kept separate from Uint for call sites
	///      that spell out the bare keyword (e.g. matching a third-party signature).
	/// [JP] unsigned のエイリアス。生のキーワードを使う呼び出し箇所
	///      （サードパーティのシグネチャに合わせる場合など）のために Uint とは別に用意。
	using Unsigned = unsigned;

	/// [EN] long/long long aliases, for interop with APIs that use them explicitly.
	/// [JP] long/long long のエイリアス。それらを明示的に使うAPIとの連携用。
	using Long = long;
	using Longlong = long long;
	using Ulong = unsigned long;
	using Ulonglong = unsigned long long;

	/// [EN] short aliases, for interop with APIs that use them explicitly.
	/// [JP] short のエイリアス。それを明示的に使うAPIとの連携用。
	using Short = short;
	using Ushort = unsigned short;

	/// [EN] Boolean alias.
	/// [JP] 真偽値のエイリアス。
	using Bool = bool;

	/// [EN] Character aliases, one per string encoding.
	/// [JP] 文字のエイリアス。文字列エンコーディングごとに1つ。
	using Char = char;
	using Char8 = char8_t;
	using Char16 = char16_t;
	using Char32 = char32_t;
	using Wchar = wchar_t;

	/// [EN] Unsigned 8-bit character, mainly for locale-safe <cctype>-style
	///      functions (e.g. std::tolower) that require an unsigned argument.
	/// [JP] 符号なし8ビット文字。主に std::tolower のような、符号なし引数を
	///      要求するロケール安全な <cctype> 系関数のために使う。
	using Uchar = unsigned char;

	/// [EN] Alias for std::size_t, used for sizes/counts/indices.
	/// [JP] std::size_t のエイリアス。サイズ・個数・インデックスに使う。
	using Size = std::size_t;

	/// [EN] Alias for raw memory bytes, distinct from Char (text) even
	///      though both are char underneath — use Byte for
	///      memcpy/binary-I/O-style code, Char for text.
	/// [JP] 生メモリのバイトを表すエイリアス。内部的には Char（テキスト）
	///      と同じ char だが用途で使い分ける — memcpy やバイナリI/Oには
	///      Byte、テキストには Char を使う。
	using Byte = char;
}