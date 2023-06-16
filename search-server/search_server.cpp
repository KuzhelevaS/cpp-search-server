#include "search_server.h"
#include "concurrent_map.h"
#include <numeric>
#include <cmath>
#include <algorithm>
#include <mutex>

using std::literals::string_literals::operator""s;

SearchServer::SearchServer(const std::string & text)
	: SearchServer(SplitIntoWords(text))
{}

SearchServer::SearchServer(std::string_view text)
	: SearchServer(static_cast<std::string>(text))
{}

void SearchServer::AddDocument(int document_id, std::string_view document,
	DocumentStatus status, const std::vector<int> & ratings)
{
	if (document_id < 0) {
		throw std::invalid_argument("Id less then zero");
	}
	if (documents_info_.count(document_id)) {
		throw std::invalid_argument("Document with this id already exists");
	}

	storage.emplace_back(document);
	const std::vector<std::string_view> words = SplitIntoWordsNoStop(storage.back());
	if (!IsValidAllWords(words)) {
		throw std::invalid_argument("Document contain special characters");
	}
	documents_id_.insert(document_id);
	documents_info_[document_id] = {ComputeAverageRating(ratings), status, {}};
	double tf_coeff = 1.0 / static_cast<double>(words.size());
	for(const auto & word : words) {
		documents_with_tf_[word][document_id] += tf_coeff;
		documents_info_[document_id].words[word] += tf_coeff;
	}
}

std::vector<Document> SearchServer::FindTopDocuments(std::string_view raw_query,
	DocumentStatus status) const
{
	return FindTopDocuments(raw_query,
		[status](int, DocumentStatus document_status, int) {
			return document_status == status;
		});
}

std::tuple<std::vector<std::string_view>, DocumentStatus> SearchServer::MatchDocument(
	std::string_view raw_query, int document_id) const
{
	DocumentStatus result_status = documents_info_.at(document_id).status;

	const Query query_words = ParseQuery(raw_query);

	bool need_check_plus_words = true;
	for (const std::string_view minus_word : query_words.minus_words) {
		if (HasWordInDocument(minus_word, document_id)) {
			need_check_plus_words = false;
			break;
		}
	}

	std::vector<std::string_view> matched_words;
	if (need_check_plus_words) {
		for (const std::string_view plus_word : query_words.plus_words) {
			if (HasWordInDocument(plus_word, document_id)) {
				matched_words.push_back((*documents_info_.at(document_id).words.find(plus_word)).first);
			}
		}
		std::sort(matched_words.begin(), matched_words.end());
	}

	return std::tie(matched_words, result_status);
}

std::tuple<std::vector<std::string_view>, DocumentStatus> SearchServer::MatchDocument(
	[[maybe_unused]] const std::execution::sequenced_policy & seq,
	std::string_view raw_query, int document_id) const
{
	return SearchServer::MatchDocument(raw_query, document_id);
}

std::tuple<std::vector<std::string_view>, DocumentStatus> SearchServer::MatchDocument(
	const std::execution::parallel_policy & par,
	std::string_view raw_query, int document_id) const
{
	DocumentStatus result_status = documents_info_.at(document_id).status;
	Query query_words = ParseQuery(raw_query, false);

	bool need_check_plus_words = std::none_of(par,
		query_words.minus_words.begin(), query_words.minus_words.end(),
		[&](auto & minus_word){
			return HasWordInDocument(minus_word, document_id);
		});

	std::vector<std::string_view> matched_words;
	if (need_check_plus_words) {
		matched_words.resize(query_words.plus_words.size());
		auto last = std::copy_if(par,
			query_words.plus_words.begin(), query_words.plus_words.end(),
			matched_words.begin(),
			[&](auto plus_word){
				return HasWordInDocument(plus_word, document_id);
			});
		matched_words.erase(last, matched_words.end());
		std::transform(matched_words.begin(), matched_words.end(), matched_words.begin(),
			[&](std::string_view word){
				return (*documents_info_.at(document_id).words.find(word)).first;
			});
        SortAndRemoveDuplicates(std::execution::par, matched_words);
	}

	return std::tie(matched_words, result_status);
}

