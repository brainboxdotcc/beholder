#pragma once

/**
 * @brief RAII representation of a pair of pipe fds
 */
class cpipe {
private:
	/**
	 * @brief File descriptors
	 */
	int fd[2];
public:
	/**
	 * @brief Get the read side of the pipe fds
	 * 
	 * @return const int 
	 */
	const int read_fd() const;

	/**
	 * @brief Get the write side of the pipe fds
	 * 
	 * @return const int 
	 */
	const int write_fd() const;

	/**
	 * @brief Construct a new cpipe object
	 */
	cpipe();

	/**
	 * @brief Close both file desciptors of the pipe
	 */
	void close();

	/**
	 * @brief Destroy the cpipe object
	 */
	~cpipe();
};