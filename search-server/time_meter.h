#pragma once

#include "process_queries.h"
#include "search_server.h"
#include <string>
#include <vector>
#include "log_duration.h"
#include <execution>

template <typename QueriesProcessor>
void TestQueriesProcessor(std::string_view mark, QueriesProcessor processor,
	const SearchServer& search_server, const std::vector<std::string>& queries)
{
	LOG_DURATION(mark);
	const auto documents_lists = processor(search_server, queries);
}
#define TEST_TIME_QUERIES_PROCESSOR(processor) TestQueriesProcessor(#processor, processor, search_server, queries)

template <typename ExecutionPolicy>
void TestMultiThreadRemoving(std::string_view mark,
	SearchServer search_server, ExecutionPolicy&& policy)
{
	LOG_DURATION(mark);
	const int document_count = search_server.GetDocumentCount();
	for (int id = 0; id < document_count; ++id) {
		search_server.RemoveDocument(policy, id);
	}
	std::cout << search_server.GetDocumentCount() << std::endl;
}

#define TEST_MULTI_THREAD_REMOVING(mode) TestMultiThreadRemoving(#mode, search_server, std::execution::mode)


template <typename ExecutionPolicy>
void TestMultiThreadMatching(std::string_view mark, SearchServer search_server,
	const std::string& query, ExecutionPolicy&& policy)
{
	LOG_DURATION(mark);
	const int document_count = search_server.GetDocumentCount();
	int word_count = 0;
	for (int id = 0; id < document_count; ++id) {
		const auto [words, status] = search_server.MatchDocument(policy, query, id);
		word_count += words.size();
	}
	std::cout << word_count << std::endl;
}

#define TEST_MULTI_THREAD_MATCHING(policy) TestMultiThreadMatching(#policy, search_server, query, std::execution::policy)

template <typename ExecutionPolicy>
void TestMultiThreadFinding(std::string_view mark, const SearchServer& search_server,
	const std::vector<std::string>& queries, ExecutionPolicy&& policy)
{
	LOG_DURATION(mark);
	double total_relevance = 0;
	for (const std::string_view query : queries) {
		for (const auto& document : search_server.FindTopDocuments(policy, query)) {
			total_relevance += document.relevance;
		}
	}
	std::cout << total_relevance << std::endl;
}
#define TEST_MULTI_THREAD_FINDING(policy) TestMultiThreadFinding(#policy, search_server, queries, std::execution::policy)
