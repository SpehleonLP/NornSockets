#include "Support.h"
#include <vector>


int isWhitespace(int c)
{
	switch (c)
	{
		// maybe whitespace?
	case 0x180E: case 0x200B: case 0x200C:
	case 0x200D: case 0x2060: case 0xFEFF:
		return 4;
		// new line
	case 0x000A: case 0x000B: case 0x000C:
	case 0x000D: case 0x0085:
	case 0x2028: case 0x2029:
		return 3;
		// tab
	case 0x0009:
		return 2;
		//space
	case 0:
	case 0x0020:
	case 0x00A0:
	case 0x1680: case 0x2000: case 0x2001:
	case 0x2002: case 0x2003: case 0x2004:
	case 0x2005: case 0x2006: case 0x2007:
	case 0x2008: case 0x2009: case 0x200A:
	case 0x202F: case 0x205F: case 0x3000:
		return 1;
	default:
		return 0;
	}
}

std::string_view TrimWhitespace(std::string_view const& it)
{
	if (it.empty())
		return it;

	size_t p = 0, len = it.size() - 1;

	for (; p < it.size() && isWhitespace(it[p]); ++p);
	for (; len > p && isWhitespace(it[len]); --len);

	return it.substr(p, len + 1 - p);
}


std::vector<std::string_view> ChunkMessage(std::string_view message, size_t limit)
{
	std::vector<std::string_view> chunks;

	size_t start = 0;
	size_t curr = 0;

	while(true)
	{
		curr = message.find("endm", start);

		if(curr == std::string_view::npos)
		{
			if(start <= message.size()-5)
			{
				chunks.push_back({ message.data() + start, (message.data() + message.size())});
			}

			break;
		}

		chunks.push_back({ message.data() + start, (message.data() + curr+4)});
		start = curr+4;
	}

	for (auto i = 0u; i < chunks.size(); ++i)
	{
		while (i + 1 < chunks.size())
		{
			size_t combinedSize = chunks[i+1].end() - chunks[i].begin();

			if (combinedSize < limit)
			{
				chunks[i] = std::string_view(chunks[i].data(), chunks[i+1].data() + chunks[i+1].size());
				chunks.erase(chunks.begin()+ (i+1));
			}
		}
	}

	return chunks;
}

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

//Returns the last Win32 error, in string format. Returns an empty string if there is no error.
std::string GetLastErrorAsString()
{
	//Get the error message ID, if any.
	DWORD errorMessageID = ::GetLastError();
	if (errorMessageID == 0) {
		return std::string(); //No error message has been recorded
	}

	LPSTR messageBuffer = nullptr;

	//Ask Win32 to give us the string version of that message ID.
	//The parameters we pass in, tell Win32 to create the buffer that holds the message for us (because we don't yet know how long the message string will be).
	size_t size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL, errorMessageID, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);

	//Copy the error message into a std::string.
	std::string message(messageBuffer, size);

	//Free the Win32's string's buffer.
	LocalFree(messageBuffer);

	return message;
}

#endif