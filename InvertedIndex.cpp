#include "InvertedIndex.h"

void InvertedIndex::AddDocument(const std::string& document, int documentID)
{
	std::vector<std::string> words = TokenizeString(document);

	for (auto& word : words)
	{
		index[word].push_back(documentID);
		reverseIndex[documentID].insert(word);
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
			std::vector<int>& docIDs = index[word];
			docIDs.erase(std::remove(docIDs.begin(), docIDs.end(), documentID), docIDs.end());
		}

		reverseIndex.erase(it);
	}
}

std::vector<int> InvertedIndex::Search(const std::string& query)
{
	std::vector<std::string> words = TokenizeString(query);

	auto it = index.find(words[0]);
	if (it != index.end())
	{
		for (int i = 1; i < words.size(); i++)
		{
			it++;
			if (it->first != words[i])
				return {};
		}
		return it->second;
	}
	return {};
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

std::vector<std::string> InvertedIndex::TokenizeString(const std::string& document)
{
	std::istringstream iss(document);
	std::vector<std::string> words;
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
			return std::ispunct(static_cast<unsigned char>(c));
			}));
	}

	// Remove stop words
	words.erase(std::remove_if(words.begin(), words.end(), [&stopWords](const std::string& word) {
		return stopWords.find(word) != stopWords.end();
		}), words.end());
		
	return words;
}