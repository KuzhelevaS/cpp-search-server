#pragma once

#include <vector>
#include <string>
#include <deque>
#include "document.h"
#include "search_server.h"

class RequestQueue {
public:
	explicit RequestQueue(const SearchServer & search_server);

	// сделаем "обёртки" для всех методов поиска, чтобы сохранять результаты для нашей статистики
	template <typename DocumentPredicate>
	std::vector<Document> AddFindRequest(const std::string & raw_query, DocumentPredicate document_predicate);
	std::vector<Document> AddFindRequest(const std::string & raw_query, DocumentStatus status);
	std::vector<Document> AddFindRequest(const std::string & raw_query);
	
	int GetNoResultRequests() const;
private:
	struct QueryResult {
		bool is_empty;
	};
	std::deque<QueryResult> requests_;
	const static int min_in_day_ = 1440;
	const SearchServer & server_;

	void PushRequest(const std::vector<Document> & response);
};

template <typename DocumentPredicate>
std::vector<Document> RequestQueue::AddFindRequest(const std::string & raw_query,
	DocumentPredicate document_predicate)
{
	std::vector<Document> result = server_.FindTopDocuments(raw_query, document_predicate);
	PushRequest(result);
	return result;
}
