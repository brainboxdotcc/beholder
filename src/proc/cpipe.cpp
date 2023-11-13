/************************************************************************************
 * 
 * Beholder, the image filtering bot
 *
 * Copyright 2019,2023 Craig Edwards <support@sporks.gg>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ************************************************************************************/
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
