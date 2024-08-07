#include "VivariumInterface.h"
#include "CreaturesSession.h"
#include <stdexcept>

#ifdef _WIN32

bool g_isDdeOpen = false;

VivariumInterface::VivariumInterface(int major, int minor)
{
	_name = "Creatures";
	_engine = "Vivarium";
	versionMinor = minor;
	versionMajor = major;

	g_isDdeOpen = true;

	if (isCreatures2())
	{
		_name += " 2";
	}

	_pid = CreaturesSession::GetSession()->getPartnerPid();

	if(_pid)
		_workingDirectory = SharedMemoryInterface::GetWorkingDirectory(_pid);
}


VivariumInterface::~VivariumInterface()
{
	g_isDdeOpen = false;
}


std::unique_ptr<SharedMemoryInterface> VivariumInterface::OpenVivarium()
{
	static std::string str = "dde: putv vrsn";

	try
	{
		auto session = CreaturesSession::GetSession();
		auto version = session->executeMacro(str);

		int majorVersion{};
		int r = sscanf(version.data(), "%d", &majorVersion);

		if (r > 0)
		{
			return std::unique_ptr<SharedMemoryInterface>(new VivariumInterface(majorVersion, 0));
		}

		return nullptr;
	}
	catch (std::exception & e)
	{
		(void)e;
		return nullptr;
	}
}

VivariumInterface::Response VivariumInterface::send1252(std::string & message)
{
	try
	{
		auto session = CreaturesSession::GetSession();

		if (message == "BRAIN DUMP")
		{
			return Response{
				.text = session->getBrainActivity(),
				.isError = false,
				.isBinary=true,
			};
		}
		else
		{
			return Response{
				.text = session->executeMacro(message),
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

bool isProcessRunning(DWORD pid) {
	// Attempt to open the process with a minimal set of rights to query its status
	HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
	if (hProcess == NULL) {
		// If OpenProcess fails, the process might not exist or we don't have the permissions
		return false;
	}

	DWORD exitCode;
	if (GetExitCodeProcess(hProcess, &exitCode)) {
		// If the process is still active, GetExitCodeProcess returns STILL_ACTIVE
		CloseHandle(hProcess);
		return (exitCode == STILL_ACTIVE);
	}

	// Close the process handle if we've finished checking
	CloseHandle(hProcess);
	return false;
}

bool VivariumInterface::isClosed()
{
	if (!_isOpen || !g_isDdeOpen) 
		return true;

	if(_pid)
		return (_isOpen = !isProcessRunning(_pid));

	auto session = CreaturesSession::GetSession();
	auto fMonitor = session->connectConversation(session->fVivariumString, session->fSessionString);

	if (fMonitor == nullptr)
		_isOpen = false;
	else
	{
		session->disconnectConversation(fMonitor);
	}

	return !_isOpen;
}



#endif
