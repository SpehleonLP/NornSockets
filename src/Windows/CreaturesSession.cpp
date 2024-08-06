#include "CreaturesSession.h"

#ifdef _WIN32

extern bool g_isDdeOpen;

std::shared_ptr<CreaturesSession> CreaturesSession::GetSession()
{
	static thread_local std::shared_ptr<CreaturesSession> session = std::make_shared<CreaturesSession>();
	return session;
}


CreaturesSession::CreaturesSession()
{
	fVivariumString = createDDEString("Vivarium");
	fMacroString = createDDEString("Macro");
	fBlankString = createDDEString(" ");
	fBrainActivityString = createDDEString("BrainActivity");
	fSessionString = createDDEString("NornSockets");
}

CreaturesSession::~CreaturesSession()
{
	freeDDEString(fSessionString);
	freeDDEString(fBrainActivityString);
	freeDDEString(fBlankString);
	freeDDEString(fMacroString);
	freeDDEString(fVivariumString);
}


std::string CreaturesSession::executeMacro(std::string& macro)
{
	HCONV fConversation = 0;
	if (fConversation == nullptr)
	{
		fConversation = connectConversation(fVivariumString, fSessionString);
		g_isDdeOpen = fConversation != nullptr;

		if (fConversation == nullptr)
			return {};
	}

	DWORD result = 0;
	HDDEDATA macro_data = DdeCreateDataHandle(fDDEInstance, (uint8_t*)macro.data(), macro.size()+1, 0, fBlankString, CF_TEXT, 0);
	if (macro_data == 0)
	{
		handleError();
	}

	HDDEDATA data = DdeClientTransaction((LPBYTE)macro_data, 0xFFFFFFFF, fConversation, fBlankString, CF_TEXT, XTYP_POKE, 5000, &result);
	if (data == 0)
	{
		handleError();
		return {};
	}

	HDDEDATA data2 = DdeClientTransaction(0, 0, fConversation, fMacroString, CF_TEXT, XTYP_REQUEST, 5000, &result);
	if (data2 == 0)
	{
		DdeFreeDataHandle(data);
		handleError();
		return {};
	}

	// Reasonable size buffer - may need to dyanmically allocate for larger queries
	unsigned int data_size = DdeGetData(data2, NULL, 0, 0);
	std::string retn(data_size + 1, '\0');

	DdeGetData(data2, (uint8_t*)retn.data(), data_size, 0);

	DdeFreeDataHandle(data);
	DdeFreeDataHandle(data2);

	disconnectConversation(fConversation);

	return retn;
}

std::string CreaturesSession::getBrainActivity()
{
	HCONV fConversation = 0;

	fConversation = connectConversation(fVivariumString, fSessionString);
	g_isDdeOpen = fConversation != nullptr;

	if (fConversation == nullptr)
		return {};

	char const* retval = 0;
	DWORD result = 0;
	HDDEDATA data = DdeClientTransaction(0, 0, fConversation, fBrainActivityString, CF_TEXT, XTYP_REQUEST, 5000, &result);
	if (data == 0)
	{
		handleError();
		return {};
	}

	DdeFreeDataHandle(data);
	unsigned int data_size = DdeGetData(data, NULL, 0, 0);
	std::string retn(data_size + 1, '\0');

	DdeGetData(data, (uint8_t*)retn.data(), data_size, 0);

	DdeFreeDataHandle(data);
	disconnectConversation(fConversation);

	return retn;
}

#endif