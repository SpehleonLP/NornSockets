#include "WindowsSMI.h"
#include "../Support.h"
#include <vector>
#include <algorithm>

#ifdef _WIN32

WindowsSMI::WindowsSMI()
{
	_engine = "C2E";
}

WindowsSMI::~WindowsSMI()
{
	if (result_event)	CloseHandle(result_event);
	if (request_event)	CloseHandle(request_event);
	if (mutex)			CloseHandle(mutex);
	if (memory_ptr)		UnmapViewOfFile(memory_ptr);
	if (memory_handle)	CloseHandle(memory_handle);
	if (creator_process_handle) CloseHandle(creator_process_handle);
}

void WindowsSMI::Initialize()
{
	_workingDirectory = GetWorkingDirectory(memory_ptr->pid);

	std::string cmd = "outv vmjr outs \" \" outv vmnr outs \" \" outx gnam";
	auto version = WindowsSMI::send1252(cmd);

	if (version.isError)
	{
		fprintf(stderr, "%s", version.text.c_str());
	}
	else
	{
		char buffer[256];
		buffer[0] = 0;
		int r = sscanf(version.text.c_str(), "%d %d \"%255[^\"]\"", &versionMajor, &versionMinor, buffer);

		if (r < 3)
		{
			fprintf(stderr, "failed to get version of engine.\n");
		}
		else
			_name = *buffer == '"'? buffer+1 : buffer;
	}
}

std::unique_ptr<SharedMemoryInterface> WindowsSMI::Open(const char* name)
{
	enum
	{
		BUFFER_SIZE = 64,
	};

	wchar_t buffer[BUFFER_SIZE];
	size_t name_len = strlen(name);
	size_t converted_len = 0;
	wchar_t wname[BUFFER_SIZE];

	// Convert char* to wchar_t*
	mbstowcs_s(&converted_len, wname, name_len + 1, name, _TRUNCATE);

	std::unique_ptr<WindowsSMI> r(new WindowsSMI);

	swprintf_s(buffer, BUFFER_SIZE, L"%s_mem", wname);
	r->memory_handle = OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, buffer);

	if (r->memory_handle == nullptr)
	{
	//	fprintf(stderr, "Unable to open %s memory", name);
		return nullptr;
	}

	r->memory_ptr = (Message*)MapViewOfFile(r->memory_handle, FILE_MAP_ALL_ACCESS, 0, 0, 0);

	swprintf_s(buffer, BUFFER_SIZE, L"%s_mutex", wname);
	r->mutex = OpenMutex(MUTEX_ALL_ACCESS, FALSE, buffer);

	swprintf_s(buffer, BUFFER_SIZE, L"%s_result", wname);
	r->result_event = OpenEvent(EVENT_ALL_ACCESS, FALSE, buffer);

	swprintf_s(buffer, BUFFER_SIZE, L"%s_request", wname);
	r->request_event = OpenEvent(EVENT_ALL_ACCESS, FALSE, buffer);
	if (r->mutex == nullptr)
	{
		fprintf(stderr, "Unable to open %s mutex", name);
		return nullptr;
	}

	if (r->result_event == nullptr)
	{
		fprintf(stderr, "Unable to open %s result_event", name);
		return nullptr;
	}

	if (r->request_event == nullptr)
	{
		fprintf(stderr, "Unable to open %s request_event", name);
		return nullptr;
	}

	// Open a handle to the creator process
	r->creator_process_handle = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, r->memory_ptr->pid);
	r->Initialize();

	return std::unique_ptr<SharedMemoryInterface>(r.release());
}


WindowsSMI::Response WindowsSMI::send1252(std::string & text)
{
	if (text.empty())
	{
		return {};
	}

	auto r = WaitForSingleObject(mutex, INFINITE);

	if (r == WAIT_ABANDONED)
	{
		return Response{
			.text = "Failed to lock engine mutex. (restart engine)",
			.isError = true
		};
	}

	if (r == WAIT_FAILED)
	{
		return Response{
			.text = "Failed to lock engine mutex. " + GetLastError(),
			.isError = true
		};
	}

	if (r != WAIT_OBJECT_0)
	{
		if (isClosed())
		{
			return Response{
				.text = "Engine is closed.",
				.isError = true
			};
		}

		return Response{
			.text = "Failed to lock engine mutex. (unknown reason)",
			.isError = true
		};
	}

	uint32_t headerSize = 0;
	bool isScript = strncmp(text.data(), "srcp", 4) == 0;
	auto size_limit = memory_ptr->memBufferSize - sizeof(Message) - sizeof("execute\n");

	auto chunks = ChunkMessage(text, memory_ptr->memBufferSize - sizeof(Message) - sizeof("execute\n"));

	for (auto& chunk : chunks)
	{
		if (chunk.size() > size_limit)
		{
			ReleaseMutex(mutex);

			char buffer[256];
			snprintf(buffer, sizeof(buffer), "message is too long! %d vs %d", chunk.size(), size_limit);

			return Response{
				.text = buffer,
				.isError = true
			};
		}
	}

	// append execute\n
	std::string result;
	HANDLE process_handle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, memory_ptr->pid);

	for (auto& chunk : chunks)
	{
		uint32_t header_size = 0;
		if (chunk.find("srcp") == std::string_view::npos)
		{
			headerSize = sizeof("execute\n") - 1;
			strcpy_s(memory_ptr->contents, memory_ptr->memBufferSize - sizeof(Message), "execute\n");
		}

		strncpy_s(memory_ptr->contents + headerSize,
			memory_ptr->memBufferSize - sizeof(Message),
			chunk.data(),
			chunk.size());

		// add extra null terminator
		memory_ptr->byteLength = headerSize + (chunk.size());
		*(memory_ptr->contents + memory_ptr->byteLength) = 0;

		ResetEvent(result_event);
		PulseEvent(request_event);

		HANDLE process_handle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, memory_ptr->pid);
		HANDLE wait_handles[2];
		wait_handles[0] = process_handle;
		wait_handles[1] = result_event;

		r = WaitForMultipleObjects(2, wait_handles, FALSE, INFINITE);

		if (r == WAIT_OBJECT_0 + 1)
			result += std::string_view(memory_ptr->contents, memory_ptr->byteLength-1);
		else
			break;
	}

	CloseHandle(process_handle);
	ReleaseMutex(mutex);

	if (r == WAIT_OBJECT_0 + 1)
		return Response{
			.text = std::move(result),
			.isError = false
	};

	if (r == WAIT_OBJECT_0)
	{
		_isClosed = true;
		return {};
	}

	return Response{
		.text = "error: idk why this happens.\n r = " + std::to_string(r) + "\n",
		.isError = true
	};
}

bool WindowsSMI::isClosed()
{
	if (_isClosed) return true;

	if (creator_process_handle == nullptr) {
		_isClosed = true;
		return true;
	}

	DWORD exit_code;
	if (GetExitCodeProcess(creator_process_handle, &exit_code)) {
		if (exit_code != STILL_ACTIVE) {
			_isClosed = true;
			CloseHandle(creator_process_handle);
			creator_process_handle = nullptr;
			return true;
		}
	}
	else {
		_isClosed = true;
		CloseHandle(creator_process_handle);
		creator_process_handle = nullptr;
		return true;
	}

	return _isClosed;
}


#endif
