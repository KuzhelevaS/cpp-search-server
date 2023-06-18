#pragma once

#include <string>
#include <vector>
#include <tuple>
#include <map>
#include <set>
#include <algorithm>
#include <stdexcept>
#include <execution>
#include <deque>
#include "document.h"
#include "string_processing.h"
#include "concurrent_map.h"

inline constexpr int MAX_RESULT_DOCUMENT_COUNT = 5;
inline constexpr double EPSILON = 1e-6;

class SearchServer {
public:
	using MatchedDocuments = std::tuple<std::vector<std::string_view>, DocumentStatus>;

	explicit SearchServer(const std::string & text = std::string(""));
	explicit SearchServer(std::string_view text);

	template <typename Container>
	explicit SearchServer(const Container & container);

	void AddDocument(int document_id, std::string_view document,
		DocumentStatus status, const std::vector<int> & ratings);

	// Возвращает топ-5 самых релевантных документов
	std::vector<Document> FindTopDocuments(std::string_view raw_query,
		DocumentStatus status = DocumentStatus::ACTUAL) const;
	template <typename ExecutionPolicy>
	std::vector<Document> FindTopDocuments(const ExecutionPolicy & policy,
		std::string_view raw_query, DocumentStatus status = DocumentStatus::ACTUAL) const;

	template <typename Filter>
	std::vector<Document> FindTopDocuments(std::string_view raw_query, Filter filter) const;
	template <typename ExecutionPolicy, typename Filter>
	std::vector<Document> FindTopDocuments(const ExecutionPolicy & policy,
		std::string_view raw_query, Filter filter) const;

	// Возвращает статус документа и слова запроса, содержащиеся в документе с заданным ID
	MatchedDocuments MatchDocument(std::string_view raw_query, int document_id) const;
	MatchedDocuments MatchDocument(const std::execution::sequenced_policy & seq,
		std::string_view raw_query, int document_id) const;
	MatchedDocuments MatchDocument(const std::execution::parallel_policy & par,
		std::string_view raw_query, int document_id) const;

	// Возвращает количество документов
	int GetDocumentCount() const;

	// Итераторы для перебора id документов
	std::set<int>::const_iterator begin() const;
	std::set<int>::const_iterator end() const;

	const std::map<std::string_view, double> & GetWordFrequencies(int document_id) const;

	void RemoveDocument(int document_id);
	void RemoveDocument(const std::execution::sequenced_policy & seq, int document_id);
	void RemoveDocument(const std::execution::parallel_policy & par, int document_id);


private:
	std::deque<std::string> storage_;
	std::set<std::string, std::less<>> stop_words_;

	struct DocumentInfo {
		int rating;
		DocumentStatus status;
		std::map<std::string_view, double> words;
	};
	std::map<int, DocumentInfo> documents_info_;
	std::set<int> documents_id_;
	std::map<std::string_view, std::map<int, double>> documents_with_tf_; // слово - id док-та, tf

	struct Query {
		std::vector<std::string_view> plus_words;
		std::vector<std::string_view> minus_words;
	};

	struct QueryWord {
		std::string_view word;
		bool is_minus;
		bool is_stop;
	};

	bool IsStopWord(std::string_view word) const;

	std::vector<std::string_view> SplitIntoWordsNoStop(std::string_view text) const;

	// Проверяет, является ли слово стоп-словом и минус-словом
	QueryWord CheckWord(std::string_view word) const;

	// Разбивает строку на упорядоченный массив строк без повторений без стоп-слов
	Query ParseQuery(std::string_view text, bool needSortAndUnique = true) const;

	template <typename Container>
	std::set<std::string, std::less<>> MakeStopWords(const Container & container);

	template <typename Filter>
	std::vector<Document> FindAllDocuments(const std::execution::sequenced_policy & seq,
		const Query& query_words, Filter filter) const;
	template <typename Filter>
	std::vector<Document> FindAllDocuments(const std::execution::parallel_policy & par,
		const Query& query_words, Filter filter) const;

	double CalcIdf(std::string_view word) const;

	bool HasWordInDocument(std::string_view word, int document_id) const;

	template <typename WordsContainer>
	static bool IsValidAllWords(WordsContainer & words);

	static bool IsValidWord(std::string_view word);

	// Возвращаем среднее значение рейтинга
	static int ComputeAverageRating(const std::vector<int> & ratings);
};


template <typename Container>
SearchServer::SearchServer(const Container & container)
	: stop_words_(MakeStopWords(container)) {
}

template <typename ExecutionPolicy>
std::vector<Document> SearchServer::FindTopDocuments(const ExecutionPolicy & policy,
	std::string_view raw_query, DocumentStatus status) const
{
	return FindTopDocuments(policy, raw_query,
		[status](int, DocumentStatus document_status, int) {
			return document_status == status;
		});
}

