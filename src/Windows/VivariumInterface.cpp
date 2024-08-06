#include "VivariumInterface.h"
#include <stdexcept>

#ifdef _WIN32

static bool g_isDdeOpen = false;

std::unique_ptr<SharedMemoryInterface> VivariumInterface::OpenVivarium()
{
	// add some kind of check so that we have a better way of checking if the program is even open and able to connect than throwing an exception
	static std::unique_ptr<VivariumInterface> vivarium;

	try
	{
		if (vivarium == nullptr)
			vivarium = std::make_unique<VivariumInterface>();

		vivarium->fConversation = vivarium->connectConversation(vivarium->fVivariumString, vivarium->fSessionString);
		g_isDdeOpen = vivarium->fConversation != nullptr;

		if (vivarium->fConversation)
		{
			fprintf(stderr, "connected to vivarium!\n");
			return std::unique_ptr<SharedMemoryInterface>(vivarium.release());
		}

		return nullptr;
	}
	catch (std::exception & e)
	{
		return nullptr;
	}

}


VivariumInterface::Response VivariumInterface::send1252(std::string & message)
{
	try
	{
		if (message == "BRAIN DUMP")
		{
			return Response{
				.text = getBrainActivity(),
				.isError = false,
				.isBinary=true,
			};
		}
		else
		{
			return Response{
				.text = executeMacro(message),
				.isError = false,
				.isBinary = false,
			};
		}
	}
	catch (std::exception& e)
	{
		return Response{
			.text=e.what(),
			.isError = true,
			.isBinary = false,
		};
	}
}

bool VivariumInterface::isClosed()
{
	return !g_isDdeOpen;
}


HDDEDATA CALLBACK VivariumInterfaceCallback(
	UINT uType,	// transaction type
	UINT uFmt,	// clipboard data format
	HCONV hconv,	// handle to the conversation
	HSZ hsz1,	// handle to a string
	HSZ hsz2,	// handle to a string
	HDDEDATA hdata,	// handle to a global memory object
	DWORD dwData1,	// transaction-specific data
	DWORD dwData2 	// transaction-specific data
);

std::string VivariumInterface::executeMacro(std::string const& macro)
{
	DWORD result = 0;
	HDDEDATA macro_data = DdeCreateDataHandle(fDDEInstance, (uint8_t*)macro.data(), macro.size(), 0, fBlankString, CF_TEXT, 0);
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
	return retn;
}

std::string VivariumInterface::getBrainActivity()
{
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

	return retn;
}

VivariumInterface::VivariumInterface()
{
	initialiseDDE();
	fVivariumString = createDDEString("Vivarium");
	fMacroString = createDDEString("Macro");
	fBlankString = createDDEString(" ");
	fBrainActivityString = createDDEString("BrainActivity");
	fSessionString = createDDEString("NornSockets.EXE");
}


VivariumInterface::~VivariumInterface()
{
	if (fConversation != 0)
	{
		disconnectConversation(fConversation);
		fConversation = 0;
	}
	if (fSessionString != 0)
	{
		freeDDEString(fSessionString);
		fSessionString = 0;
	}
	if (fBrainActivityString != 0)
	{
		freeDDEString(fBrainActivityString);
		fBrainActivityString = 0;
	}
	if (fBlankString != 0)
	{
		freeDDEString(fBlankString);
		fBlankString = 0;
	}
	if (fMacroString != 0)
	{
		freeDDEString(fMacroString);
		fMacroString = 0;
	}
	if (fVivariumString != 0)
	{
		freeDDEString(fVivariumString);
		fVivariumString = 0;
	}
	if (fDDEInstance != 0)
	{
		uninitialiseDDE();
		fDDEInstance = 0;
	}
}


void VivariumInterface::initialiseDDE()
{
	if (DdeInitialize(&fDDEInstance, &VivariumInterfaceCallback, APPCLASS_STANDARD, 0) != DMLERR_NO_ERROR)
	{
		handleError();
	}
}

void VivariumInterface::uninitialiseDDE()
{
	if (!DdeUninitialize(fDDEInstance))
	{
		handleError();
	}
}

HSZ VivariumInterface::createDDEString(char const* s)
{
	enum
	{
		BUFFER_SIZE = 64,
	};

	size_t name_len = strlen(s);
	size_t converted_len = 0;
	wchar_t wname[BUFFER_SIZE];

	// Convert char* to wchar_t*
	mbstowcs_s(&converted_len, wname, name_len + 1, s, _TRUNCATE);

	HSZ retval = DdeCreateStringHandle(fDDEInstance, wname, CP_WINANSI);
	if (retval == 0)
	{
		handleError();
	}
	return retval;
}

