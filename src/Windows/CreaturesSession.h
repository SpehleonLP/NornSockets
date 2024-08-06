#pragma once
#include <memory>

#ifdef _WIN32
#include "DdeSession.h"
#include <string>


class CreaturesSession :  public DdeSession
{
public:
	static std::shared_ptr<CreaturesSession> GetSession();

	CreaturesSession();
	~CreaturesSession();

	std::string executeMacro(std::string& macro);
	std::string getBrainActivity();

	HSZ fVivariumString{};
	HSZ fMacroString{};
	HSZ fBlankString{};
	HSZ fBrainActivityString{};
	HSZ fSessionString{};
};

#endif