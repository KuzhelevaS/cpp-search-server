#include "server_wrapers.h"
#include "log_duration.h"

using std::literals::string_literals::operator""s;

void MatchDocuments(const SearchServer & server, std::string query, int id) {
	LOG_DURATION_STREAM("Operation time"s, std::cout);
	auto [words, status] = server.MatchDocument(query, id);
	std::cout << "{ document_id = "s << id
		<< ", status = " << static_cast<int>(status)
		<< ", words ="s;
	for (auto word : words) {
		std::cout << ' ' << word;
	}
	std::cout <<"}" << std::endl;
}

void FindTopDocuments(const SearchServer & server, std::string query) {
	LOG_DURATION_STREAM("Operation time"s, std::cout);
	const auto search_results = server.FindTopDocuments(query);
	std::cout << "Результаты поиска по запросу: "s << query << std::endl;
	for (auto document : search_results) {
		std::cout << document << std::endl;
	}
}

void AddDocument(SearchServer & server, int document_id, const std::string & document,
	DocumentStatus status, const std::vector<int> & ratings)
{
	server.AddDocument(document_id, document, status, ratings);
}
