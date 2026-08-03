#pragma once
#include <FoundationEngine/Utility/Types.h>
#include <string>

namespace SeedCore
{
	/**
	* [EN]
	* Immutable, interned, reference-counted-free string type. Every
	* distinct string value is stored once by InternPool (see
	* FoundationEngine/Pool/InternPool.h), so a String is a lightweight
	* view into that shared storage: copies are cheap (a pointer/size
	* copy) and equality is a pointer comparison, not a content compare.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 不変で、インターン化され、参照カウント不要な文字列型。個々の異なる
	* 文字列値は InternPool（FoundationEngine/Pool/InternPool.h 参照）に
	* 一度だけ格納されるため、String はその共有ストレージへの軽量な view
	* である。コピーは軽量（ポインタ/サイズのコピー）で、等価比較は内容比較
	* ではなくポインタ比較になる。
	*/
	class SEEDCORE_API String
	{
	public:
		/**
		* [EN]
		* Interns view (UTF-8) and returns the corresponding String.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* view（UTF-8）をインターンし、対応する String を返す。
		*/
		static String intern(std::string_view view);

		/**
		* [EN]
		* Interns view (UTF-16), converting it to UTF-8 first.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* view（UTF-16）を先に UTF-8 へ変換したうえでインターンする。
		*/
		static String intern(std::wstring_view view);

		/**
		* [EN]
		* Interns a NUL-terminated UTF-8 (char8_t) string.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* NUL終端のUTF-8（char8_t）文字列をインターンする。
		*/
		static String intern(const Char8* string);

		/**
		* [EN]
		* Interns view (UTF-8 as char8_t). This is the primitive that every
		* other intern overload eventually calls.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* view（char8_t としてのUTF-8）をインターンする。他の全ての intern
		* オーバーロードは最終的にこれを呼び出す。
		*/
		static String intern(std::u8string_view view);

		String() = default;

		/**
		* [EN]
		* Constructs by interning a NUL-terminated UTF-8 string literal
		* (const char*). Non-explicit so string literals convert directly
		* to String without going through std::string_view first, since
		* chaining two implicit user-defined conversions (const char* ->
		* std::string_view -> String) is not allowed by the language.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* NUL終端のUTF-8文字列リテラル（const char*）をインターンして構築
		* する。非explicitにすることで、文字列リテラルが std::string_view を
		* 経由せず直接 String へ変換できるようにする（const char* ->
		* std::string_view -> String という2段階の暗黙のユーザー定義変換は
		* 言語仕様上許可されないため）。
		*/
		String(const Char* text);

		/**
		* [EN]
		* Constructs by interning view (UTF-8 as char8_t).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* view（char8_t としてのUTF-8）をインターンして構築する。
		*/
		String(std::u8string_view view);

		/**
		* [EN]
		* Constructs by interning view (UTF-8).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* view（UTF-8）をインターンして構築する。
		*/
		String(std::string_view view);

		/**
		* [EN]
		* Constructs by interning view (UTF-16).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* view（UTF-16）をインターンして構築する。
		*/
		String(std::wstring_view view);

		/**
		* [EN]
		* Returns a view onto the interned UTF-8 (char8_t) data.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* インターン済みのUTF-8（char8_t）データへの view を返す。
		*/
		constexpr std::u8string_view view()const noexcept
		{
			return view_;
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
		const Char* c_str()const;

		/**
		* [EN]
		* Returns a copy of the interned data as std::string (UTF-8).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* インターン済みデータのコピーを std::string（UTF-8）として返す。
		*/
		std::string str()const;

		/**
		* [EN]
		* Returns a copy of the interned data converted to std::wstring (UTF-16).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* インターン済みデータを std::wstring（UTF-16）に変換したコピーを返す。
		*/
		std::wstring w_str()const;

		/**
		* [EN]
		* Returns a copy of the interned data as std::u8string.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* インターン済みデータのコピーを std::u8string として返す。
		*/
		std::u8string u8str()const;

		/**
		* [EN]
		* Returns whether first and second reference the same interned
		* storage (pointer comparison, not content comparison).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* first と second が同じインターン済みストレージを参照しているかを
		* 返す（内容比較ではなくポインタ比較）。
		*/
		friend Bool operator==(String first, String second)noexcept
		{
			return first.view_.data() == second.view_.data();
		}

		/**
		* [EN]
		* Negation of operator==.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* operator== の否定。
		*/
		friend Bool operator!=(String first, String second)noexcept
		{
			return !(first == second);
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
		explicit operator std::string()const;

		/**
		* [EN]
		* Explicit conversion to std::u8string, equivalent to u8str().
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* std::u8string への明示的な変換。u8str() と同等。
		*/
		explicit operator std::u8string()const;

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
		explicit operator std::string_view()const;

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
		explicit operator std::u8string_view()const;

	private:
		/// [EN] View onto the interned UTF-8 (char8_t) storage owned by InternPool.
		/// [JP] InternPool が所有するインターン済みUTF-8（char8_t）ストレージへの view。
		std::u8string_view view_;

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
		explicit String(const Char8* view, Size size);

		friend class InternPool;
	};
}

namespace std
{
	/**
	* [EN]
	* Specializes std::hash for String so it can be used as a key in
	* std::unordered_map/std::unordered_set, hashing the underlying
	* interned pointer rather than the string's content.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* String を std::unordered_map/std::unordered_set のキーとして使える
	* ようにする std::hash の特殊化。文字列の内容ではなく、背後にある
	* インターン済みポインタをハッシュ化する。
	*/
	template<>
	struct hash<SeedCore::String>
	{
		/**
		* [EN]
		* Hashes string's interned pointer.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* string のインターン済みポインタをハッシュ化する。
		*/
		size_t operator()(const SeedCore::String& string)const noexcept
		{
			return std::hash<const void*>{}(string.view().data());
		}
	};
}