#include "search_server.h"
#include <numeric>
#include <cmath>

using std::literals::string_literals::operator""s;

SearchServer::SearchServer(const std::string & text)
	: SearchServer((SplitIntoWords(text)))
{}

void SearchServer::AddDocument(int document_id, const std::string & document,
	DocumentStatus status, const std::vector<int> & ratings)
{
	if (document_id < 0) {
		throw std::invalid_argument("Id less then zero");
	}
	if (document_info_.count(document_id)) {
		throw std::invalid_argument("Document with this id already exists");
	}

	const std::vector<std::string> words = SplitIntoWordsNoStop(document);
	if (!IsValidAllWords(words)) {
		throw std::invalid_argument("Document contain special characters");
	}
	adding_history_.push_back(document_id);
	document_count_++;
	document_info_[document_id] = {ComputeAverageRating(ratings), status};
	double tf_coeff = 1.0 / static_cast<double>(words.size());
	for(const auto & word : words) {
		documents_with_tf_[word][document_id] += tf_coeff;
	}
}

std::vector<Document> SearchServer::FindTopDocuments(const std::string & raw_query,
	DocumentStatus status) const
{
	return FindTopDocuments(raw_query,
		[status](int, DocumentStatus document_status, int) {
			return document_status == status;
		});
}

int SearchServer::GetDocumentCount() const {
	return static_cast<int>(document_info_.size());
}

int SearchServer::GetDocumentId(int index) const {
	return adding_history_.at(static_cast<unsigned>(index));
}

std::tuple<std::vector<std::string>, DocumentStatus> SearchServer::MatchDocument(
	const std::string & raw_query, int document_id) const
{
	const Query query_words = ParseQuery(raw_query);

	bool need_check_plus_words = true;
	for (const std::string & minus_word : query_words.minus_words) {
		if (HasWordInDocument(minus_word, document_id)) {
			need_check_plus_words = false;
		}
	}

	std::vector<std::string> matched_words;
	if (need_check_plus_words) {
		for (const std::string & plus_word : query_words.plus_words) {
			if (HasWordInDocument(plus_word, document_id)) {
				matched_words.push_back(plus_word);
			}
		}
		std::sort(matched_words.begin(), matched_words.end());
	}
	return std::tie(matched_words, document_info_.at(document_id).status);
}

bool SearchServer::IsStopWord(const std::string & word) const {
	return stop_words_.count(word) != 0;
}

std::vector<std::string> SearchServer::SplitIntoWordsNoStop(const std::string& text) const {
	std::vector<std::string> words;
	for (const std::string & word : SplitIntoWords(text)) {
		if (!IsStopWord(word)) {
			words.push_back(word);
		}
	}
	return words;
}

SearchServer::QueryWord SearchServer::CheckWord(const std::string & word) const {
	QueryWord checked_word;

	if (word[0] == '-') {
		checked_word.is_minus = true;
	} else {
		checked_word.is_minus = false;
	}

	checked_word.word = checked_word.is_minus ? word.substr(1) : word;

	if (IsStopWord(checked_word.word)) {
		checked_word.is_stop = true;
	} else {
		checked_word.is_stop = false;
	}

	return checked_word;
}

SearchServer::Query SearchServer::ParseQuery(const std::string & text) const {
	Query query;
	for (const std::string & word : SplitIntoWords(text)) {
		if (!IsValidWord(word)) {
			throw std::invalid_argument("Query contain special characters");
		}
		QueryWord checked_word = CheckWord(word);
		if (checked_word.is_stop) {
			continue;
		}
		if (checked_word.is_minus) {
			if (checked_word.word.empty()) {
				throw std::invalid_argument("Empty minus-word in query");
			}
			if (checked_word.word[0] == '-') {
				throw std::invalid_argument("More then 1 minus-character in minus-word in query");
			}
			query.minus_words.insert(checked_word.word);
		} else {
			query.plus_words.insert(checked_word.word);
		}
	}
	return query;
}

double SearchServer::CalcIdf(const std::string & word) const {
	return std::log(static_cast<double>(document_count_)
		/ static_cast<double>(documents_with_tf_.at(word).size()));
}

bool SearchServer::HasWordInDocument(const std::string & word, int document_id) const {
	return documents_with_tf_.count(word) != 0
		&& documents_with_tf_.at(word).count(document_id) != 0;
}

bool SearchServer::IsValidWord(const std::string & word) {
	// A valid word must not contain special characters
	return std::none_of(word.begin(), word.end(), [](char c) {
		return c >= '\0' && c < ' ';
	});
}

int SearchServer::ComputeAverageRating(const std::vector<int> & ratings) {
	if (ratings.empty()) {
		return 0;
	} else {
		int ratings_sum = std::accumulate(ratings.begin(), ratings.end(), 0);
		int average = ratings_sum / static_cast<int>(ratings.size());
		return average;
	}
}
