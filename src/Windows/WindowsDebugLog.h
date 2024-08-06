#pragma once
#include "../DebugLog.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

class WindowsDebugLog : public DebugLog
{
public:
	WindowsDebugLog();
	~WindowsDebugLog();

	bool write(std::string_view) override;
	bool isClosed() override;
	void flush() override;

private:
	PROCESS_INFORMATION pi;
	HANDLE hReadPipe{};
	HANDLE hWritePipe{};
	bool wrote{};
};

#endif
