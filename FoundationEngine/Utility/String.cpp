#include <FoundationEngine/Utility/String.h>
#include <FoundationEngine/Pool/InternPool.h>

namespace SeedCore
{
	/**
	* [EN]
	* Interns view (UTF-8) and returns the corresponding String.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* view（UTF-8）をインターンし、対応する String を返す。
	*/
	String String::intern(std::string_view view)
	{
		return intern(std::u8string_view(reinterpret_cast<const Char8*>(view.data()), view.size()));
	}

	/**
	* [EN]
	* Interns view (UTF-16), converting it to UTF-8 first.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* view（UTF-16）を先に UTF-8 へ変換したうえでインターンする。
	*/
	String String::intern(std::wstring_view view)
	{
		std::string string = ConvertToCharString(view);
		return intern(std::string_view(string));
	}

	/**
	* [EN]
	* Interns a NUL-terminated UTF-8 (char8_t) string.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* NUL終端のUTF-8（char8_t）文字列をインターンする。
	*/
	String String::intern(const Char8* string)
	{
		return intern(std::u8string_view(reinterpret_cast<const Char8*>(string)));
	}

	/**
	* [EN]
	* Interns view (UTF-8 as char8_t) via the process-wide InternPool.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* プロセス全体で共有される InternPool 経由で view（char8_t としての
	* UTF-8）をインターンする。
	*/
	String String::intern(std::u8string_view view)
	{
		return InternPool::Singleton().Intern<String>(view);
	}

	/**
	* [EN]
	* Constructs by interning a NUL-terminated UTF-8 string literal
	* (const char*), delegating to the std::string_view constructor.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* NUL終端のUTF-8文字列リテラル（const char*）をインターンして構築する。
	* std::string_view を受け取るコンストラクタへ委譲する。
	*/
	String::String(const Char* text) :String(std::string_view(text))
	{
		/// No Code
	}

	/**
	* [EN]
	* Constructs by interning view (UTF-8 as char8_t).
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* view（char8_t としてのUTF-8）をインターンして構築する。
	*/
	String::String(std::u8string_view view)
	{
		*this = intern(view);
	}

	/**
	* [EN]
	* Constructs by interning view (UTF-8).
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* view（UTF-8）をインターンして構築する。
	*/
	String::String(std::string_view view)
	{
		*this = intern(std::u8string_view(reinterpret_cast<const Char8*>(view.data()), view.size()));
	}

	/**
	* [EN]
	* Constructs by interning view (UTF-16).
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* view（UTF-16）をインターンして構築する。
	*/
	String::String(std::wstring_view view)
	{
		*this = intern(view);
	}

	/**
	* [EN]
	* Constructs directly from an already-interned pointer/size pair.
	* Only InternPool may call this.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 既にインターン済みのポインタ/サイズの組から直接構築する。
	* InternPool のみが呼び出せる。
	*/
	String::String(const Char8* view, Size size) :view_(view, size)
	{
		/// No Code
	}

	/**
	* [EN]
	* Returns a NUL-terminated char pointer to the interned UTF-8 data.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* インターン済みのUTF-8データへの、NUL終端の char ポインタを返す。
	*/
	const Char* String::c_str()const
	{
		return reinterpret_cast<const Char*>(view_.data());
	}

	/**
	* [EN]
	* Returns a copy of the interned data as std::string (UTF-8).
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* インターン済みデータのコピーを std::string（UTF-8）として返す。
	*/
	std::string String::str()const
	{
		return std::string(c_str(), view_.size());
	}

	/**
	* [EN]
	* Returns a copy of the interned data converted to std::wstring (UTF-16).
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* インターン済みデータを std::wstring（UTF-16）に変換したコピーを返す。
	*/
	std::wstring String::w_str()const
	{
		return ConvertToWideString(this->str());
	}

	/**
	* [EN]
	* Returns a copy of the interned data as std::u8string.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* インターン済みデータのコピーを std::u8string として返す。
	*/
	std::u8string String::u8str()const
	{
		return std::u8string(view_.data(), view_.size());
	}

	/**
	* [EN]
	* Explicit conversion to std::string (UTF-8), equivalent to str().
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* std::string（UTF-8）への明示的な変換。str() と同等。
	*/
	String::operator std::string()const
	{
		return std::string(reinterpret_cast<const Char*>(view_.data()), view_.size());
	}

	/**
	* [EN]
	* Explicit conversion to std::u8string, equivalent to u8str().
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* std::u8string への明示的な変換。u8str() と同等。
	*/
	String::operator std::u8string()const
	{
		return std::u8string(view_.data(), view_.size());
	}

	/**
	* [EN]
	* Explicit conversion to a std::string_view onto the interned UTF-8
	* data (reinterpreted from char8_t).
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* インターン済みUTF-8データ（char8_t から再解釈）への
	* std::string_view への明示的な変換。
	*/
	String::operator std::string_view()const
	{
		return std::string_view(reinterpret_cast<const Char*>(view_.data()), view_.size());
	}

	/**
	* [EN]
	* Explicit conversion to a std::u8string_view onto the interned data,
	* equivalent to view().
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* インターン済みデータへの std::u8string_view への明示的な変換。
	* view() と同等。
	*/
	String::operator std::u8string_view()const
	{
		return std::u8string_view(view_.data(), view_.size());
	}
}