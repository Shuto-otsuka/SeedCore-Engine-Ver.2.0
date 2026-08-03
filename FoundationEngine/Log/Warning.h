#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/Log/LogSystem.h>

/**
* [EN]
* Pushes a Warning-level entry to LogSystem, formatted with fmt and
* tagged with the current file/line.
*
* ---------------------------------------------------------------------
*
* [JP]
* fmt で整形し、現在のファイル/行を付与した Warning レベルのエントリを
* LogSystem に追加する。
*/
#define SC_LOG_WARNING(fmt, ...) \
	SeedCore::LogSystem::Push(SeedCore::LogLevel::Warning, std::format(fmt, ##__VA_ARGS__), __FILE__, __LINE__)
