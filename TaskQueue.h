#pragma once
#include <queue>
#include <thread>
#include <shared_mutex>
#include <mutex>
#include <iostream>

template <typename T>
class taskQueue
{
public:
	inline taskQueue() = default;
	inline ~taskQueue() { clear(); }

	inline bool empty() const;
	inline size_t size() const;

	inline void clear()
	{
		std::unique_lock<std::shared_mutex> _(readWriteMutex);
		while (!tasks.empty())
			tasks.pop();
	}
	inline bool pop(T& task)
	{
		std::unique_lock<std::shared_mutex> _(readWriteMutex);
		if (tasks.empty())
			return false;
		task = std::move(tasks.front());
		tasks.pop();
		return true;
	}

	template <typename... args>
	inline void emplace(args&&... params)
	{
		std::unique_lock<std::shared_mutex> _(readWriteMutex);
		tasks.emplace(std::forward<args>(params)...);
		outputMutex.lock();
		std::cout << "A new task is added to the queue" << '\n';
		outputMutex.unlock();
	}

	taskQueue(const taskQueue& queue);
	taskQueue(taskQueue&& queue) noexcept;
	taskQueue& operator=(const taskQueue& queue);
	taskQueue& operator=(taskQueue&& queue) noexcept;

	std::mutex outputMutex;

private:
	mutable std::shared_mutex readWriteMutex;
	std::queue<T> tasks;
};