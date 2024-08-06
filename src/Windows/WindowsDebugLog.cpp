#include "WindowsDebugLog.h"
#include <cstdio>

#ifdef _WIN32

static const wchar_t command[] = L"powershell.exe -NoLogo -NoProfile -Command \"[console]::InputEncoding = [System.Text.Encoding]::UTF8; while ($true) { $line = [Console]::ReadLine(); Write-Output $line }\"";

std::string GetLastErrorAsString();

WindowsDebugLog::WindowsDebugLog()
{
	ZeroMemory(&pi, sizeof(PROCESS_INFORMATION));

// for some stupid reason it needs a wchar_t* not a const wchar_t*
	wchar_t buffer[sizeof(command)];
	memcpy(buffer, command, sizeof(command));

	// Set up the security attributes
	SECURITY_ATTRIBUTES sa;
	sa.nLength = sizeof(SECURITY_ATTRIBUTES);
	sa.bInheritHandle = TRUE;  // Allow the handle to be inherited
	sa.lpSecurityDescriptor = NULL;

	// Create a pipe for the child process's STDIN
	//  this line  crashes
	try {
		if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0)) {
			fprintf(stderr, "Could not create pipe: %s\n", GetLastErrorAsString().c_str());
			return;
		}

		if (!SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0))
		{
			fprintf(stderr, "Could not set handle information: %s\n", GetLastErrorAsString().c_str());
			return;
		}
	}
	catch (const std::exception& e) {
		fprintf(stderr, "Exception caught: %s\n", e.what());
		return;
	}


	// Set up the start up info
	STARTUPINFO si;
	ZeroMemory(&si, sizeof(STARTUPINFO));
	si.cb = sizeof(STARTUPINFO);
	si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
	si.hStdInput = hReadPipe;
	si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
	si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
	si.wShowWindow = SW_SHOW;

	// Create the child process
	if (!CreateProcess(NULL, 
		buffer,         // Command line
		NULL,           // Process handle not inheritable
		NULL,			// Thread handle not inheritable
		TRUE,          // Set handle inheritance to FALSE
		CREATE_NEW_CONSOLE, // Creation flags
		NULL,           // Use parent's environment block
		NULL,           // Use parent's starting directory 
		&si,            // Pointer to STARTUPINFO structure
		&pi             // Pointer to PROCESS_INFORMATION structure
	)) {
		fprintf(stderr, "Failed to create process. Error code: %s\n", GetLastErrorAsString().c_str());
		CloseHandle(hReadPipe);
		CloseHandle(hWritePipe);
		hReadPipe = NULL;
		hWritePipe = NULL;
		return;
	}
}

WindowsDebugLog::~WindowsDebugLog()
{
	//TerminateProcess(pi.hProcess, 0);
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);
	CloseHandle(hWritePipe);
	CloseHandle(hReadPipe);
}


bool WindowsDebugLog::write(std::string_view message)
{
// fail silently
	if (hWritePipe == nullptr || message.empty())
		return false;

	DWORD written;
	if (!WriteFile(hWritePipe, message.data(), static_cast<DWORD>(message.size()), &written, nullptr)) {
		wrote = true;
		auto error = GetLastErrorAsString();
		fprintf(stderr, "Failed to write to child process input: %s\n", error.c_str());
		fwrite(message.data(), message.size(), 1, stdout);
		return false;
	}		

	wrote = true;

	return true;
}

bool WindowsDebugLog::isClosed()
{
	if (pi.hProcess == 0)
		return true;

	DWORD exitCode;
	GetExitCodeProcess(pi.hProcess, &exitCode);
	return exitCode != STILL_ACTIVE;
}

void WindowsDebugLog::flush()
{
	if(wrote)
	{
		wrote = false;

		// Flush the pipe
		if (!FlushFileBuffers(hWritePipe)) {
			auto error = GetLastErrorAsString();
			fprintf(stderr, "Failed to flush pipe: %s\n", error.c_str());
		}
	}
}

#endif
