#include "localserver.h"
#include "DebugLog.h"
#include <vector>
#include <fstream>
#include <cstring>
#include <system_error>

#include "qrcodegen.h"
#include "stb_bmp_write.h"


LocalServer::LocalServer()
{
	_logFile = "nornsockets.log";
	Load();
}


LocalServer::~LocalServer()
{
	Save();
}

void  LocalServer::OnGameOpened(SharedMemoryInterface* i)
{
	std::lock_guard lock(_mutex);
	_interface = i;
}

void  LocalServer::OnGameClosed(SharedMemoryInterface* i)
{
	(void)i;
	std::lock_guard lock(_mutex);
	_interface = nullptr;
}

void LocalServer::Load()
{
	std::lock_guard lock(_mutex);

	if(std::filesystem::exists(_logFile) == false)
		return;

	try
	{
		std::ifstream file(_logFile.string());

		if (!file.is_open())
		{
			throw std::system_error(errno, std::system_category(), _logFile.string());
		}

		file.exceptions(std::ifstream::badbit);

		_files.clear();
		uint64_t value;
		std::filesystem::path path;
		std::string line;

		while (std::getline(file, line))
		{
			if (line.empty())
				break;

			std::istringstream iss(line);
			if (!(iss >> value >> path))
			{
				throw std::runtime_error("Invalid format in log file");
			}

			_files[path] = value;
		}
	}
	catch(std::exception & e)
	{
		fprintf(stderr, "Problem loading log file: %s", e.what());
	}
}


void LocalServer::Save()
{
	std::lock_guard lock(_mutex);

	try
	{
		std::ofstream file(_logFile.string());

		if (!file.is_open())
		{
			throw std::system_error(errno, std::system_category(), _logFile.string());
		}

		file.exceptions(std::ifstream::badbit | std::ifstream::failbit);

		for(auto & item : _files)
		{
			file << item.second << "\t" << item.first.string() << "\n";
		}

		file << std::endl;
		file.close();
	}
	catch(std::exception & e)
	{
		fprintf(stderr, "Problem saving log file: %s", e.what());
	}
}

std::filesystem::path LocalServer::GetPath(std::string_view file)
{
	std::lock_guard lock(_mutex);

	if(_interface == nullptr)
		return {};
	std::filesystem::path file_name(file);
	auto extension = file_name.extension().string();

	std::string directory;
	bool world{};

	for(auto & c : extension)
		c = tolower(c);

	if(extension == ".cob")
		directory = "Objects", world = false;
	else if(extension == ".agents")
		directory = "My Agents", world = false;
	else if(extension == ".blk")
		directory = "Backgrounds", world = false;
	else if(extension == ".catalogue")
		directory = "Catalogue", world = false;
	else if(extension == ".creature")
		directory = "My Creatures", world = false;
	else if(extension == ".wav")
		directory = "Sounds", world = false;
	else if(extension == ".spr" || extension == ".s16" || extension == ".c16")
		directory = "Images", world = false;
	else if(extension == ".att")
		directory = "Body Data", world = false;
	else if(extension == ".gen" || extension == ".gno")
		directory = "Genetics", world = false;
	else if(extension == ".bmp")
		directory = "SNAP", world = true;
	else
		directory = "Journal", world = true;

	if(world == false)
	{
		std::filesystem::path path = _interface->GetGameDirectory();

		if(path.empty() == false)
			return (path /= directory) /= file;
	}
	else if(world == false)
	{
		std::filesystem::path path = _interface->GetWorldDirectory();

		if(path.empty() == false)
			return (path /= directory) /= file;
	}

	return {};
}

void LocalServer::OnModifiedFile(std::filesystem::path const& path, bool exists)
{
	std::lock_guard lock(_mutex);

	if(exists == false)
	{
		auto itr = _files.find(path);

		if(itr != _files.end())
			_files.erase(itr);
	}
	else
	{
		auto ftime = std::filesystem::last_write_time(path);
		auto sctp = std::chrono::time_point_cast<std::chrono::seconds>(ftime);
		uint64_t time = sctp.time_since_epoch().count();

		_files[path] = time;
	}
}

