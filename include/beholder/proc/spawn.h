#pragma once
#include <ext/stdio_filebuf.h> // NB: Specific to libstdc++
#include <beholder/proc/cpipe.h>
#include <memory>

/**
 * @brief Allows two way communication with a child process via stdin/stdout
 */
class spawn {
private:
	/**
	 * @brief Child stdin pipe
	 */
	cpipe write_pipe;
	/**
	 * @brief Child stdout pipe
	 */
	cpipe read_pipe;
public:
	pid_t child_pid{-1};
	std::unique_ptr<__gnu_cxx::stdio_filebuf<char> > write_buf{nullptr};
	std::unique_ptr<__gnu_cxx::stdio_filebuf<char> > read_buf{nullptr};
	std::ostream stdin;
	std::istream stdout;

	/**
	 * @brief Construct a new spawn object
	 * 
	 * @note Does not search PATH.
	 * @param argv Name and arguments to pass to child program
	 * @param envp environment to pass to child program
	 */
	spawn(const char* const argv[], const char* const envp[] = 0);

	/**
	 * @brief Send EOF to stdin on child program
	 */
	void send_eof();

	/**
	 * @brief Get the child process PID
	 * 
	 * @return pid_t child PID, or -1 if no child process running
	 */
	pid_t get_pid() const;
    
	/**
	 * @brief Wait and reap child process
	 * 
	 * @return int return code of child program
	 */
	int wait();
};

