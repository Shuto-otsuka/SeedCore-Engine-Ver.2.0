#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	/// [EN] Severity of a LogEntry.
	/// [JP] LogEntry の重大度。
	enum class LogLevel : Uint8
	{
		/// [EN] Informational message.
		/// [JP] 情報メッセージ。
		Notice,

		/// [EN] Non-fatal issue that should be looked at.
		/// [JP] 致命的ではないが確認すべき問題。
		Warning,

		/// [EN] Failure.
		/// [JP] 失敗。
		Error
	};

	/**
	* [EN]
	* A single recorded log line, as stored by LogSystem.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* LogSystem に記録される1件のログ行。
	*/
	struct LogEntry
	{
		/// [EN] Severity of this entry.
		/// [JP] このエントリの重大度。
		LogLevel level_;

		/// [EN] Formatted log message.
		/// [JP] 整形済みのログメッセージ。
		String message_;

		/// [EN] Source file name (basename only) that logged this entry.
		/// [JP] このエントリをログしたソースファイル名（ベース名のみ）。
		String file_;

		/// [EN] Source line number that logged this entry.
		/// [JP] このエントリをログしたソースの行番号。
		Int line_;
	};

	/**
	* [EN]
	* Process-wide, in-memory log buffer. Entries are pushed via the
	* SC_LOG_NOTICE/SC_LOG_WARNING/SC_LOG_ERROR macros and read back
	* by the Editor's Console panel.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* プロセス全体で共有される、メモリ上のログバッファ。エントリは
	* SC_LOG_NOTICE/SC_LOG_WARNING/SC_LOG_ERROR マクロ経由で追加され、
	* Editor の Console パネルから読み出される。
	*/
	class SEEDCORE_API LogSystem
	{
	public:
		/**
		* [EN]
		* Appends a new LogEntry built from level/message and the
		* calling file/line (file is reduced to its basename).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* level/message と呼び出し元のファイル/行から LogEntry を
		* 構築して追加する（file はベース名に縮約される）。
		*/
		static void Push(LogLevel level, const std::string& message, const Char* file, Int line);

		/**
		* [EN]
		* Returns all entries logged so far, in push order.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* これまでに記録された全エントリを、追加順で返す。
		*/
		static const DynamicArray<LogEntry>& GetLogs();

		/**
		* [EN]
		* Removes all logged entries.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 記録済みの全エントリを削除する。
		*/
		static void Clear();

	private:
		/// [EN] Backing storage for all logged entries.
		/// [JP] 全ログエントリを保持するストレージ。
		static DynamicArray<LogEntry> logs_;
	};
}
