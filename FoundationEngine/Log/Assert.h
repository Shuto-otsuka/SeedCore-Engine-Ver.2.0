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