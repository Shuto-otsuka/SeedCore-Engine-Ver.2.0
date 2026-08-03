#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/Log/LogSystem.h>

/**
* [EN]
* Pushes a Notice-level entry to LogSystem, formatted with fmt and
* tagged with the current file/line.
*
* ---------------------------------------------------------------------
*
* [JP]
* fmt で整形し、現在のファイル/行を付与した Notice レベルのエントリを
* LogSystem に追加する。
*/
#define SC_LOG_NOTICE(fmt, ...) \
	SeedCore::LogSystem::Push(SeedCore::LogLevel::Notice, std::format(fmt, ##__VA_ARGS__), __FILE__, __LINE__)
