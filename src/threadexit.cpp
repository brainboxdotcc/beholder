#include <stack> 
#include <functional>

void on_thread_exit(std::function<void()> func)
{
	class thread_exit_t
	{
		std::stack<std::function<void()>> exit_funcs;

	public:

		thread_exit_t() = default;
		thread_exit_t(thread_exit_t const&) = delete;
		void operator=(thread_exit_t const&) = delete;
		~thread_exit_t() {
			while (!exit_funcs.empty()) {
				exit_funcs.top()();
				exit_funcs.pop();
			}
		}

		void add(std::function<void()> func) {
			exit_funcs.push(std::move(func));
		}   
	};

	thread_local thread_exit_t exiter;
	exiter.add(std::move(func));
}
