#include "SharedMemoryInterface.h"
#include "Support.h"


#undef interface

bool  SharedMemoryInterface::isAscii(std::string const& input)
{
	for (unsigned char c : input) {
		if(c == '\r' || c >= 0x80)
			return false;
	}

	return true;
}

std::string SharedMemoryInterface::utf8FromCp1252(std::string const& input)
{
	std::string result;
	result.reserve(input.length() * 2); // Reserve space for worst-case scenario

	for (unsigned char c : input) {
		if (c == '\r') 	continue;
		if (c < 0x80) {
			// ASCII characters (0-127) remain unchanged
			result += c;
		}
		else {
			// Convert CP1252 to Unicode code point
			uint32_t codepoint;
			switch (c) {
			case 0x80: codepoint = 0x20AC; break; // Euro sign
			case 0x82: codepoint = 0x201A; break; // Single low-9 quotation mark
			case 0x83: codepoint = 0x0192; break; // Latin small letter f with hook
			case 0x84: codepoint = 0x201E; break; // Double low-9 quotation mark
			case 0x85: codepoint = 0x2026; break; // Horizontal ellipsis
			case 0x86: codepoint = 0x2020; break; // Dagger
			case 0x87: codepoint = 0x2021; break; // Double dagger
			case 0x88: codepoint = 0x02C6; break; // Modifier letter circumflex accent
			case 0x89: codepoint = 0x2030; break; // Per mille sign
			case 0x8A: codepoint = 0x0160; break; // Latin capital letter S with caron
			case 0x8B: codepoint = 0x2039; break; // Single left-pointing angle quotation mark
			case 0x8C: codepoint = 0x0152; break; // Latin capital ligature OE
			case 0x8E: codepoint = 0x017D; break; // Latin capital letter Z with caron
			case 0x91: codepoint = 0x2018; break; // Left single quotation mark
			case 0x92: codepoint = 0x2019; break; // Right single quotation mark
			case 0x93: codepoint = 0x201C; break; // Left double quotation mark
			case 0x94: codepoint = 0x201D; break; // Right double quotation mark
			case 0x95: codepoint = 0x2022; break; // Bullet
			case 0x96: codepoint = 0x2013; break; // En dash
			case 0x97: codepoint = 0x2014; break; // Em dash
			case 0x98: codepoint = 0x02DC; break; // Small tilde
			case 0x99: codepoint = 0x2122; break; // Trade mark sign
			case 0x9A: codepoint = 0x0161; break; // Latin small letter s with caron
			case 0x9B: codepoint = 0x203A; break; // Single right-pointing angle quotation mark
			case 0x9C: codepoint = 0x0153; break; // Latin small ligature oe
			case 0x9E: codepoint = 0x017E; break; // Latin small letter z with caron
			case 0x9F: codepoint = 0x0178; break; // Latin capital letter Y with diaeresis
			default: 
				result += '?';
				continue;
				break; // Other characters map directly
			}

			// Convert Unicode code point to UTF-8
			if (codepoint <= 0x7F) {
				result += static_cast<char>(codepoint);
			}
			else if (codepoint <= 0x7FF) {
				result += static_cast<char>(0xC0 | ((codepoint >> 6) & 0x1F));
				result += static_cast<char>(0x80 | (codepoint & 0x3F));
			}
			else if (codepoint <= 0xFFFF) {
				result += static_cast<char>(0xE0 | ((codepoint >> 12) & 0x0F));
				result += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
				result += static_cast<char>(0x80 | (codepoint & 0x3F));
			}
			else {
				// This case should not occur for CP1252, but included for completeness
				result += static_cast<char>(0xF0 | ((codepoint >> 18) & 0x07));
				result += static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F));
				result += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
				result += static_cast<char>(0x80 | (codepoint & 0x3F));
			}
		}
	}

	return result;
}


