#pragma once
#include <FoundationEngine/Prelude.h>

/**
* [EN]
* Thin wrapper over static_assert for naming consistency with SC_ASSERT.
*
* ---------------------------------------------------------------------
*
* [JP]
* SC_ASSERT と命名を揃えるための static_assert の薄いラッパー。
*/
#define SC_STATIC_ASSERT(cond, msg) static_assert(cond, msg)

/**
* [EN]
* Asserts that a C++ struct's size matches its HLSL mirror's, with a
* Japanese message generated from the type name and the HLSL file it
* mirrors - avoids hand-writing "<Type> が <file> と一致していません"
* at every call site.
*
* ---------------------------------------------------------------------
*
* [JP]
* C++ 構造体のサイズが対応する HLSL 側と一致することを表明する。
* 型名とミラー先の HLSL ファイル名から日本語メッセージを自動生成する
* ため、呼び出し側で「<型> が <ファイル> と一致していません」を
* 毎回手書きしなくてよい。
*/
#define SC_STATIC_ASSERT_SIZE(type, size, hlslFile) static_assert(sizeof(type) == (size), #type " が " hlslFile " と一致していません")

/**
* [EN]
* Asserts that a C++ struct's size is a whole multiple of 16 bytes (one
* cbuffer row), with a generated Japanese message. For cbuffer-mirror
* structs whose HLSL side has no single fixed byte size to compare
* against directly.
*
* ---------------------------------------------------------------------
*
* [JP]
* C++ 構造体のサイズが 16 バイト(cbuffer の1行)の倍数であることを
* 表明する。日本語メッセージは自動生成する。HLSL 側に比較すべき単一の
* 固定バイト数が無い cbuffer ミラー構造体向け。
*/
#define SC_STATIC_ASSERT_ALIGNED16(type) static_assert(sizeof(type) % 16 == 0, #type " が 16 バイト行の倍数ではありません")

/**
* [EN]
* Runtime assertion. If cond is false, reports via HandleAssert
* (optionally with a formatted message) and terminates. Wrapped in a
* do { } while (false) so it behaves like a single statement at the
* call site (safe inside unbraced if/else).
*
* ---------------------------------------------------------------------
*
* [JP]
* 実行時アサーション。cond が偽の場合、HandleAssert（任意で整形済み
* メッセージ付き）を通じて報告し、終了する。呼び出し側で単一の文として
* 振る舞うよう do { } while (false) でラップしている（波括弧なしの
* if/else 内でも安全）。
*/
#define SC_ASSERT(cond, ...) \
		do { \
			if (!(cond)) { \
				SeedCore::HandleAssert(#cond, std::source_location::current(), ##__VA_ARGS__); \
			} \
		} while (false)

namespace SeedCore
{
	/**
	* [EN]
	* Reports a failed assertion with a formatted message to stderr, then
	* calls std::terminate. Called by SC_ASSERT when a message/format
	* arguments are supplied.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 整形済みメッセージ付きでアサーション失敗を stderr に報告し、
	* std::terminate を呼ぶ。メッセージ/フォーマット引数が渡された場合に
	* SC_ASSERT から呼ばれる。
	*/
	template<typename... Args>
	void HandleAssert(const Char* cond, const std::source_location& loc, std::format_string<Args...> fmt, Args&&... args)
	{
		std::cerr << std::format("[Assert Failed] {}\nFile: {}\nLine: {}\nMessage: {}\n", cond, loc.file_name(), loc.line(), std::format(fmt, std::forward<Args>(args)...));
		std::terminate();
	}

	/**
	* [EN]
	* Reports a failed assertion without a message to stderr, then calls
	* std::terminate. Called by SC_ASSERT when no message/format
	* arguments are supplied.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* メッセージなしでアサーション失敗を stderr に報告し、std::terminate
	* を呼ぶ。メッセージ/フォーマット引数が渡されなかった場合に
	* SC_ASSERT から呼ばれる。
	*/
	inline void HandleAssert(const Char* cond, const std::source_location& loc)
	{
		std::cerr << std::format("[Assert Failed] {}\nFile: {}\nLine: {}\n", cond, loc.file_name(), loc.line());
		std::terminate();
	}
}