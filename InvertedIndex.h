#pragma once
#include <unordered_map>
#include <shared_mutex>
#include <algorithm>
#include <string>
#include <iostream>
#include <unordered_set>
#include <sstream>

class InvertedIndex
{
public:
	void AddDocument(const std::string& document, int documentID);
	void RemoveDocument(int documentID);
	void Search(const std::string& query, std::unordered_set<int>& result);
	void Clear();
	size_t Size();

private:
	std::vector<std::string> TokenizeString(const std::string& document);

	std::unordered_map<std::string, std::unordered_set<int>> index;
	std::shared_mutex readWriteMutex;
	std::unordered_map<int, std::unordered_set<std::string>> reverseIndex;
};