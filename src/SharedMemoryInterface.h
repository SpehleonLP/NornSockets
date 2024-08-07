#pragma once
#include <filesystem>
#include <memory>
#include <string>


class SharedMemoryInterface
{
public:
#ifdef _WIN32
using pid_t = unsigned int;
#endif

struct Response;
	enum
	{
		Creatures1Version = 20,
	};

	static std::string cp1252FromUtf8(std::string const&);
	static std::string utf8FromCp1252(std::string const&);
	static bool isAscii(std::string const&);

// CRLF to LF
// if c1 is true:
//	- strings use [] not ""
//  - you cannot escape characters in a string (\] still ends the string). 
//  - multiple whitespace characters in a row outside a string causes a crash.
	static std::string cleanWhitespace(std::string &&, bool c1 = true);

	static std::unique_ptr<SharedMemoryInterface> Open();
	virtual ~SharedMemoryInterface() = default;

	Response send(std::string const&);

	virtual Response send1252(std::string &) = 0;
	virtual bool isClosed() = 0;


// server commands SAVE/LOAD/etc won't work if these aren't defined.
	virtual std::filesystem::path GetWorldDirectory() { return {}; }

	static std::filesystem::path GetWorkingDirectory(pid_t pid);

	std::string _name;
	std::string _engine;
	int versionMajor{};
	int versionMinor{};

	std::filesystem::path _workingDirectory{};

	bool isDDE() const { return _engine == "Vivarium"; }
	bool isCreatures1() const { return versionMajor <= Creatures1Version && isDDE(); }
	bool isCreatures2() const { return versionMajor > Creatures1Version && isDDE(); }
};

struct SharedMemoryInterface::Response
{
	std::string text{};
	bool isError{};
	bool isBinary{};
};
