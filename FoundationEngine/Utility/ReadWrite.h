#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	/**
	* [EN]
	* Tag type marking a query/access parameter as read-only. Type
	* resolves to const T, used by query systems (e.g. ECS Query) to
	* determine component access mode at compile time.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* クエリ/アクセスパラメータを読み取り専用としてマークするタグ型。
	* Type は const T に解決され、クエリシステム（ECS の Query など）が
	* コンポーネントのアクセスモードをコンパイル時に判定するために使う。
	*/
	template<typename T>
	struct Read
	{
		using Type = const T;
	};

	/// [EN] Satisfied by Read<T>-shaped tag types (i.e. Type is const-qualified).
	/// [JP] Read<T> 形のタグ型（すなわち Type が const 修飾されている）を満たす。
	template<typename T>
	concept IsReadAccess = requires{typename T::Type;} && std::is_const_v<typename T::Type>;

	/**
	* [EN]
	* Tag type marking a query/access parameter as read-write. Type
	* resolves to T (unqualified), used by query systems (e.g. ECS
	* Query) to determine component access mode at compile time.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* クエリ/アクセスパラメータを読み書き可能としてマークするタグ型。
	* Type は T（非修飾）に解決され、クエリシステム（ECS の Query など）が
	* コンポーネントのアクセスモードをコンパイル時に判定するために使う。
	*/
	template<typename T>
	struct Write
	{
		using Type = T;
	};

	/// [EN] Satisfied by Write<T>-shaped tag types (i.e. Type is not const-qualified).
	/// [JP] Write<T> 形のタグ型（すなわち Type が const 修飾されていない）を満たす。
	template<typename T>
	concept IsWriteAccess = requires{typename T::Type;} && !std::is_const_v<typename T::Type>;
}