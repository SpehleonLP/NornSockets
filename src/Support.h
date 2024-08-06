#pragma once
#include <string_view>
#include <vector>

int isWhitespace(int c);
std::string_view TrimWhitespace(std::string_view const& it);
std::vector<std::string_view> ChunkMessage(std::string_view message, size_t limit);

#ifdef _WIN32
#include <string>
std::string GetLastErrorAsString();
#endif