std::string SharedMemoryInterface::cp1252FromUtf8(std::string const& input)
{
	std::string result;
	result.reserve(input.length()+8); // Reserve space for the worst-case scenario

	for (size_t i = 0; i < input.length(); ++i) {
		unsigned char c = input[i];
		if (c == '\r') 	continue;
		if (c <= 0x7F) {
			// ASCII characters (0-127) remain unchanged
			result += c;
		}
		else
		{
			uint32_t codepoint = 0;

			if (c >= 0xC2 && c <= 0xDF && i + 1 < input.length()) 
			{
				// 2-byte UTF-8 sequence
				codepoint = ((c & 0x1F) << 6) | (input[i + 1] & 0x3F);
				i++; // Skip the next byte as we've already processed it
			}
			else if (c >= 0xE0 && c <= 0xEF && i + 2 < input.length()) {
				// 3-byte UTF-8 sequence
				codepoint = ((c & 0x0F) << 12) | ((input[i + 1] & 0x3F) << 6) | (input[i + 2] & 0x3F);
				i += 2; // Skip
			}
			else if (c >= 0xF0 && c <= 0xF7 && i + 3 < input.length()) {
				// 3-byte UTF-8 sequence
				codepoint = ((((c & 0x0F) << 12) | ((input[i + 1] & 0x3F) << 6) | (input[i + 2] & 0x3F)) << 6) | (input[i + 3] & 0x3F);
				i += 3; // Skip
			}

			switch (codepoint) {
			case 0x20AC: result += static_cast<char>(0x80); break; // Euro sign
			case 0x201A: result += static_cast<char>(0x82); break; // Single low-9 quotation mark
			case 0x0192: result += static_cast<char>(0x83); break; // Latin small letter f with hook
			case 0x201E: result += static_cast<char>(0x84); break; // Double low-9 quotation mark
			case 0x2026: result += static_cast<char>(0x85); break; // Horizontal ellipsis
			case 0x2020: result += static_cast<char>(0x86); break; // Dagger
			case 0x2021: result += static_cast<char>(0x87); break; // Double dagger
			case 0x02C6: result += static_cast<char>(0x88); break; // Modifier letter circumflex accent
			case 0x2030: result += static_cast<char>(0x89); break; // Per mille sign
			case 0x0160: result += static_cast<char>(0x8A); break; // Latin capital letter S with caron
			case 0x2039: result += static_cast<char>(0x8B); break; // Single left-pointing angle quotation mark
			case 0x0152: result += static_cast<char>(0x8C); break; // Latin capital ligature OE
			case 0x017D: result += static_cast<char>(0x8E); break; // Latin capital letter Z with caron
			case 0x2018: result += static_cast<char>(0x91); break; // Left single quotation mark
			case 0x2019: result += static_cast<char>(0x92); break; // Right single quotation mark
			case 0x201C: result += static_cast<char>(0x93); break; // Left double quotation mark
			case 0x201D: result += static_cast<char>(0x94); break; // Right double quotation mark
			case 0x2022: result += static_cast<char>(0x95); break; // Bullet
			case 0x2013: result += static_cast<char>(0x96); break; // En dash
			case 0x2014: result += static_cast<char>(0x97); break; // Em dash
			case 0x02DC: result += static_cast<char>(0x98); break; // Small tilde
			case 0x2122: result += static_cast<char>(0x99); break; // Trade mark sign
			case 0x0161: result += static_cast<char>(0x9A); break; // Latin small letter s with caron
			case 0x203A: result += static_cast<char>(0x9B); break; // Single right-pointing angle quotation mark
			case 0x0153: result += static_cast<char>(0x9C); break; // Latin small ligature oe
			case 0x017E: result += static_cast<char>(0x9E); break; // Latin small letter z with caron
			case 0x0178: result += static_cast<char>(0x9F); break; // Latin capital letter Y with diaeresis
			default:	 result += '?'; break;
			}
		}
	}

	return result;
}

