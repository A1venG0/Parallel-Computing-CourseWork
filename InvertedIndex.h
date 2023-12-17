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
	std::vector<int> Search(const std::string& query);
	void Clear();
	size_t Size();

private:
	std::vector<std::string> TokenizeString(const std::string& document);

	std::unordered_map<std::string, std::vector<int>> index;
	std::unordered_map<int, std::unordered_set<std::string>> reverseIndex;
};