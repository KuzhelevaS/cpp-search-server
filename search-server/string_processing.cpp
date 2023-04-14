#include "string_processing.h"

using std::literals::string_literals::operator""s;

std::vector<std::string> SplitIntoWords(const std::string & text) {
	std::vector<std::string> words;
	std::string word = ""s;
	for (const char c : text) {
		if (c == ' ') {
			if (!word.empty()) {
				words.push_back(word);
				word.clear();
			}
		} else {
			word += c;
		}
	}
	if (!word.empty()) {
		words.push_back(word);
	}
	return words;
}
