#pragma once
#include "../SharedMemoryInterface.h"

#ifndef _WIN32


class PosixSMI : public SharedMemoryInterface
{
public:
	static std::unique_ptr<SharedMemoryInterface> Create();

	PosixSMI(int socket);
	~PosixSMI();

	Response send1252(std::string &) override;
	bool isClosed() override;

	std::string_view read_message(const char *& error);

private:
	static int _connectionReset;
	const char * HandleError();

	int _socket{};
	bool _isClosed{false};
	bool _timeout{false};
};
#endif
