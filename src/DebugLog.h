#pragma once
#include <string_view>
#include <memory>
#include <vector>
#include <string>

// represents an external process,
// - should be a cmd window, write to it with write(), test if user closed it with isClosed
class DebugLog
{
public:
	static void WriteDebugMessage(std::string_view msg);
	static std::vector<std::string> GetDebugLog();

	static std::unique_ptr<DebugLog> Open();

	DebugLog() = default;
	virtual ~DebugLog() = default;

	virtual bool write(std::string_view) = 0;
	virtual bool isClosed() = 0;
	virtual void flush() = 0;
};