bool LocalServer::CanModify(std::filesystem::path const& path)
{
	if(std::filesystem::exists(path) == false)
		return true;

	std::lock_guard lock(_mutex);
	auto itr = _files.find(path);

	if(itr == _files.end())
		return false;

	auto ftime = std::filesystem::last_write_time(path);
	auto sctp = std::chrono::time_point_cast<std::chrono::seconds>(ftime);
	uint64_t time = sctp.time_since_epoch().count();

	if(time > itr->second)
	{
		_files.erase(itr);
		return false;
	}

	return true;
}

// split string using same rules as a terminal emulator (whitespace deliminated unless quoted, quotes removed from strings, etc)
std::vector<std::string_view> LocalServer::Parse(std::string_view str)
{
	std::vector<std::string_view> tokens;
	size_t pos = 0;
	bool in_quotes = false;
	char current_quote = 0;

	for (size_t i = 0; i < str.length(); ++i) {
		if (str[i] == '"' || str[i] == '\'') {
			if (in_quotes && str[i] == current_quote && (i + 1 == str.length() || std::isspace(str[i + 1]))) {
				tokens.emplace_back(str.substr(pos, i - pos));
				in_quotes = false;
				++i; // skip space after closing quote if exists
				pos = i + 1; // set start of next token
			} else if (!in_quotes) {
				in_quotes = true;
				current_quote = str[i];
				pos = i + 1; // start after the quote
			}
		} else if (std::isspace(str[i]) && !in_quotes) {
			if (i > pos) {
				tokens.emplace_back(str.substr(pos, i - pos));
			}
			pos = i + 1; // set start of next token
		}
	}

	if (pos < str.length()) { // add last token
		tokens.emplace_back(str.substr(pos));
	}

	return tokens;
}


