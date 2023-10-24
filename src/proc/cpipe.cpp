#include <beholder/proc/cpipe.h>
#include <sys/wait.h>
#include <stdexcept>

const int cpipe::read_fd() const {
	return fd[0];
}

const int cpipe::write_fd() const {
	return fd[1];
}

cpipe::cpipe() {
	if (pipe(fd)) {
		throw std::runtime_error("Failed to create pipe");
	}
}

void cpipe::close() {
	::close(fd[0]);
	::close(fd[1]);
}

/**
 * @brief Destroy the cpipe object
 */
cpipe::~cpipe() {
	this->close();
}
