#include "TaskQueue.h"

template <typename T>
bool taskQueue<T>::empty() const
{
	std::shared_lock<std::shared_mutex> _(readWriteMutex);
	return tasks.empty();
}

template <typename T>
size_t taskQueue<T>::size() const
{
	std::shared_lock<std::shared_mutex> _(readWriteMutex);
	return tasks.size();
}

template <typename T>
taskQueue<T>& taskQueue<T>::operator=(taskQueue<T>&& other) noexcept
{
	if (this != &other) {
		std::unique_lock<std::shared_mutex> _(readWriteMutex);
		std::unique_lock<std::shared_mutex> __(other.readWriteMutex);
		tasks = std::move(other.tasks);
	}
	return *this;
}

template <typename T>
taskQueue<T>::taskQueue(const taskQueue& other)
{
	std::unique_lock<std::shared_mutex> _(other.readWriteMutex);
	tasks = other.tasks;
}

template <typename T>
taskQueue<T>::taskQueue(taskQueue<T>&& other) noexcept {
	std::unique_lock<std::shared_mutex> _(other.readWriteMutex);
	std::swap(tasks, other.tasks);
}

template<typename T>
taskQueue<T>& taskQueue<T>::operator=(const taskQueue<T>& other) {
	if (this != &other) {
		std::unique_lock<std::shared_mutex> _(readWriteMutex);
		std::unique_lock<std::shared_mutex> _(other.readWriteMutex);
		tasks = other.tasks;
	}
	return *this;
}