template <typename Filter>
std::vector<Document> SearchServer::FindTopDocuments(std::string_view raw_query, Filter filter) const {
	return FindTopDocuments(std::execution::seq , raw_query, filter);
}

template <typename ExecutionPolicy, typename Filter>
std::vector<Document> SearchServer::FindTopDocuments(const ExecutionPolicy & policy,
	std::string_view raw_query, Filter filter) const
{
	Query query_words = ParseQuery(raw_query, false);
	SortAndRemoveDuplicates(policy, query_words.plus_words);
	SortAndRemoveDuplicates(policy, query_words.minus_words);

	std::vector<Document> result = FindAllDocuments(policy, query_words, filter);

	std::sort(policy, result.begin(), result.end(),
		[](const Document & lhs, const Document & rhs) {
			if (std::abs(lhs.relevance - rhs.relevance) < EPSILON) {
				return lhs.rating > rhs.rating;
			}
			return lhs.relevance > rhs.relevance;
		});

	if (result.size() > MAX_RESULT_DOCUMENT_COUNT) {
		result.resize(MAX_RESULT_DOCUMENT_COUNT);
	}
	return result;
}

template <typename Container>
std::set<std::string, std::less<>> SearchServer::MakeStopWords(const Container & container) {
	std::set<std::string, std::less<>> stop_words;
	for (const std::string & word : container) {
		if (!IsValidWord(word)) {
			std::string message = std::string("Stop word \"") + word
				+ std::string("\" contain special characters");
			throw std::invalid_argument(message);
		}
		if (!word.empty()) {
			stop_words.insert(word);
		}
	}
	return stop_words;
}

template <typename Filter>
std::vector<Document> SearchServer::FindAllDocuments(
	[[maybe_unused]] const std::execution::sequenced_policy & seq,
	const Query & query_words, Filter filter) const
{
	std::map<int, double> matched_documents;
	for(const auto & plus : query_words.plus_words) {
		// нужно, т.к. дальше вызываем documents_with_tf_.at()
		// также предупреждаем деление на 0
		if (documents_with_tf_.count(plus) == 0) {
			continue;
		}
		double idf = CalcIdf(plus);
		for (const auto & [doc_id, tf] : documents_with_tf_.at(plus)) {
			matched_documents[doc_id] += idf * tf;
		}
	}
	for(const auto & minus : query_words.minus_words) {
		// нужно, т.к. дальше вызываем documents_with_tf_.at()
		// также предупреждаем деление на 0
		if (documents_with_tf_.count(minus) == 0) {
			continue;
		}
		for (const auto & [doc_id, tf] : documents_with_tf_.at(minus)) {
			matched_documents.erase(doc_id);
		}
	}
	std::vector<Document> result;
	for (const auto & [id, rel] : matched_documents) {
		if (filter(id,
			documents_info_.at(id).status,
			documents_info_.at(id).rating))
		{
			int rating = documents_info_.at(id).rating;
			result.push_back({id, rel, rating});
		}
	}
	return result;
}

template <typename Filter>
std::vector<Document> SearchServer::FindAllDocuments(const std::execution::parallel_policy & par,
	const Query& query_words, Filter filter) const
{
	ConcurrentMap<int, double> concurrent_matched_documents(100);
	std::for_each(par, query_words.plus_words.begin(), query_words.plus_words.end(),
		[&](auto plus) {
			// нужно, т.к. дальше вызываем documents_with_tf_.at()
			// также предупреждаем деление на 0
			if (documents_with_tf_.count(plus) == 0) {
				return;
			}
			double idf = CalcIdf(plus);
			for (const auto & [doc_id, tf] : documents_with_tf_.at(plus)) {
				concurrent_matched_documents[doc_id].ref_to_value += idf * tf;
			}
		});
	std::for_each(par, query_words.minus_words.begin(), query_words.minus_words.end(),
		[&](auto minus) {
			// нужно, т.к. дальше вызываем documents_with_tf_.at()
			// также предупреждаем деление на 0
			if (documents_with_tf_.count(minus) == 0) {
				return;
			}
			for (const auto & [doc_id, tf] : documents_with_tf_.at(minus)) {
				concurrent_matched_documents.Erase(doc_id);
			}
		});
	std::map<int, double> matched_documents = concurrent_matched_documents.BuildOrdinaryMap();
	std::vector<Document> result;
	for (const auto & [id, rel] : matched_documents) {
		if (filter(id,
			documents_info_.at(id).status,
			documents_info_.at(id).rating))
		{
			int rating = documents_info_.at(id).rating;
			result.push_back({id, rel, rating});
		}
	}
	return result;
}

template <typename WordsContainer>
bool SearchServer::IsValidAllWords(WordsContainer & words) {
	for (const auto & word : words) {
		if (!IsValidWord(word)) {
			return false;
		}
	}
	return true;
}
