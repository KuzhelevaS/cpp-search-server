#pragma once

#include <random>
#include <vector>
#include <string>

std::string GenerateWord(std::mt19937& generator, int max_length);
std::vector<std::string> GenerateDictionary(std::mt19937& generator,
	int word_count, int max_length);
std::string GenerateQuery(std::mt19937& generator,
	const std::vector<std::string>& dictionary, int max_word_count);
std::vector<std::string> GenerateQueries(std::mt19937& generator,
	const std::vector<std::string>& dictionary,
	int query_count, int max_word_count);
std::string GenerateQueryStrict(std::mt19937& generator,
	const std::vector<std::string>& dictionary,
	int word_count, double minus_prob = 0);
std::vector<std::string> GenerateQueriesStrict(std::mt19937& generator,
	const std::vector<std::string>& dictionary,
	int query_count, int max_word_count);
