#pragma once

namespace SeedCore
{
	/**
	* [EN]
	* Primary template (undefined); only the std::variant<Ts...> partial
	* specialization below is usable. Resolves the compile-time index of
	* T within a std::variant's alternative list.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* プライマリテンプレート（未定義）。以下の std::variant<Ts...>
	* 部分特殊化のみが使用可能。std::variant の代替型リスト内における
	* T のコンパイル時インデックスを求める。
	*/
	template <typename T, typename>
	struct GetIndex;

	/**
	* [EN]
	* Recursion base case: T not found among Ts... (no member value,
	* so use with a mismatched T is a compile error).
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 再帰の基底ケース: T が Ts... の中に見つからない（value メンバを
	* 持たないため、一致しない T での使用はコンパイルエラーになる）。
	*/
	template <Size I, typename... Ts>
	struct GetIndexImplementation {};

	/**
	* [EN]
	* Recursion match case: the type at the current position equals T; yields index I.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 再帰の一致ケース: 現在位置の型が T と一致する。インデックス I を返す。
	*/
	template <Size I, typename T, typename... Ts>
	struct GetIndexImplementation<I, T, T, Ts...> : std::integral_constant<Size, I> {};

	/**
	* [EN]
	* Recursion step case: the type at the current position doesn't
	* match T; advance to the next position and increment the index.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 再帰のステップケース: 現在位置の型が T と一致しない。次の位置へ
	* 進み、インデックスをインクリメントする。
	*/
	template <Size I, typename T, typename U, typename... Ts>
	struct GetIndexImplementation<I, T, U, Ts...> : GetIndexImplementation<I + 1, T, Ts...> {};

	/**
	* [EN]
	* Entry point: extracts the variant's alternative types Ts... and
	* starts the search from index 0.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* エントリポイント: variant の代替型 Ts... を取り出し、
	* インデックス0から検索を開始する。
	*/
	template <typename T, typename... Ts>
	struct GetIndex<T, std::variant<Ts...>> : GetIndexImplementation<0, T, Ts...> {};

	/// [EN] Convenience variable template: the compile-time index of T within
	///      std::variant<Ts...>, e.g. GetIndexValue<Foo, MyVariant>.
	/// [JP] 利便性のための変数テンプレート: std::variant<Ts...> 内における
	///      T のコンパイル時インデックス。例: GetIndexValue<Foo, MyVariant>。
	template <typename T, typename... Ts>
	constexpr auto GetIndexValue = GetIndex<T, Ts...>::value;
}
