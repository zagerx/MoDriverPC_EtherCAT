#include "app/stdin_command_reader.h"

#include <poll.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <iostream>

#include "utils/logger.h"

namespace mo_ecat
{

ReadResult StdinCommandReader::Read(std::string &command, int timeout_ms)
{
	struct pollfd pfd {};
	pfd.fd = STDIN_FILENO;
	pfd.events = POLLIN;

	int ret = poll(&pfd, 1, timeout_ms);
	if (ret < 0) {
		if (errno == EINTR) {
			return ReadResult::kTimeout;
		}
		LOG_ERROR << "poll stdin failed: " << std::strerror(errno);
		return ReadResult::kError;
	}

	if (ret == 0) {
		return ReadResult::kTimeout;
	}

	if (!std::getline(std::cin, command)) {
		// EOF / Ctrl+D
		return ReadResult::kEof;
	}

	return ReadResult::kOk;
}

} // namespace mo_ecat
