#pragma once
#include <FoundationEngine/Utility/Types.h>

#include <string>
#include <memory_resource>
#include <unordered_set>

namespace SeedCore
{
	/**
	* [EN]
	* @brief Global backend for string interning.
	*
	* Initial design direction:
	* - InternPool was originally planned as a non-singleton storage object.
	* - The goal was to allow multiple ownership domains such as Scene,
	*   World, AssetSystem, or runtime-local storage contexts.
	* - This was intended to avoid hidden global state and reduce coupling.
	*
	* Why the design changed:
	* - Passing pool ownership through gameplay-facing systems caused
	*   architectural pollution and excessive infrastructure exposure.
	* - Scene/World level ownership made higher-level systems feel like
	*   memory containers rather than gameplay abstractions.
	* - Service/Context based access also introduced additional indirection
	*   and made storage infrastructure visible throughout the engine.
	* - The actual usage model only requires:
	*
	*       String(u8"...")
	*
	*   while all interning behavior should remain completely internal.
	*
	* Current direction:
	* - InternPool is implemented as a hidden singleton-style backend.
	* - The pool is considered internal engine infrastructure, not part of
	*   gameplay architecture.
	* - Game code should never access InternPool directly.
	* - String / InternedString types are the intended public abstraction.
	*
	* Design characteristics:
	* - Process-wide append-only cache.
	* - Uses monotonic_buffer_resource for stable string lifetime.
	* - Interned strings remain valid until pool destruction.
	* - Optimized for identifier-like strings:
	*      tags, asset paths, type names, shader names, etc.
	*
	* Important:
	* - Dynamic runtime formatted strings should not be interned aggressively.
	* - Destroying the pool invalidates all interned string pointers.
	* - Intended lifetime is effectively the entire engine runtime.
	*
	* TODO:
	* - Investigate sharded/thread-local lookup optimization.
	* - Consider freeze/read-only mode after initialization.
	* - Evaluate whether all String instances should intern automatically,
	*   or only specific identifier-oriented string types.
	* - Improve StringLike concept constraints.
	* - Add diagnostics for excessive unique interned strings.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* @brief 文字列インターン用のグローバルバックエンド。
	*
	* 初期設計方針:
	* - InternPool は元々 singleton にしないストレージオブジェクトとして
	*   設計する予定だった。
	* - Scene、World、AssetSystem、またはランタイムローカルな
	*   ストレージコンテキストなど、複数の所有ドメインを許可することが目的だった。
	* - これは hidden global state を避け、結合度を下げるためだった。
	*
	* なぜ設計を変更したか:
	* - gameplay 系システムへ pool ownership を伝搬させることで、
	*   アーキテクチャ汚染とインフラ露出が発生した。
	* - Scene / World に所有させると、
	*   上位システムが gameplay abstraction ではなく
	*   memory container のようになってしまった。
	* - Service / Context ベースのアクセスも、
	*   不要な間接化を増やし、
	*   ストレージインフラをエンジン全体へ露出させてしまった。
	* - 実際の利用モデルは:
	*
	*       String(u8"...")
	*
	*   のみを要求しており、
	*   intern 処理自体は完全に内部実装であるべきだった。
	*
	* 現在の方針:
	* - InternPool は hidden singleton-style backend として実装する。
	* - pool は gameplay architecture ではなく、
	*   internal engine infrastructure として扱う。
	* - game code から InternPool を直接触ってはいけない。
	* - String / InternedString が公開 abstraction となる。
	*
	* 設計特性:
	* - process-wide append-only cache。
	* - stable string lifetime のため monotonic_buffer_resource を使用。
	* - interned string は pool 破棄まで有効。
	* - 以下のような identifier 系文字列向けに最適化:
	*      tag、asset path、type name、shader name など。
	*
	* 注意:
	* - 動的な runtime formatted string を大量 intern するべきではない。
	* - pool を破棄すると、全 interned string pointer は無効化される。
	* - 想定 lifetime は実質 engine runtime 全体。
	*
	* TODO:
	* - sharded / thread-local lookup 最適化の検討。
	* - 初期化後 freeze/read-only mode の検討。
	* - 全 String を自動 intern するか、
	*   identifier 専用 string type のみに限定するか検討。
	* - StringLike concept 制約の改善。
	* - interned string の過剰増加検出用 diagnostics の追加。
	*/
	class SEEDCORE_API InternPool
	{
	public:
		/**
		* [EN]
		* Returns the process-wide InternPool instance.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* プロセス全体で共有される InternPool インスタンスを返す。
		*/
		static InternPool& Singleton();

