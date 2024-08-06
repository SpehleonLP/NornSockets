#include "DebugLog.h"
#include <mutex>

#ifdef _WIN32
#include "Windows/WindowsDebugLog.h"

std::unique_ptr<DebugLog> DebugLog::Open()
{
	return std::unique_ptr<DebugLog>(new WindowsDebugLog);
}
#else
#include "Posix/PosixDebugLog.h"

std::unique_ptr<DebugLog> DebugLog::Open()
{
	return std::unique_ptr<DebugLog>(new PosixDebugLog);
}

#endif

static std::mutex g_dbgMutex;
static std::vector<std::string> & g_dbgLog()
{
	static std::vector<std::string> r;
	return r;
}

void DebugLog::WriteDebugMessage(std::string_view msg)
{
	std::lock_guard lock(g_dbgMutex);
	g_dbgLog().emplace_back(msg);
}


std::vector<std::string> DebugLog::GetDebugLog()
{
	std::lock_guard lock(g_dbgMutex);

	std::vector<std::string> r;
	r.swap(g_dbgLog());
	return r;
}

