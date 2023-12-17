#pragma once

#include <vector>
#include <functional>
#include <iostream>

#include "TaskQueue.h"
#include "TaskQueue.cpp"

class threadPool
{
public:
	inline threadPool() = default;
	inline ~threadPool() { terminate(); }

	void initilize(const size_t threadCount);
	void terminate();
	void terminate_immidiately();
	void temporarilyStopWorking(int givenTime);
	void routine(int threadId);

	bool working() const;
	bool workingUnsafe() const;

	template <typename T, typename... args>
	inline void add_task(T&& task, args&&... params)
	{
		{
			std::unique_lock<std::shared_mutex> _(readWriteMutex);
			if (!workingUnsafe())
			{
				return;
			}
		}

		auto bind = std::bind(std::forward<T>(task), std::forward<args>(params)...);

		tasksTwo.emplace(bind);
	}

	threadPool(threadPool& pool) = delete;
	threadPool(threadPool&& pool) = delete;
	threadPool& operator=(threadPool& pool) = delete;
	threadPool& operator=(threadPool&& pool) = delete;

	long long overallWaitingTime = 0;

private:
	mutable std::shared_mutex readWriteMutex;
	std::mutex threadFinishedMutex;
	std::mutex outputMutex;
	std::vector<std::thread> threads;
	taskQueue<std::function<void()>> tasks;
	taskQueue<std::function<void()>> tasksTwo;

	bool isInitialized = false;
	bool isTerminated = false;
	bool isRunning = false;
};