LocalServer::Response LocalServer::ProcessMessage(uint32_t code, std::string_view c_str, std::string_view binary_buffer)
{
	auto args =  LocalServer::Parse(c_str);
	std::filesystem::path src;
	std::filesystem::path dst;

	switch(LocalServer::Commands(code))
	{
	case LocalServer::SAVE:
		if(args.size() < 1)
		{
			return Response{
				.text="too few args to save command.",
				.isError=true,
				.isBinary=false,
			};
		}

		dst = GetPath(args[0]);

		if(dst.empty())
		{
			return Response{
				.text=(std::string("unable to find path for: ") + std::string(args[0])),
				.isError=true,
				.isBinary=false,
			};
		}

		if(!CanModify(dst))
		{
			return Response{
				.text="lack permission to modify given file.",
				.isError=true,
				.isBinary=false,
			};
		}

		try {
			std::ofstream file(src.string(), std::ios::binary);

			if (!file.is_open())
			{
				throw std::system_error(errno, std::system_category(), src.string());
			}

			file.exceptions(std::ifstream::badbit | std::ifstream::failbit);

			file.write(binary_buffer.data(), binary_buffer.size());
			OnModifiedFile(dst, true);
		}
		catch(std::exception & e)
		{
			return Response{
				.text=e.what(),
				.isError=true,
				.isBinary=false,
			};
		}
		break;
	case LocalServer::LOAD:
		if(args.size() < 1)
		{
			return Response{
				.text="too few args to load command.",
				.isError=true,
				.isBinary=false,
			};
		}


		src = GetPath(c_str);

		if(src.empty())
		{
			return Response{
				.text=(std::string("unable to find path for: ") + std::string(args[0])),
				.isError=true,
				.isBinary=false,
			};
		}

		try {
			std::ifstream file(src.string(), std::ios::binary);

			if (!file.is_open())
			{
				throw std::system_error(errno, std::system_category(), src.string());
			}

			file.exceptions(std::ifstream::badbit | std::ifstream::failbit);

			std::string text;

			file.seekg(0, std::ios::end);
			size_t file_size = size_t(file.tellg());
			text.resize(size_t(file.tellg())+sizeof(8));
			file.seekg(0, std::ios::beg);

			memcpy(text.data(), &file_size, sizeof(file_size));
			file.read(text.data()+sizeof(file_size), text.size() - sizeof(file_size));
			file.close();

			return Response{
				.text=std::move(text),
				.isError=false,
				.isBinary=true,
			};
		}
		catch(std::exception & e)
		{
			return Response{
				.text=e.what(),
				.isError=true,
				.isBinary=false,
			};
		}
		break;
	case LocalServer::DLTE:
		if(args.size() < 1)
		{
			return Response{
				.text="too few args to delete command.",
				.isError=true,
				.isBinary=false,
			};
		}

		src = GetPath(c_str);
		if(CanModify(src) && std::filesystem::exists(src))
		{
			std::remove(src.string().c_str());
			OnModifiedFile(src, false);
		}
		else if(std::filesystem::exists(src))
		{
			return Response{
				.text="lack permission to modify given file.",
				.isError=true,
				.isBinary=false,
			};
		}
		break;
	case LocalServer::MOVE:
	{
		if(args.size() < 2)
		{
			return Response{
				.text="too few args to move command.",
				.isError=true,
				.isBinary=false,
			};
		}

		src = GetPath(args[0]);
		dst = GetPath(args[1]);

		if(src.empty())
		{
			return Response{
				.text=(std::string("unable to find path for: ") + std::string(args[0])),
				.isError=true,
				.isBinary=false,
			};
		}

		if(!std::filesystem::exists(src))
		{
			return Response{
				.text="cannot move source file because it does not exist.",
				.isError=true,
				.isBinary=false,
			};
		}

		if(CanModify(dst))
		{
			if(CanModify(src))
				std::rename(src.string().c_str(), dst.string().c_str());
			else
				std::filesystem::copy(src.string().c_str(), dst.string().c_str());

			OnModifiedFile(dst, true);
		}
		else if(std::filesystem::exists(dst))
		{
			return Response{
				.text="lack permission to modifiy given file.",
				.isError=true,
				.isBinary=false,
			};
		}
		break;
	case LocalServer::QRCD:
		if(args.size() < 2)
		{
			return Response{
				.text="too few args to move command.",
				.isError=true,
				.isBinary=false,
			};
		}

		dst = GetPath(args[1]);

		if(dst.empty())
		{
			return Response{
				.text=(std::string("unable to find path for: ") + std::string(args[0])),
				.isError=true,
				.isBinary=false,
			};
		}
		if(!CanModify(dst))
		{
			return Response{
				.text="lack permission to modifiy given file.",
				.isError=true,
				.isBinary=false,
			};
		}

		std::string url_cstr = std::string(args[0]);
		std::vector<uint8_t> buffer(qrcodegen_BUFFER_LEN_FOR_VERSION(args[0].size()));
		uint8_t tempBuffer[qrcodegen_BUFFER_LEN_MAX];


		if(!qrcodegen_encodeText(url_cstr.c_str(), tempBuffer, buffer.data(),
			qrcodegen_Ecc_MEDIUM, qrcodegen_VERSION_MIN, qrcodegen_VERSION_MAX, qrcodegen_Mask_AUTO, true))
		{
			return Response{
				.text="unable to create QR code.",
				.isError=true,
				.isBinary=false,
			};
		}

		int size = qrcodegen_getSize(buffer.data());
		std::vector<uint8_t> pixels(size*size);

		for (int y = 0; y < size; y++) {
			for (int x = 0; x < size; x++) {
				pixels[y*size+x] = qrcodegen_getModule(buffer.data(), x, y)? 0 : 255;
			}
		}

		stbi_write_bmp(dst.string().c_str(), size, size, 1, pixels.data());
	}
	case LocalServer::LOG:
		fprintf(stderr, "%s", c_str.data());
		break;
	case LocalServer::DBG:
		DebugLog::WriteDebugMessage(c_str);
		break;
	case LocalServer::OOPE:
		DebugLog::WriteDebugMessage(c_str);
		break;
	}

	return {};
}
