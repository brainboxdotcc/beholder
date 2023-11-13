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
