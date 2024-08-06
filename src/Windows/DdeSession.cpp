#include "DdeSession.h"
#include <stdexcept>
#include <string>

#ifdef _WIN32

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

DdeSession::DdeSession()
{
	initialiseDDE();
}

DdeSession::~DdeSession()
{
	uninitialiseDDE();
}


void DdeSession::initialiseDDE()
{
	if (DdeInitialize(&fDDEInstance, &VivariumInterfaceCallback, APPCLASS_STANDARD, 0) != DMLERR_NO_ERROR)
	{
		handleError();
	}
}

void DdeSession::uninitialiseDDE()
{
	if (fDDEInstance && !DdeUninitialize(fDDEInstance))
	{
		handleError();
		fDDEInstance = 0;
	}
}

HSZ DdeSession::createDDEString(char const* s)
{
	HSZ retval = DdeCreateStringHandleA(fDDEInstance, s, CP_WINANSI);
	if (retval == 0)
	{
		handleError();
	}
	return retval;
}

void DdeSession::freeDDEString(HSZ & s)
{
	if (s && !DdeFreeStringHandle(fDDEInstance, s))
	{
		handleError();
		s = 0;
	}
}

HCONV DdeSession::connectConversation(HSZ service, HSZ topic)
{
	HCONV retval = DdeConnect(fDDEInstance, service, topic, 0);
	if (retval == 0)
	{
		handleError();
	}

	return retval;
}

void DdeSession::disconnectConversation(HCONV & conversation)
{
	if (conversation && !DdeDisconnect(conversation))
	{
		handleError();
		conversation = 0;
	}
}

void DdeSession::handleError()
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