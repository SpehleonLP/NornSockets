#pragma once
#include "../SharedMemoryInterface.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <ddeml.h>

#define RESTART_CONVERSATION 0

class VivariumInterface : public SharedMemoryInterface
{
struct DDE_Strings;
public:
	static std::unique_ptr<SharedMemoryInterface> OpenVivarium();

	VivariumInterface(int major, int minor);
	~VivariumInterface();

	Response send1252(std::string &) override;
	bool isClosed() override;

private:
	HCONV fMonitor{};
	bool _isOpen{true};
};

#endif
