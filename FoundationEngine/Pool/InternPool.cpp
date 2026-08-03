#include <Windows.h>

#include <FoundationEngine/Pool/InternPool.h>

namespace SeedCore
{
	/**
	* [EN]
	* Hashes a u8string_view directly (the transparent lookup path).
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* u8string_view を直接ハッシュ化する（透過検索経路）。
	*/
	Size InternPool::TransparentHash::operator()(std::u8string_view view)const noexcept
	{
		return std::hash<std::u8string_view>{}(view);
	}

	/**
	* [EN]
	* Hashes a stored pmr::u8string (used when hashing set elements),
	* producing the same result as hashing its u8string_view.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 格納済みの pmr::u8string（セット要素のハッシュ計算時に使用）を
	* ハッシュ化する。その u8string_view をハッシュ化した場合と同じ結果になる。
	*/
	Size InternPool::TransparentHash::operator()(const std::pmr::u8string& string)const noexcept
	{
		return std::hash<std::u8string_view>{}(std::u8string_view{ string });
	}
}

namespace SeedCore
{
	/**
	* [EN]
	* Compares two u8string_views for content equality.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 2つの u8string_view の内容が等しいか比較する。
	*/
	Bool InternPool::TransparentEqual::operator()(std::u8string_view a, std::u8string_view b)const noexcept
	{
		return a == b;
	}

	/**
	* [EN]
	* Compares a stored pmr::u8string against a lookup u8string_view.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 格納済みの pmr::u8string と、検索用の u8string_view を比較する。
	*/
	Bool InternPool::TransparentEqual::operator()(const std::pmr::u8string& a, std::u8string_view b)const noexcept
	{
		return a == b;
	}

	/**
	* [EN]
	* Compares a lookup u8string_view against a stored pmr::u8string
	* (mirror of the previous overload for the other argument order).
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 検索用の u8string_view と、格納済みの pmr::u8string を比較する
	* （引数順が逆のオーバーロード）。
	*/
	Bool InternPool::TransparentEqual::operator()(std::u8string_view a, const std::pmr::u8string& b)const noexcept
	{
		return a == b;
	}

	/**
	* [EN]
	* Compares two stored pmr::u8strings for content equality.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 格納済みの pmr::u8string 同士の内容が等しいか比較する。
	*/
	Bool InternPool::TransparentEqual::operator()(const std::pmr::u8string& a, const std::pmr::u8string& b)const noexcept
	{
		return a == b;
	}
}

namespace SeedCore
{
	/**
	* [EN]
	* Returns the process-wide InternPool instance.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* プロセス全体で共有される InternPool インスタンスを返す。
	*/
	InternPool& InternPool::Singleton()
	{
		static InternPool instance;
		return instance;
	}

	/**
	* [EN]
	* Constructs the pool, binding strings_ to arena_ for allocation.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* プールを構築し、strings_ の割り当て先として arena_ を紐付ける。
	*/
	InternPool::InternPool() : arena_(std::pmr::get_default_resource()), strings_(&arena_)
	{
		/// No Code
	}
}

namespace SeedCore
{
	/**
	* [EN]
	* Converts a UTF-8 string to a UTF-16 (wchar_t) string, for Win32 API calls.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* UTF-8 文字列を UTF-16（wchar_t）文字列に変換する。Win32 API 呼び出し用。
	*/
	std::wstring ConvertToWideString(std::string_view view)
	{
		if (view.empty())
		{
			return std::wstring();
		}

		Size length = MultiByteToWideChar(CP_UTF8, 0, view.data(), (Int)view.size(), nullptr, 0);
		if (length == static_cast<Size>(-1))
		{
			return std::wstring();
		}

		std::wstring wstring(static_cast<Int>(length), L'\0');
		MultiByteToWideChar(CP_UTF8, 0, view.data(), (Int)view.size(), wstring.data(), static_cast<Int>(length));
		return wstring;
	}

	/**
	* [EN]
	* Converts a UTF-16 (wchar_t) string to a UTF-8 string.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* UTF-16（wchar_t）文字列を UTF-8 文字列に変換する。
	*/
	std::string ConvertToCharString(std::wstring_view view)
	{
		if (view.empty())
		{
			return std::string();
		}

		Size length = WideCharToMultiByte(CP_UTF8, 0, view.data(), static_cast<Int>(view.size()), nullptr, 0, nullptr, nullptr);
		if (length == static_cast<Size>(-1))
		{
			return std::string();
		}

		std::string string(static_cast<Int>(length), '\0');
		WideCharToMultiByte(CP_UTF8, 0, view.data(), static_cast<Int>(view.size()), string.data(), static_cast<Int>(length), nullptr, nullptr);
		return string;
	}
}