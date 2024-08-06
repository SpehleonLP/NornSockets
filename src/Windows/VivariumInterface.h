#pragma once
#include "../SharedMemoryInterface.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <ddeml.h>

class VivariumInterface : public SharedMemoryInterface
{
struct DDE_Strings;
public:
	static std::unique_ptr<SharedMemoryInterface> OpenVivarium();

	VivariumInterface();
	~VivariumInterface();

	Response send1252(std::string &) override;
	bool isClosed() override;

private:
	HSZ fVivariumString{};
	HSZ fMacroString{};
	HSZ fBlankString{};
	HSZ fBrainActivityString{};
	HSZ fSessionString{};
	HCONV fConversation{};
	DWORD fDDEInstance{};

	std::string executeMacro(std::string const& macro);
	std::string getBrainActivity();

	HSZ createDDEString(char const* s);
	void freeDDEString(HSZ s);

	void initialiseDDE();
	void uninitialiseDDE();

	HCONV connectConversation(HSZ service, HSZ topic);
	void disconnectConversation(HCONV conversation);

	void handleError();
};

#endif
