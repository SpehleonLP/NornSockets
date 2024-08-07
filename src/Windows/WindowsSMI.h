#pragma once
#include "../SharedMemoryInterface.h"


#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

class WindowsSMI : public SharedMemoryInterface
{
public:
struct Message;
	static std::unique_ptr<SharedMemoryInterface> Open(const char* name);

	WindowsSMI();
	~WindowsSMI();

	Response send1252(std::string &) override;
	bool isClosed() override;

private:
	void Initialize();

	HANDLE memory_handle{};
	Message* memory_ptr{};
	HANDLE mutex{};
	HANDLE result_event{};
	HANDLE request_event{};
	HANDLE creator_process_handle{};
	bool _isClosed{};
};

struct WindowsSMI::Message
{
	char c2e_[4];
	unsigned int pid;
	int  resultCode;
	unsigned int byteLength;
	unsigned int memBufferSize;
	unsigned int pad00;
	char contents[];
};
#endif
