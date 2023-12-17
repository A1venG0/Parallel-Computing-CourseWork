#include "ThreadPool.h"

bool threadPool::working() const
{
	std::shared_lock<std::shared_mutex> _(readWriteMutex);
	return workingUnsafe();
}

bool threadPool::workingUnsafe() const
{
	return isInitialized && !isTerminated;
}

void threadPool::initilize(const size_t threadCount)
{
	std::unique_lock<std::shared_mutex> _(readWriteMutex);

	if (isInitialized || isTerminated)
		return;

	threads.reserve(threadCount);
	for (int i = 0; i < threadCount; i++)
	{
		outputMutex.lock();
		std::cout << "A new thread has been created" << '\n';
		outputMutex.unlock();
		threads.emplace_back(&threadPool::routine, this, i);
	}
	isInitialized = !threads.empty();
}

void threadPool::terminate()
{
	{
		std::unique_lock<std::shared_mutex> _(readWriteMutex);

		if (workingUnsafe())
		{
			isTerminated = true;
		}
		else
		{
			threads.clear();
			isTerminated = false;
			isInitialized = false;
			return;
		}
	}
	for (auto& thread : threads)
	{
		thread.join();
	}

	threads.clear();
	isTerminated = true;
	isInitialized = false;
}

void threadPool::terminate_immidiately()
{
	std::unique_lock<std::shared_mutex> _(readWriteMutex);
	if (workingUnsafe())
	{
		isTerminated = true;
	}
	else
	{
		threads.clear();
		isTerminated = false;
		isInitialized = false;
		return;
	}


	for (auto& thread : threads)
	{
		thread.detach();
	}

	threads.clear();
	isTerminated = true;
	isInitialized = false;


}

void threadPool::temporarilyStopWorking(int givenTime)
{
	std::this_thread::sleep_for(std::chrono::seconds(givenTime));
}


void threadPool::routine(int threadId)
{
	while (true)
	{
		/*outputMutex.lock();
		std::cout << "Thread " << threadId << " before the lock" << '\n';
		outputMutex.unlock();*/
		auto start = std::chrono::high_resolution_clock::now();
		threadFinishedMutex.lock();
		auto end = std::chrono::high_resolution_clock::now();
		/*outputMutex.lock();
		std::cout << "Thread " << threadId << " after the lock" << '\n';
		outputMutex.unlock();*/
		if (!isRunning)
		{
			/*outputMutex.lock();
			std::cout << "Thread " << threadId << " is waiting for 5 seconds" << '\n';
			outputMutex.unlock();*/
			isRunning = true;
			
			std::this_thread::sleep_for(std::chrono::seconds(45));

			/*outputMutex.lock();
			std::cout << "Thread " << threadId << " swapped the buffers" << '\n';
			outputMutex.unlock();*/
			std::swap(tasks, tasksTwo);
		}
		threadFinishedMutex.unlock();
		

		bool taskAcquired = false;
		std::function<void()> task;
		{
			auto startTwo = std::chrono::high_resolution_clock::now();
			std::unique_lock<std::shared_mutex> _(readWriteMutex);;
			auto endTwo = std::chrono::high_resolution_clock::now();
			/*outputMutex.lock();
			std::cout << "Thread " << threadId << " before the wait condition" << '\n';
			outputMutex.unlock();*/
			auto waitCondition = [this, &taskAcquired, &task] {
				taskAcquired = tasks.pop(task);
				return isTerminated || taskAcquired;
			};

			waitCondition();
			/*outputMutex.lock();
			std::cout << "Thread " << threadId << " task acquired: " << taskAcquired << '\n';
			outputMutex.unlock();*/
			outputMutex.lock();
			overallWaitingTime += std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() + std::chrono::duration_cast<std::chrono::milliseconds>(endTwo - startTwo).count();
			outputMutex.unlock();
			if (isTerminated && !taskAcquired && tasksTwo.empty())
				return;

			if (!taskAcquired) {
				isRunning = false;
				continue;
			}
		}
		outputMutex.lock();
		std::cout << "Thread " << threadId << " started task execution" << '\n';
		outputMutex.unlock();
		task();
		outputMutex.lock();
		std::cout << "Task finished by " << threadId << '\n';
		std::cout << "-----------------------------------" << '\n';
		outputMutex.unlock();
	}
	
}