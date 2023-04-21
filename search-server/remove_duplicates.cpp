#include "remove_duplicates.h"

#include <set>
#include <string>
#include <iostream>
#include <algorithm>

using std::literals::string_literals::operator""s;

void RemoveDuplicates(SearchServer& search_server) {
	std::vector<int> duplicates;
	std::set<std::set<std::string>> scanned;
	for (int document_id : search_server) {
		std::set<std::string> document_words;
		for (const auto& [word, freq] : search_server.GetWordFrequencies(document_id)) {
			document_words.insert(word);
		}
		if (scanned.count(document_words)) {
			duplicates.push_back(document_id);
		} else {
			scanned.insert(document_words);
		}
	}
	for (int document_id : duplicates) {
		std::cout << "Found duplicate document id "s << document_id << std::endl;
		search_server.RemoveDocument(document_id);
	}
}
