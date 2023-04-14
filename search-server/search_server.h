#pragma once

#include <string>
#include <vector>
#include <tuple>
#include <map>
#include <set>
#include <algorithm>
#include <stdexcept>
#include "document.h"
#include "string_processing.h"

inline constexpr int MAX_RESULT_DOCUMENT_COUNT = 5;
inline constexpr double EPSILON = 1e-6;

class SearchServer {
public:
	explicit SearchServer(const std::string & text = std::string(""));

	template <typename Container>
	explicit SearchServer(const Container & container);

	void AddDocument(int document_id, const std::string & document,
		DocumentStatus status, const std::vector<int> & ratings);

	// Возвращает топ-5 самых релевантных документов
	std::vector<Document> FindTopDocuments(const std::string & raw_query,
		DocumentStatus status = DocumentStatus::ACTUAL) const;

	template <typename Filter>
	std::vector<Document> FindTopDocuments(const std::string & raw_query, Filter filter) const;

	// Возвращает количество документов
	int GetDocumentCount() const;

	// Возвращает ID документа по очередности добавления
	int GetDocumentId(int index) const;

	// Возвращает статус документа и слова запроса, содержащиеся в документе с заданным ID
	std::tuple<std::vector<std::string>, DocumentStatus> MatchDocument(
		const std::string & raw_query, int document_id) const;

private:
	std::map<std::string, std::map<int, double>> documents_with_tf_; // слово - id док-та, tf
	std::set<std::string> stop_words_;
	int document_count_ = 0;

	struct DocumentInfo {
		int rating;
		DocumentStatus status;
	};
	std::map<int, DocumentInfo> document_info_;
	std::vector<int> adding_history_;

	struct Query {
		std::set<std::string> plus_words;
		std::set<std::string> minus_words;
	};

	struct QueryWord {
		std::string word;
		bool is_minus;
		bool is_stop;
	};

	bool IsStopWord(const std::string & word) const;

	std::vector<std::string> SplitIntoWordsNoStop(const std::string& text) const;

	// Проверяет, является ли слово стоп-словом и минус-словом
	QueryWord CheckWord(const std::string & word) const;

	// Разбивает строку на упорядоченный массив строк без повторений без стоп-слов
	Query ParseQuery(const std::string & text) const;

	template <typename Container>
	std::set<std::string> MakeStopWords(const Container & container);

	template <typename Filter>
	std::vector<Document> FindAllDocuments(const Query& query_words, Filter filter) const;

	double CalcIdf(const std::string & word) const;

	bool HasWordInDocument(const std::string & word, int document_id) const;

	template <typename WordsContainer>
	static bool IsValidAllWords(WordsContainer & words);

	static bool IsValidWord(const std::string & word);

	// Возвращаем среднее значение рейтинга
	static int ComputeAverageRating(const std::vector<int> & ratings);
};


template <typename Container>
SearchServer::SearchServer(const Container & container)
	: stop_words_(MakeStopWords(container)) {
}

template <typename Filter>
std::vector<Document> SearchServer::FindTopDocuments(const std::string & raw_query, Filter filter) const {
	const Query query_words = ParseQuery(raw_query);

	std::vector<Document> result = FindAllDocuments(query_words, filter);

	std::sort(result.begin(), result.end(),
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
std::set<std::string> SearchServer::MakeStopWords(const Container & container) {
	std::set<std::string> stop_words;
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
std::vector<Document> SearchServer::FindAllDocuments(const Query & query_words, Filter filter) const {
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
			document_info_.at(id).status,
			document_info_.at(id).rating))
		{
			int rating = document_info_.at(id).rating;
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

