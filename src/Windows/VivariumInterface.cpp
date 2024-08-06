#include "VivariumInterface.h"
#include "CreaturesSession.h"
#include <stdexcept>

#ifdef _WIN32

bool g_isDdeOpen = false;

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

bool VivariumInterface::isClosed()
{
	if (!_isOpen || !g_isDdeOpen) 
		return false;

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
}


VivariumInterface::~VivariumInterface()
{
	g_isDdeOpen = false;
}



#endif