void VivariumInterface::freeDDEString(HSZ s)
{
	if (!DdeFreeStringHandle(fDDEInstance, s))
	{
		handleError();
	}
}

HCONV VivariumInterface::connectConversation(HSZ service, HSZ topic)
{
	HCONV retval = DdeConnect(fDDEInstance, service, topic, 0);
	if (retval == 0)
	{
		handleError();
	}

	return retval;
}

void VivariumInterface::disconnectConversation(HCONV conversation)
{
	if (!DdeDisconnect(conversation))
	{
		handleError();
	}
}

void VivariumInterface::handleError()
{
	auto errorCode = DdeGetLastError(fDDEInstance);
	switch (errorCode) {
	case DMLERR_NO_ERROR: break;
	case DMLERR_ADVACKTIMEOUT:
		throw std::runtime_error("Advise transaction timed out.");
	case DMLERR_BUSY:
		throw std::runtime_error("DDEML is busy.");
		break;
	case DMLERR_DATAACKTIMEOUT:
		throw std::runtime_error("Data acknowledgment transaction timed out.");
		break;
	case DMLERR_DLL_NOT_INITIALIZED:
		throw std::runtime_error("DDEML not initialized.");
		break;
	case DMLERR_DLL_USAGE:
		throw std::runtime_error("DDEML usage error.");
		break;
	case DMLERR_EXECACKTIMEOUT:
		throw std::runtime_error("Execute acknowledgment transaction timed out.");
		break;
	case DMLERR_INVALIDPARAMETER:
		throw std::runtime_error("Invalid parameter.");
		break;
	case DMLERR_LOW_MEMORY:
		throw std::runtime_error("Low memory.");
		break;
	case DMLERR_MEMORY_ERROR:
		throw std::runtime_error("Memory error.");
		break;
	case DMLERR_NOTPROCESSED:
		throw std::runtime_error("Transaction not processed.");
		break;
// fail silently
	case DMLERR_NO_CONV_ESTABLISHED:
		break;
	case DMLERR_POKEACKTIMEOUT:
		throw std::runtime_error("Poke acknowledgment transaction timed out.");
		break;
	case DMLERR_POSTMSG_FAILED:
		throw std::runtime_error("Post message failed.");
		break;
	case DMLERR_REENTRANCY:
		throw std::runtime_error("Reentrancy error.");
		break;
	case DMLERR_SERVER_DIED:
		throw std::runtime_error("Server died.");
		break;
	case DMLERR_SYS_ERROR:
		throw std::runtime_error("System error.");
		break;
	case DMLERR_UNADVACKTIMEOUT:
		throw std::runtime_error("Unadvise acknowledgment transaction timed out.");
		break;
	case DMLERR_UNFOUND_QUEUE_ID:
		throw std::runtime_error("Queue ID not found.");
		break;
	default:
		throw std::runtime_error("Unknown error: " + std::to_string(errorCode));
		break;
	}
}

HDDEDATA CALLBACK VivariumInterfaceCallback(
	UINT uType,	// transaction type
	UINT uFmt,	// clipboard data format
	HCONV hconv,	// handle to the conversation
	HSZ hsz1,	// handle to a string
	HSZ hsz2,	// handle to a string
	HDDEDATA hdata,	// handle to a global memory object
	DWORD dwData1,	// transaction-specific data
	DWORD dwData2 	// transaction-specific data
)
{
	HDDEDATA retval = 0;
	switch (uType)
	{
	case XTYP_ADVSTART:
	case XTYP_CONNECT:
	{
		g_isDdeOpen = true;
		retval = 0;
	}
	break;

	case XTYP_ADVREQ:
	case XTYP_REQUEST:
	case XTYP_WILDCONNECT:
	{
		retval = 0;
	}
	break;

	case XTYP_ADVDATA:
	case XTYP_EXECUTE:
	case XTYP_POKE:
	{
		retval = (HDDEDATA)DDE_FACK;
	}
	break;

	case XTYP_DISCONNECT:
	case XTYP_ERROR:
	{
		g_isDdeOpen = false;
		retval = 0;
		break;
	}


	case XTYP_ADVSTOP:
	case XTYP_CONNECT_CONFIRM:
	case XTYP_MONITOR:
	case XTYP_REGISTER:
	case XTYP_XACT_COMPLETE:
	case XTYP_UNREGISTER:
	{
		retval = 0;
	}
	break;

	default:
	{
		retval = 0;
	}
	break;
	}

	return retval;
}

#endif
