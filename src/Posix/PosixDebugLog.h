#pragma once

#ifndef _WIN32
#include "DebugLog.h"
#include <sched.h>
#include <string_view>

class PosixDebugLog : public DebugLog
{
public:
	PosixDebugLog();
	~PosixDebugLog();

	bool write(std::string_view);
	bool isClosed();
	void flush();

private:
	int fdWrite{};
	pid_t childPid{};
	bool _wrote{};
};


#endif
