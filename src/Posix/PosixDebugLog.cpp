#include "PosixDebugLog.h"

#ifndef _WIN32
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <cstring>
#include <stdexcept>


PosixDebugLog::PosixDebugLog()
{
	int pipefd[2];
	if (pipe(pipefd) == -1) {
		fprintf(stderr, "Could not create pipe.\n");
		return;
	}

	childPid = fork();
	if (childPid == -1) {
		close(pipefd[0]);
		close(pipefd[1]);
		fprintf(stderr, "Failed to fork.\n");
		return;
	}

	if (childPid == 0) {  // Child process
		close(pipefd[1]);  // Close the write end in the child
		dup2(pipefd[0], STDIN_FILENO);  // Redirect standard input to the read end of the pipe
		close(pipefd[0]);

		execlp("xterm", "xterm", "-e", "cat", nullptr);
		execlp("gnome-terminal", "gnome-terminal", "--", "cat", nullptr);
		exit(EXIT_FAILURE);  // In case execlp fails
	}
	else {  // Parent process
		close(pipefd[0]);  // Close the read end in the parent
		fdWrite = pipefd[1];
	}
}

PosixDebugLog::~PosixDebugLog()
{
	if (fdWrite != -1) {
		close(fdWrite);
	}
	waitpid(childPid, nullptr, 0);  // Wait for the child process to exit
}

bool PosixDebugLog::write(std::string_view message) 
{
	if (fdWrite == 0) return false;
	
	if (::write(fdWrite, message.data(), message.size()) == -1) {
		fprintf(stderr, "Failed to write to child process: %s.\n", strerror(errno));
		return false;
	}
	
	_wrote = true;
	return true;
}

bool PosixDebugLog::isClosed() 
{
	int status;
	pid_t result = waitpid(childPid, &status, WNOHANG);
	if (result == 0) {
		return false;  // Child still running
	}
	else {
		return true;  // Child exited
	}
}

void PosixDebugLog::flush()
{
	if(_wrote)
	{
		_wrote= false;
		fsync(fdWrite);
	}

}

#endif