// CRLF to LF
// if c1 is true:
//	- strings use [] not ""
//  - you cannot escape characters in a string (\] still ends the string). 
//  - multiple whitespace characters in a row outside a string causes a crash.
std::string SharedMemoryInterface::cleanWhitespace(std::string&& input, bool c1)
{
	std::string result;
	result.reserve(input.length()); // Reserve space for efficiency

	bool inString = false;
	char stringDelimiter = c1 ? '[' : '"';
	char stringEndDelimiter = c1 ? ']' : '"';
	bool lastWasWhitespace = false;

	for (size_t i = 0; i < input.length(); ++i) {
		char c = input[i];

		if (inString) {
			result += c;
			if (c == stringEndDelimiter && (!c1 || (c1 && i > 0 && input[i - 1] != '\\'))) {
				inString = false;
			}
		}
		else {
			if (c == stringDelimiter) {
				inString = true;
				result += c;
				lastWasWhitespace = false;
			}
			else if (c == '\r' && i + 1 < input.length() && input[i + 1] == '\n') {
				// CRLF to LF conversion
				result += '\n';
				++i; // Skip the next character ('\n')
				lastWasWhitespace = c1; // In c1 mode, consider this as whitespace
			}
			else if (isWhitespace(c)) {
				if (c != '\r') { // Keep other whitespace in non-c1 mode, except lone '\r'
					if (lastWasWhitespace && c1) {
						continue;
					}

					result += c;
					lastWasWhitespace = true;
				}
			}
			else {
				result += c;
				lastWasWhitespace = false;
			}
		}
	}

	if (inString)
	{
		result += stringEndDelimiter;
	}

	return result;
}


SharedMemoryInterface::Response SharedMemoryInterface::send(std::string const& text)
{
	auto cp1252 = cp1252FromUtf8(text);

#ifdef _WIN32
	if(isDDE())
	{
		cp1252 = cleanWhitespace(std::move(cp1252));
	}
#endif

	auto r = send1252(cp1252);

	if(r.isBinary == false && isAscii(r.text) == false)
	{
		r.text = utf8FromCp1252(r.text);
	}

	return r;
}

#ifdef _WIN32
#include "Windows/WindowsSMI.h"
#include "Windows/VivariumInterface.h"

std::unique_ptr<SharedMemoryInterface> SharedMemoryInterface::Open()
{

	std::unique_ptr<SharedMemoryInterface> interface;

#ifdef _WIN32
	interface = VivariumInterface::OpenVivarium();

	if (interface)
	{
		return interface;
	}

#endif

	if (interface == nullptr)
	{
		const char* current = "";
		const char* engine_names[] = {
			"Creatures Village",
			"Creatures Playground",
			"Creatures 3",
			"Docking Station",
			"Edynn",
			"Sea Monkeys",
			"Creatures Evolution Engine",
			nullptr
		};

		for (auto ptr = engine_names; interface == nullptr && *ptr; ++ptr)
		{
			interface = WindowsSMI::Open(current = *ptr);
		}

		if (interface && interface->_name.size())
			return interface;

		return nullptr;
	}


	return interface;
}

#else
#include "Posix/PosixSMI.h"


std::unique_ptr<SharedMemoryInterface> SharedMemoryInterface::Open()
{
	auto p = PosixSMI::Create();

	if(p && p->_name.size())
		return p;

	return nullptr;
}

#endif
#include <psapi.h>

std::filesystem::path SharedMemoryInterface::GetWorkingDirectory(pid_t pid)
{
#ifdef _WIN32
	HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
	if (hProcess == NULL) {
		return "";
	}

	wchar_t path[MAX_PATH];
	if (GetModuleFileNameEx(hProcess, NULL, path, MAX_PATH) == 0) {
		CloseHandle(hProcess);
		return "";
	}

	CloseHandle(hProcess);
	std::filesystem::path p(path);
	return p.parent_path();
#else
	char path[PATH_MAX];
	snprintf(path, sizeof(path), "/proc/%d/cwd", pid);
	char resolved_path[PATH_MAX];
	if (realpath(path, resolved_path) == NULL) {
		return "";
	}

	return std::filesystem::path(resolved_path);
#endif
}

