#ifndef LOCALSERVER_H
#define LOCALSERVER_H
#include "SharedMemoryInterface.h"
#include <filesystem>
#include <map>
#include <string_view>
#include <vector>

class SharedMemoryInterface;

#ifndef MAKEFOURCC
#define MAKEFOURCC(ch0, ch1, ch2, ch3) \
				(static_cast<uint32_t>(static_cast<uint8_t>(ch0)) \
				| (static_cast<uint32_t>(static_cast<uint8_t>(ch1)) << 8) \
				| (static_cast<uint32_t>(static_cast<uint8_t>(ch2)) << 16) \
				| (static_cast<uint32_t>(static_cast<uint8_t>(ch3)) << 24))
#endif /* defined(MAKEFOURCC) */

class LocalServer
{
public:
using Response = SharedMemoryInterface::Response;
	enum Commands
	{
		SAVE = MAKEFOURCC('S', 'A', 'V', 'E'),
		LOAD = MAKEFOURCC('L', 'O', 'A', 'D'),
		DLTE = MAKEFOURCC('D', 'L', 'T', 'E'),
		MOVE = MAKEFOURCC('M', 'O', 'V', 'E'),
		QRCD = MAKEFOURCC('Q', 'R', 'C', 'D'),
		LOG  = MAKEFOURCC('L', 'O', 'G', '\0'),
		DBG  = MAKEFOURCC('D', 'B', 'G', '\0'),
		OOPE = MAKEFOURCC('O', 'O', 'P', 'E'),
		PATH = MAKEFOURCC('P', 'A', 'T', 'H'),
	};

// split into args.
	static std::vector<std::string_view> Parse(std::string_view);

	LocalServer();
	~LocalServer();

	void OnGameOpened(SharedMemoryInterface*);
	void OnGameClosed(SharedMemoryInterface*);


	Response ProcessMessage(uint32_t code, std::string_view c_str, std::string_view binary_buffer);

private:
	void Load();
	void Save();

	std::filesystem::path GetPath(std::string_view file);
	bool CanModify(std::filesystem::path const& path);
	void OnModifiedFile(std::filesystem::path const& path, bool exists);

	std::filesystem::path _logFile;
	std::map<std::filesystem::path, uint64_t> _files;
	std::mutex _mutex;
	SharedMemoryInterface * _interface{};
};

#endif // LOCALSERVER_H
