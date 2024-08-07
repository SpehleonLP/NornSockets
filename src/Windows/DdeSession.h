#pragma once

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <ddeml.h>

class DdeSession
{
public:
	DdeSession();
	~DdeSession();

	HSZ  createDDEString(const char*);
	void freeDDEString(HSZ & s);

	unsigned int getPartnerPid(HSZ service, HSZ topic);
	HCONV connectConversation(HSZ service, HSZ topic);
	void disconnectConversation(HCONV & conversation);

	void handleError();
	DWORD fDDEInstance{};

private:
	void initialiseDDE();
	void uninitialiseDDE();
};

#endif

