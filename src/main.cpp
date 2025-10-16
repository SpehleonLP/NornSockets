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
#include <cstring>


bool g_Verbose = false;

using namespace std::chrono_literals;
static std::atomic<bool> g_running{true};
static std::condition_variable _mainSleep;
// needed out here by win console handler (annoying)

void split(std::vector<std::string_view> & out, std::string_view in, const char delim)
{
	out.clear();
	
	for(auto i = 0u, j = 0u; i < in.size(); i = j+1)
	{
		for(j = i; j < in.size() && in[j] != delim; ++j) {}
		
		out.push_back(in.substr(i, j-i));
	}
}


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

int main(int argc, const char * args[])
{
	for(int i = 0; i < argc; ++i)
	{
		if(strcmp("-v", args[i]) == 0
		|| strcmp("--verbose", args[i]) == 0)
			g_Verbose = true;
	}

// so signals can wake us up.
	std::mutex dummy_mutex;
	std::unique_lock lock(dummy_mutex);

	std::signal(SIGINT, signalHandler);
	std::signal(SIGTERM, signalHandler);

#ifndef _WIN32
	std::signal(SIGPIPE, SIG_IGN);
#else
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
	std::vector<std::string_view>		  tokens;

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
		bool wrote = false;

		if(log.size())
		{
			if (isDebugLogOpen)
				isDebugLogOpen = !debugLog->isClosed();

			for(auto & item : log)
			{
				fprintf(stdout, "%.*s", int(item.size()), item.data());
			}
		}

		if(!isC2E)
		{
			_mainSleep.wait_for(lock, 1s);
		}
		else if(isC2E)
		{
			bool wrote = false;
			auto response = interface->send("DBG: POLL");

			if (response.isError == true)
			{
				fprintf(stderr, "%s\n", response.text.data());
				continue;
			}

			if (response.text.size())
			{
				if (isDebugLogOpen)
					isDebugLogOpen = !debugLog->isClosed();

				split(tokens, response.text, '\n');
				
				for(auto tok : tokens)
				{
					if (!server->Parse(tok))
						fprintf(stdout, "%.*s\n", int(tok.size()), tok.data());					
				}
			}

			if(wrote)
			{
// TODO: if deamon open window to display stdout.
				if (debugLog == nullptr)
				{
				///	debugLog = DebugLog::Open();
				//	isDebugLogOpen = true;
				}

				fflush(stdout);
			}

			_mainSleep.wait_for(lock, 200ms);
		}
	}

	server.reset();
	ix::uninitNetSystem();

	return 0;
}