	public:
		/**
		* [EN]
		* Heterogeneous hash functor allowing lookups by u8string_view
		* without constructing a temporary pmr::u8string key.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 一時的な pmr::u8string キーを構築せずに u8string_view で
		* 検索できるようにする、透過的なハッシュ関数オブジェクト。
		*/
		struct TransparentHash
		{
			using is_transparent = void;

			Size operator()(std::u8string_view view)const noexcept;

			Size operator()(const std::pmr::u8string& string)const noexcept;
		};

		/**
		* [EN]
		* Heterogeneous equality functor pairing with TransparentHash,
		* comparing any combination of u8string_view/pmr::u8string.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* TransparentHash と対になる透過的な等価比較関数オブジェクト。
		* u8string_view/pmr::u8string の任意の組み合わせを比較できる。
		*/
		struct TransparentEqual
		{
			using is_transparent = void;

			Bool operator()(std::u8string_view a, std::u8string_view b)const noexcept;

			Bool operator()(const std::pmr::u8string& a, std::u8string_view b)const noexcept;

			Bool operator()(std::u8string_view a, const std::pmr::u8string& b)const noexcept;

			Bool operator()(const std::pmr::u8string& a, const std::pmr::u8string& b)const noexcept;
		};

	public:
		InternPool();

		/**
		* [EN]
		* Interns view, returning an instance of T constructed from
		* the pool's stable copy of the string. Returns T() for an
		* empty view without touching the pool. Thread-safe.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* view をインターンし、プール内で安定して保持される文字列
		* コピーから構築した T のインスタンスを返す。空の view の場合は
		* プールに触れずに T() を返す。スレッドセーフ。
		*/
		template<typename T>
		T Intern(std::u8string_view view)
		{
			if (view.empty())
			{
				return T();
			}

			std::lock_guard<std::mutex> lock(mutex_);

			auto it = strings_.find(view);
			if (it != strings_.end())
			{
				return T(it->c_str(), it->size());
			}
			auto result = strings_.emplace(std::pmr::u8string(view, &arena_));

			return T(result.first->c_str(), result.first->size());
		}

	private:
		/// [EN] Guards arena_/strings_ for concurrent Intern calls.
		/// [JP] 並行する Intern 呼び出しから arena_/strings_ を保護する。
		std::mutex mutex_;

		/// [EN] Backing memory resource for interned strings; append-only for
		///      the process lifetime, giving interned data a stable address.
		/// [JP] インターン済み文字列を保持するメモリリソース。プロセスの
		///      生存期間中は追記のみで、インターン済みデータのアドレスは安定する。
		std::pmr::monotonic_buffer_resource arena_;

		/// [EN] Deduplicated set of interned strings, keyed transparently.
		/// [JP] 重複を排除したインターン済み文字列の集合。透過的にキー検索できる。
		std::pmr::unordered_set<std::pmr::u8string, TransparentHash, TransparentEqual> strings_;
	};

	/**
	* [EN]
	* Converts a UTF-8 string to a UTF-16 (wchar_t) string, for Win32 API calls.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* UTF-8 文字列を UTF-16（wchar_t）文字列に変換する。Win32 API 呼び出し用。
	*/
	SEEDCORE_API std::wstring ConvertToWideString(std::string_view view);

	/**
	* [EN]
	* Converts a UTF-16 (wchar_t) string to a UTF-8 string.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* UTF-16（wchar_t）文字列を UTF-8 文字列に変換する。
	*/
	SEEDCORE_API std::string ConvertToCharString(std::wstring_view view);
}