// NornSockets.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "SharedMemoryInterface.h"
#include <ixwebsocket/IXNetSystem.h>
#include "WebsocketServer.h"
#include "DebugLog.h"
#include "Support.h"
#include <csignal>
#include <chrono>
#include <thread>
#include <condition_variable>
#include <mutex>



using namespace std::chrono_literals;
static std::atomic<bool> g_running{true};
static std::condition_variable _mainSleep;
// needed out here by win console handler (annoying)

bool IsRunning()
{
	return g_running;
}

void OnFatalError()
{
	g_running = false;
}

void signalHandler(int signal)
{
	if (signal == SIGINT || signal == SIGTERM)
	{
		g_running = false;
		_mainSleep.notify_all();
	}
}

#ifdef _WIN32

static std::mutex _mutex;
static std::condition_variable _condition;

BOOL WINAPI ConsoleHandler(DWORD signal) {
	if (signal == CTRL_CLOSE_EVENT || signal == CTRL_C_EVENT) {
		g_running = false;
		_mainSleep.notify_all();

		std::unique_lock lock(_mutex);
		_condition.wait(lock);

		return TRUE;
	}
	return FALSE;
}
#endif

int main()
{
// so signals can wake us up.
	std::mutex dummy_mutex;
	std::unique_lock lock(dummy_mutex);

	std::signal(SIGINT, signalHandler);
	std::signal(SIGTERM, signalHandler);
	std::signal(SIGPIPE, SIG_IGN);

#ifdef _WIN32
	if (!SetConsoleCtrlHandler(ConsoleHandler, TRUE)) {
		fprintf(stderr, "Could not set control handler: %s\n", GetLastErrorAsString().c_str());
		return 1;
	}
#endif

	if (ix::initNetSystem() == false)
	{
		fprintf(stderr, "Unable to open web interface.");
		return -1;
	}

// test debug log

	std::unique_ptr<WebsocketServer>	   server(new WebsocketServer);
	std::unique_ptr<SharedMemoryInterface> interface;
	std::unique_ptr<DebugLog>			  debugLog;

	debugLog = DebugLog::Open();
	if(debugLog)
	{
		 debugLog->write("test text\n");
		 debugLog->flush();
	}

	bool isDebugLogOpen = false;
	bool isC2E = false;

	while (IsRunning())
	{
		if (interface == nullptr)
		{
			interface = SharedMemoryInterface::Open();

			if (interface == nullptr)
			{
				_mainSleep.wait_for(lock, 1s);
				continue;
			}

			server->OnGameOpened(interface.get());
			_mainSleep.wait_for(lock, 50ms);
			isC2E = !interface->isDDE();
		}

		if (interface->isClosed())
		{
			server->OnGameClosed(interface.get());

			debugLog = nullptr;
			interface = nullptr;
			isDebugLogOpen = false;
			_mainSleep.wait_for(lock, 1s);
			continue;
		}

		auto log = DebugLog::GetDebugLog();

		if(log.size())
		{
			if (isDebugLogOpen)
				isDebugLogOpen = !debugLog->isClosed();

			if (debugLog == nullptr)
			{
				debugLog = DebugLog::Open();
				isDebugLogOpen = true;
			}

			if (!isDebugLogOpen)
			{
				for(auto & item : log)
					debugLog->write(item);
			}
		}

		if(!isC2E)
		{
			_mainSleep.wait_for(lock, 1s);
		}
		else if(isC2E)
		{
		//	auto response = interface->send("DBG: OUTS \"Test Text\" DBG: POLL");

			auto response = interface->send("OUTS \"Test Text\"");

			if (response.isError == true)
			{
				fprintf(stderr, "%s\n", response.text.data());
				continue;
			}

			if (response.text.size())
			{
				if (isDebugLogOpen)
					isDebugLogOpen = !debugLog->isClosed();

				size_t start = 0;
				size_t curr = 0;
				auto txt = response.text.data();

				for (curr = response.text.find("ws", curr); curr != std::string::npos; curr = response.text.find("ws", curr))
				{
					auto line_end = response.text.find("\n", curr);

					if (line_end == std::string::npos)
						line_end = response.text.size() - 1;

	// not a line begin
					if (curr != 0 && response.text[curr - 1] != '\n')
					{
						curr = line_end;
						continue;
					}

	// if we can't parse it it's nosie
					if (!server->Parse(std::string_view(txt + curr, txt + line_end)))
					{
						curr = line_end;
					}
					else if (curr != 0)
					{
						if (debugLog == nullptr)
						{
							debugLog = DebugLog::Open();
							isDebugLogOpen = true;
						}

						if (isDebugLogOpen)
							isDebugLogOpen |= debugLog->write(std::string_view(txt + start, txt + curr));

						start = line_end+1;
					}
				}

				if (start < response.text.size()-1)
				{
					if (debugLog == nullptr)
					{
						debugLog = DebugLog::Open();
						isDebugLogOpen = true;
					}

					if(isDebugLogOpen)
						isDebugLogOpen |= debugLog->write(std::string_view(txt + start, txt + response.text.size()));
				}
			}

			if(debugLog)
				 debugLog->flush();

			_mainSleep.wait_for(lock, 500ms);
		}
	}

	server.reset();
	ix::uninitNetSystem();

	return 0;
}
