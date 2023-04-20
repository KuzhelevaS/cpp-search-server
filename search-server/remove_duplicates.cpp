#include "remove_duplicates.h"

#include <set>
#include <string>
#include <iostream>
#include <algorithm>

using std::literals::string_literals::operator""s;

void RemoveDuplicates(SearchServer& search_server) {
	std::set<int> duplicates;
	for (int document_id : search_server) {
		if (duplicates.count(document_id)) {
			continue;
		}
		for (int copy_id : search_server) {
			if (duplicates.count(copy_id)) {
				continue;
			}
			std::set<std::string> document_words;
			for (const auto& [word, freq] : search_server.GetWordFrequencies(document_id)) {
				document_words.insert(word);
			}

			std::set<std::string> copy_words;
			for (const auto& [word, freq] : search_server.GetWordFrequencies(copy_id)) {
				copy_words.insert(word);
			}

			if (document_words == copy_words && document_id != copy_id) {
				int max_id = std::max(document_id, copy_id);
				duplicates.insert(max_id);
			}
		}
	}
	for (int document_id : duplicates) {
		std::cout << "Found duplicate document id "s << document_id << std::endl;
		search_server.RemoveDocument(document_id);
	}
}
