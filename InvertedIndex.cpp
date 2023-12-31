#include "InvertedIndex.h"

void InvertedIndex::AddDocument(const std::string& document, int documentID)
{
	std::vector<std::string> words;
	TokenizeString(document, words);
	try
	{
		for (auto& word : words)
		{
			std::unique_lock<std::shared_mutex> _(readWriteMutex);
			index[word].insert(documentID);
			reverseIndex[documentID].insert(word);
		}
	}
	catch (const std::exception& e)
	{
		std::cout << e.what() << '\n';
	}
	
}

void InvertedIndex::RemoveDocument(int documentID)
{
	auto it = reverseIndex.find(documentID);
	if (it != reverseIndex.end())
	{
		const std::unordered_set<std::string>& words = it->second;

		for (auto& word : words)
		{
			std::unordered_set<int>& docIDs = index[word];
			docIDs.erase(documentID);
		}

		reverseIndex.erase(it);
	}
}

void InvertedIndex::Search(const std::string& query, std::unordered_set<int>& result, TaskState& taskState)
{
	std::vector<std::string> words;
	TokenizeString(query, words);

	std::shared_lock<std::shared_mutex> _(readWriteMutex);

	try
	{
		if (!words.empty())
		{
			auto it = index.find(words[0]);
			if (it != index.end()) {
				int i = 1;
				while (i < words.size() && it != index.end()) {
					if (it->first != words[i]) {
						result = {};
						break;
					}
					i++;
					it++;
				}
				if (i == words.size()) {
					result = it->second;
				}
			}
			else {
				result = {};
			}
		}
		else
		{
			result = {};
		}
		taskState = TaskState::Completed;
		
	}
	catch (const std::exception& e)
	{
		std::cerr << e.what() << '\n';
	}
	
}

void InvertedIndex::Clear()
{
	index.clear();
	reverseIndex.clear();
}

size_t InvertedIndex::Size()
{
	return index.size();
}

void InvertedIndex::TokenizeString(const std::string& document, std::vector<std::string>& words)
{
	std::istringstream iss(document);
	std::string word;

	while (iss >> word) {
		std::transform(word.begin(), word.end(), word.begin(),
			[](unsigned char c) { return std::tolower(c); });
		words.push_back(word);
	}
	std::unordered_set<std::string> stopWords = { "a", "the", "and", "of", "to", "is", "in" };

	// Remove punctuation chars
	for (auto& word : words)
	{
		word.erase(std::remove_if(word.begin(), word.end(), [](char c) {
			auto temp = std::ispunct(static_cast<unsigned char>(c));
			return temp;
			}));
	}

	// Remove stop words
	words.erase(std::remove_if(words.begin(), words.end(), [&stopWords](const std::string& word) {
		return stopWords.find(word) != stopWords.end();
		}), words.end());		
}