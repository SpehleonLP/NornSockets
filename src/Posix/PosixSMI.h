#pragma once
#include "../SharedMemoryInterface.h"

#ifndef _WIN32
#include <netinet/in.h>


class PosixSMI : public SharedMemoryInterface
{
public:
	static std::unique_ptr<SharedMemoryInterface> Create();

	PosixSMI(sockaddr_in & serv_addr, int port);
	~PosixSMI();

	Response send1252(std::string &) override;
	bool isClosed() override;


private:
struct Socket;
	static pid_t GetPid(int port);
	static int _connectionReset;

	sockaddr_in serv_addr;
	pid_t _pid{};
	bool _isClosed{false};
	bool _timeout{false};
};

struct PosixSMI::Socket
{
	Socket(PosixSMI & parent);
	~Socket();

	std::string_view read_message(const char *& error);
	const char * HandleError();

	int _socket{};
	PosixSMI & parent;
};

#endif
