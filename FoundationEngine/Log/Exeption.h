#pragma once
#include <FoundationEngine/Prelude.h>

/**
* [EN]
* Throws a SeedCore::Exception formatted with fmt, tagged with the
* current source location.
*
* ---------------------------------------------------------------------
*
* [JP]
* fmt で整形し、現在のソース位置を付与した SeedCore::Exception を
* スローする。
*/
#define SC_THROW(fmt, ...) \
	throw SeedCore::Exception(std::format(fmt, ##__VA_ARGS__), std::source_location::current())

namespace SeedCore
{
	/**
	* [EN]
	* Engine-wide exception type. Prefixes the message with the throwing
	* file/line before handing it to std::runtime_error.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* エンジン全体で使う例外型。std::runtime_error に渡す前に、
	* スロー元のファイル/行をメッセージの先頭に付与する。
	*/
	class Exception : public std::runtime_error
	{
	public:
		Exception(const std::string& message, const std::source_location& loc) : std::runtime_error(std::format("[{}:{}] {}", loc.file_name(), loc.line(), message))
		{
			/// No Code
		}
	};
}