int SearchServer::GetDocumentCount() const {
	return static_cast<int>(documents_info_.size());
}

std::set<int>::const_iterator SearchServer::begin() const {
	return documents_id_.begin();
}

std::set<int>::const_iterator SearchServer::end() const {
	return documents_id_.end();
}

const std::map<std::string_view, double> & SearchServer::GetWordFrequencies(int document_id) const {
	static const std::map<std::string_view, double> empty_words;
	if (!documents_info_.count(document_id)) {
		return empty_words;
	}
	return documents_info_.at(document_id).words;
}

void SearchServer::RemoveDocument(int document_id) {
	documents_id_.erase(document_id);
	if (documents_info_.count(document_id)) {
		std::vector<std::string_view> words_without_document;
		for (auto [word, tf] : documents_info_.at(document_id).words) {
			documents_with_tf_.at(word).erase(document_id);
			if (documents_with_tf_.at(word).empty()) {
				words_without_document.push_back(word);
			}
		}
		for (auto word : words_without_document) {
			documents_with_tf_.erase(word);
		}
		documents_info_.erase(document_id);
	}
}

void SearchServer::RemoveDocument([[maybe_unused]] const std::execution::sequenced_policy & seq, int document_id) {
	RemoveDocument(document_id);
}

void SearchServer::RemoveDocument(const std::execution::parallel_policy & par, int document_id) {
	documents_id_.erase(document_id);
	if (documents_info_.count(document_id)) {
		std::vector<std::string_view> words_in_document(documents_info_.at(document_id).words.size());
		std::transform(par,
			documents_info_.at(document_id).words.begin(),
			documents_info_.at(document_id).words.end(),
			words_in_document.begin(),
			[](const auto word_with_tf){
				return word_with_tf.first;
			});
		std::vector<std::string_view> words_without_document;
		words_without_document.reserve(words_in_document.size());
		std::mutex empty_word_mutex;
		std::for_each(par, words_in_document.begin(), words_in_document.end(),
			[&](std::string_view word){
				documents_with_tf_.at(word).erase(document_id);
				if (documents_with_tf_.at(word).empty()) {
					std::lock_guard guard(empty_word_mutex);
					words_without_document.push_back(word);
				}
			});
		for (auto word : words_without_document) {
			documents_with_tf_.erase(word);
		}
		documents_info_.erase(document_id);
	}
}

bool SearchServer::IsStopWord(std::string_view word) const {
	return stop_words_.count(word) != 0;
}

std::vector<std::string_view> SearchServer::SplitIntoWordsNoStop(std::string_view text) const {
	std::vector<std::string_view> words;
	for (const std::string_view word : SplitIntoWordsView(text)) {
		if (!IsStopWord(word)) {
			words.push_back(word);
		}
	}
	return words;
}

SearchServer::QueryWord SearchServer::CheckWord(std::string_view word) const {
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

SearchServer::Query SearchServer::ParseQuery(std::string_view text, bool needSortAndUnique) const {
	Query query;
	for (std::string_view word : SplitIntoWordsView(text)) {
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
			query.minus_words.push_back(checked_word.word);
		} else {
			query.plus_words.push_back(checked_word.word);
		}
	}
	if (needSortAndUnique) {
		SortAndRemoveDuplicates(std::execution::seq, query.plus_words);
		SortAndRemoveDuplicates(std::execution::seq, query.minus_words);
	}
	return query;
}

double SearchServer::CalcIdf(std::string_view word) const {
	return std::log(static_cast<double>(documents_info_.size())
		/ static_cast<double>(documents_with_tf_.at(word).size()));
}

bool SearchServer::HasWordInDocument(std::string_view word, int document_id) const {
	return documents_with_tf_.count(word) != 0
		&& documents_with_tf_.at(word).count(document_id) != 0;
}

bool SearchServer::IsValidWord(std::string_view word) {
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
