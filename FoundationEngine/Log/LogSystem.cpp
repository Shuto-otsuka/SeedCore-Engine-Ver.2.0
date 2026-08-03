#include <FoundationEngine/Log/LogSystem.h>

namespace SeedCore
{
	DynamicArray<LogEntry> LogSystem::logs_;

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
	void LogSystem::Push(LogLevel level, const std::string& message, const Char* file, Int line)
	{
		logs_.push_back({ level, String(message), String(std::filesystem::path(file).filename().string()), line });
	}

	/**
	* [EN]
	* Returns all entries logged so far, in push order.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* これまでに記録された全エントリを、追加順で返す。
	*/
	const DynamicArray<LogEntry>& LogSystem::GetLogs()
	{
		return logs_;
	}

	/**
	* [EN]
	* Removes all logged entries.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 記録済みの全エントリを削除する。
	*/
	void LogSystem::Clear()
	{
		logs_.clear();
	}
}
