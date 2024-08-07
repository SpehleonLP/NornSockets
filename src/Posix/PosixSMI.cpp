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
	sockaddr_in serv_addr;
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
		throw std::runtime_error("Invalid address/ Address not supported.");
	}

	return std::unique_ptr<SharedMemoryInterface>(new PosixSMI(serv_addr, port));
}

PosixSMI::PosixSMI(sockaddr_in & serv_addr, int port) :
	serv_addr(serv_addr)
{
	_engine = "C2E";

	try
	{
		_pid = GetPid(port);

		if(_pid)
		{
			_workingDirectory = GetWorkingDirectory(_pid);
		}

		char buffer[256];
		buffer[0] = 0;
		std::string cmd = "outv vmjr outs \" \" outv vmnr\n outs \" \" outx gnam";
		auto version = PosixSMI::send1252(cmd);

		int r = sscanf(version.text.c_str(), "%d %d \"%255[^\"]\"", &versionMajor, &versionMinor, buffer);

		if (r < 3)
		{
			fprintf(stderr, "failed to get version of engine.\n");
		}
		else
			_name = *buffer == '"'? buffer+1 : buffer;
	}
	catch(std::exception & e)
	{
		fprintf(stderr, "failed to get working directory of engine: %s", e.what());
	}
};

PosixSMI::~PosixSMI()
{
}


PosixSMI::Response PosixSMI::send1252(std::string & message)
{
	if(_isClosed)
	{
		return Response{
			.text= "Port is not open.",
			.isError=true,
			.isBinary=false,
		};
	}

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
		try
		{
			Socket socket(*this);

			memcpy(buffer, &chunk.back(), 8);
			strncpy(const_cast<char*>(&chunk.back()+1), "\nrscr\n", 8);
			// second time in this loop causes: SIGPIPE: broken pipe
			auto r = ::send(socket._socket, chunk.data(), chunk.size()+8, MSG_NOSIGNAL);
			memcpy(const_cast<char*>(&chunk.back()), buffer, 8);

			if(r == -1)
			{
				error = socket.HandleError();

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
				auto retn = socket.read_message(error);
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
		catch(std::exception & e)
		{
			_isClosed = true;

			return Response{
				.text= e.what(),
				.isError=true,
				.isBinary=false,
			};
		}
	}

	return Response{
		.text= std::move(response),
		.isError=false,
		.isBinary=false,
	};
}


bool PosixSMI::isClosed()
{
// just count on us polling the socket for debug information as notifying us when it closed i guess?
	return _isClosed;
}


pid_t PosixSMI::GetPid(int port)
{
	auto exec = [](const char* cmd) -> std::string {
		std::array<char, 128> buffer;
		std::string result;
		std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
		if (!pipe) {
			throw std::runtime_error("popen() failed!");
		}
		while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
			result += buffer.data();
		}
		return result;
	};

	auto command = "lsof -i tcp:" + std::to_string(port) + " | grep LISTEN";
	std::string output = exec(command.c_str());

	char process_name[64];
	pid_t pid{};

	sscanf(output.c_str(), "%63s %d", process_name, &pid);
	return pid;
}

PosixSMI::Socket::Socket(PosixSMI &parent) :
	parent(parent)
{
	sockaddr_in& serv_addr = parent.serv_addr;

	// Create socket
	if ((_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		throw std::runtime_error("Socket creation error: " + std::string(strerror(errno)));
	}

	// Connect to the server
	if (connect(_socket, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {

		close(_socket);
		throw std::runtime_error("Connection failed: " + std::string(strerror(errno)));
	}

	int _enable{1};

	struct timeval timeout;
	memset(&timeout, 0, sizeof(timeout));
	timeout.tv_sec = 60;  // 0 seconds
	timeout.tv_usec = 0; // 100 miliseconds

	setsockopt(_socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
	setsockopt(_socket, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
	setsockopt(_socket, SOL_SOCKET, SO_REUSEADDR, &_enable, sizeof(_enable));
}


PosixSMI::Socket::~Socket()
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

std::string_view PosixSMI::Socket::read_message(const char *& error)
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

const char * PosixSMI::Socket::HandleError()
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
			parent._isClosed = true;
			return "Connection reset by host";
			break;
		case EINTR:
			parent._isClosed = true;
			return "Caught signal";
			break;
		case EPIPE:
			parent._isClosed = true;
			_connectionReset = true;
			return 	"Connection reset by peer";
		case EINVAL:
			parent._isClosed = true;
			return "No out of band data available";
			break;
		case ENOTCONN:
			throw std::logic_error("socket not connected?");
		case ENOTSOCK:
			throw std::logic_error("socket not a socket?");
		case EOPNOTSUPP:
			throw std::logic_error("flags not supported.");
		case ETIMEDOUT:
			parent._timeout = true;
			break;
	}

	return nullptr;
}


#endif
