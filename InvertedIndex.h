#pragma once
#include <unordered_map>
#include <shared_mutex>
#include <algorithm>
#include <string>
#include <iostream>
#include <unordered_set>
#include <sstream>
#include "TaskState.h"

class InvertedIndex
{
public:
	void AddDocument(const std::string& document, int documentID);
	void RemoveDocument(int documentID);
	void Search(const std::string& query, std::unordered_set<int>& result, TaskState& taskState);
	void Clear();
	size_t Size();

private:
	void TokenizeString(const std::string& document, std::vector<std::string>& words);

	std::unordered_map<std::string, std::unordered_set<int>> index;
	std::shared_mutex readWriteMutex;
	std::unordered_map<int, std::unordered_set<std::string>> reverseIndex;
};