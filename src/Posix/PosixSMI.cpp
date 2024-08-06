#include "PosixSMI.h"

#ifndef _WIN32
#include <netinet/tcp.h>
#include "Support.h"
#include <filesystem>
#include <fstream>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>

int PosixSMI::_connectionReset = 0;

enum
{
	BUFFER_SIZE = 4096
};

std::unique_ptr<SharedMemoryInterface> PosixSMI::Create()
{
	int port = 0;
	int _socket = 0;
	struct sockaddr_in serv_addr;
	memset(&serv_addr, 0, sizeof(serv_addr));

	static std::filesystem::path path;

	if(path.empty())
	{
	  auto home = getenv("HOME");
	  path = std::filesystem::path(home) /= ".creaturesengine/port";
	}

	if(!std::filesystem::exists(path))
		return nullptr;

	static std::filesystem::file_time_type lastModified;
	auto currModifed = std::filesystem::last_write_time(path);

	if(currModifed == lastModified)
	{
		if(_connectionReset == false)
			return nullptr;

		fprintf(stderr, "trying to reconnect...\n");
		_connectionReset = false;
	}
	else
	{
		lastModified = currModifed;

		{
			std::ifstream file(path, std::ios_base::in);
			if(file.is_open() == false)
				return nullptr;

			file >> port;
			file.close();
		}
	}

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(port);

	if(inet_aton("127.0.0.1", (struct in_addr *) &(serv_addr.sin_addr.s_addr)) < 0)
	{
		fprintf(stderr, "Invalid address/ Address not supported.\n");
		return nullptr;
	}

	// Create socket
	if ((_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		fprintf(stderr, "Socket creation error: %s\n", strerror(errno));
		return nullptr;
	}

	// Connect to the server
	if (connect(_socket, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {

		close(_socket);
		fprintf(stderr, "Connection failed: %s\n", strerror(errno));
		return nullptr;
	}

	return std::unique_ptr<SharedMemoryInterface>(new PosixSMI(_socket));
}

PosixSMI::PosixSMI(int socket) :
	_socket(socket)
{
	int _enable{1};
	int _keepAliveTime{45};

	struct timeval timeout;
	timeout.tv_sec = 0;  // 0 seconds
	timeout.tv_usec = 100 * 1000; // 100 miliseconds

	setsockopt(_socket, SOL_SOCKET, SO_KEEPALIVE, &_enable, sizeof(_enable));
	setsockopt(_socket, IPPROTO_TCP, TCP_KEEPIDLE, &_keepAliveTime, sizeof(_keepAliveTime));
	setsockopt(_socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
	setsockopt(_socket, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
	setsockopt(_socket, SOL_SOCKET, SO_REUSEADDR, &_enable, sizeof(_enable));
};

PosixSMI::~PosixSMI()
{
	auto error = close(_socket);

	if(error < 0)
	{
		try
		{
			auto message = HandleError();

			if(message)
				fprintf(stderr, "%s\n", message);
		}
		catch(std::exception & e)
		{
			fprintf(stderr, "%s\n", e.what());
		}
	}
}


PosixSMI::Response PosixSMI::send1252(std::string & message)
{
	if(message.capacity() < message.size()+8)
		message.reserve(message.size()+8);

	const char * error = nullptr;
	std::string response;
	std::string_view retn;

	auto chunks = ChunkMessage(message, 64000-10);
	_timeout = false;

	char buffer[8];
	message.resize(message.size()+8, '\0');

	for(std::string_view & chunk : chunks)
	{
		memcpy(buffer, &chunk.back(), 8);
		strncpy(const_cast<char*>(&chunk.back()+1), "\nrscr\n", 8);
		// second time in this loop causes: SIGPIPE: broken pipe
		auto r = ::send(_socket, chunk.data(), chunk.size()+8, MSG_NOSIGNAL);
		memcpy(const_cast<char*>(&chunk.back()), buffer, 8);

		if(r == -1)
		{
			error = HandleError();

			if(error)
			{
				return Response{
					.text= error,
					.isError=true,
					.isBinary=false,
				};
			}
		}

	// read the reply
		do
		{
			auto retn = read_message(error);
			response += retn;
		} while(retn.size() == BUFFER_SIZE);

		if(error)
		{
			return Response{
				.text= error,
				.isError=true,
				.isBinary=false,
			};
		}
	}

// just try it a second time (i guess)?
	if(_timeout)
	{
		do
		{
			auto retn = read_message(error);
			response += retn;
		} while(retn.size() == BUFFER_SIZE);

		if(error)
		{
			return Response{
				.text= error,
				.isError=true,
				.isBinary=false,
			};
		}

		_timeout = false;

	}

	return Response{
		.text= std::move(response),
		.isError=false,
		.isBinary=false,
	};
}

std::string_view PosixSMI::read_message(const char *& error)
{
	errno = 0;
	static char buffer[BUFFER_SIZE];
	auto length = recv(_socket, buffer, sizeof(buffer), MSG_NOSIGNAL);

	if(length > 0)
	{
		return std::string_view(buffer, length);
	}
	else if(length != 0)
	{
		error = HandleError();
	}

	if(errno)
	{
		HandleError();
	}

	return {};
}

const char * PosixSMI::HandleError()
{
	auto error = errno;
	errno = 0;

	switch(error)
	{
		case EWOULDBLOCK:
			break;
		case EBADF:
			throw std::logic_error("socket not a valid file descriptor?");
		case ECONNRESET:
			_isClosed = true;
			return "Connection reset by host";
			break;
		case EINTR:
			_isClosed = true;
			return "Caught signal";
			break;
		case EPIPE:
			_isClosed = true;
			_connectionReset = true;
			return 	"Connection reset by peer";
		case EINVAL:
			_isClosed = true;
			return "No out of band data available";
			break;
		case ENOTCONN:
			throw std::logic_error("socket not connected?");
		case ENOTSOCK:
			throw std::logic_error("socket not a socket?");
		case EOPNOTSUPP:
			throw std::logic_error("flags not supported.");
		case ETIMEDOUT:
			_timeout = true;
			break;
	}

	return nullptr;
}


bool PosixSMI::isClosed()
{
// just count on us polling the socket for debug information as notifying us when it closed i guess?
	return _isClosed;
}


#endif
