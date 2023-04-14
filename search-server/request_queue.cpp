#include "request_queue.h"

RequestQueue::RequestQueue(const SearchServer & search_server)
	: server_(search_server)
{}

std::vector<Document> RequestQueue::AddFindRequest(const std::string & raw_query, DocumentStatus status) {
	std::vector<Document> result = server_.FindTopDocuments(raw_query, status);
	PushRequest(result);
	return result;
}

std::vector<Document> RequestQueue::AddFindRequest(const std::string & raw_query) {
	std::vector<Document> result = server_.FindTopDocuments(raw_query);
	PushRequest(result);
	return result;
}

int RequestQueue::GetNoResultRequests() const {
	int result = std::count_if(requests_.begin(), requests_.end(), [](QueryResult request) {
		return request.is_empty == true;
	});
	return result;
}

void RequestQueue::PushRequest(const std::vector<Document> & response) {
	if (response.empty()) {
		requests_.push_back(QueryResult{true});
	} else {
		requests_.push_back(QueryResult{false});
	}
	if(requests_.size() > min_in_day_) {
		requests_.pop_front();
	}
}
