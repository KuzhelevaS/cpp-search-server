#pragma once

#include <vector>
#include <string>
#include <string_view>
#include <algorithm>

//Разбивает строку на массив строк, разделитель пробел(ы)
std::vector<std::string> SplitIntoWords(const std::string & text);
std::vector<std::string_view> SplitIntoWordsView(std::string_view str);

//Сортирует и удаляет дубликаты в векторе строк
template <typename ExecutionPolicy, typename Container>
void SortAndRemoveDuplicates(ExecutionPolicy && policy, Container & words) {
	std::sort(policy, words.begin(), words.end());
	auto last = std::unique(policy, words.begin(), words.end());
	words.erase(last, words.end());
}
