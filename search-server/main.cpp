#include <iostream>
#include <string>
#include <execution>
#include <random>

#include "search_server.h"
#include "paginator.h"
#include "request_queue.h"
#include "server_wrapers.h"
#include "remove_duplicates.h"
#include "process_queries.h"
#include "query_generator.h"
#include "time_meter.h"

#include "test_search_server.h"
#include "test_paginator.h"
#include "test_request_queue.h"
#include "test_remove_duplicates.h"

using std::literals::string_literals::operator""s;

int main() {
	// Тесты
	TestSearchServer();
	TestPaginator();
	TestRequestQueue();
	TestRemoveDuplicates();

	//Постраничная выдача
	{
		SearchServer search_server("and with"s);
		search_server.AddDocument(1, "funny pet and nasty rat"s, DocumentStatus::ACTUAL, {7, 2, 7});
		search_server.AddDocument(2, "funny pet with curly hair"s, DocumentStatus::ACTUAL, {1, 2, 3});
		search_server.AddDocument(3, "big cat nasty hair"s, DocumentStatus::ACTUAL, {1, 2, 8});
		search_server.AddDocument(4, "big dog cat Vladislav"s, DocumentStatus::ACTUAL, {1, 3, 2});
		search_server.AddDocument(5, "big dog hamster Borya"s, DocumentStatus::ACTUAL, {1, 1, 1});
		const auto search_results = search_server.FindTopDocuments("curly dog"s);
		size_t page_size = 2;
		const auto pages = Paginate(search_results, page_size);
		// Выводим найденные документы по страницам
		for (auto page = pages.begin(); page != pages.end(); ++page) {
		//for (auto page : pages) {
			std::cout << *page << std::endl;
			std::cout << "Page break"s << std::endl;
		}
	}
	// Очередь запросов
	{
		SearchServer search_server("and in at"s);
		RequestQueue request_queue(search_server);
		search_server.AddDocument(1, "curly cat curly tail"s, DocumentStatus::ACTUAL, {7, 2, 7});
		search_server.AddDocument(2, "curly dog and fancy collar"s, DocumentStatus::ACTUAL, {1, 2, 3});
		search_server.AddDocument(3, "big cat fancy collar "s, DocumentStatus::ACTUAL, {1, 2, 8});
		search_server.AddDocument(4, "big dog sparrow Eugene"s, DocumentStatus::ACTUAL, {1, 3, 2});
		search_server.AddDocument(5, "big dog sparrow Vasiliy"s, DocumentStatus::ACTUAL, {1, 1, 1});
		// 1439 запросов с нулевым результатом
		for (int i = 0; i < 1439; ++i) {
			request_queue.AddFindRequest("empty request"s);
		}
		// все еще 1439 запросов с нулевым результатом
		request_queue.AddFindRequest("curly dog"s);
		// новые сутки, первый запрос удален, 1438 запросов с нулевым результатом
		request_queue.AddFindRequest("big collar"s);
		// первый запрос удален, 1437 запросов с нулевым результатом
		request_queue.AddFindRequest("sparrow"s);
		std::cout << "Total empty requests: "s << request_queue.GetNoResultRequests() << std::endl;
	}

	// Логирование времени работы
	{
		SearchServer search_server("and with"s);
		search_server.AddDocument(1, "funny pet and nasty rat"s, DocumentStatus::ACTUAL, {7, 2, 7});
		search_server.AddDocument(2, "funny pet with curly hair"s, DocumentStatus::ACTUAL, {1, 2, 3});
		search_server.AddDocument(3, "big cat nasty hair"s, DocumentStatus::ACTUAL, {1, 2, 8});
		search_server.AddDocument(4, "big dog cat Vladislav"s, DocumentStatus::ACTUAL, {1, 3, 2});
		search_server.AddDocument(5, "big dog hamster Borya"s, DocumentStatus::ACTUAL, {1, 1, 1});

		MatchDocuments(search_server, "nasty dog"s, 1);
		FindTopDocuments(search_server, "curly dog"s);

	}
	// Перебор id документа
	{
		SearchServer search_server("and with"s);
		search_server.AddDocument(1, "funny pet and nasty rat"s, DocumentStatus::ACTUAL, {7, 2, 7});
		search_server.AddDocument(2, "funny pet with curly hair"s, DocumentStatus::ACTUAL, {1, 2, 3});
		search_server.AddDocument(3, "big cat nasty hair"s, DocumentStatus::ACTUAL, {1, 2, 8});
		search_server.AddDocument(6, "big dog cat Vladislav"s, DocumentStatus::ACTUAL, {1, 3, 2});
		search_server.AddDocument(8, "big dog hamster Borya"s, DocumentStatus::ACTUAL, {1, 1, 1});

		for (const int document_id : search_server) {
			std::cout << document_id << " ";
		}
		std::cout << std::endl;
	}
	//Удаление дубликатов
	{
		SearchServer search_server("and with"s);

		AddDocument(search_server, 1, "funny pet and nasty rat"s, DocumentStatus::ACTUAL, {7, 2, 7});
		AddDocument(search_server, 2, "funny pet with curly hair"s, DocumentStatus::ACTUAL, {1, 2});

		// дубликат документа 2, будет удалён
		AddDocument(search_server, 3, "funny pet with curly hair"s, DocumentStatus::ACTUAL, {1, 2});

		// отличие только в стоп-словах, считаем дубликатом
		AddDocument(search_server, 4, "funny pet and curly hair"s, DocumentStatus::ACTUAL, {1, 2});

		// множество слов такое же, считаем дубликатом документа 1
		AddDocument(search_server, 5, "funny funny pet and nasty nasty rat"s, DocumentStatus::ACTUAL, {1, 2});

		// добавились новые слова, дубликатом не является
		AddDocument(search_server, 6, "funny pet and not very nasty rat"s, DocumentStatus::ACTUAL, {1, 2});

		// множество слов такое же, как в id 6, несмотря на другой порядок, считаем дубликатом
		AddDocument(search_server, 7, "very nasty rat and not very funny pet"s, DocumentStatus::ACTUAL, {1, 2});

		// есть не все слова, не является дубликатом
		AddDocument(search_server, 8, "pet with rat and rat and rat"s, DocumentStatus::ACTUAL, {1, 2});

		// слова из разных документов, не является дубликатом
		AddDocument(search_server, 9, "nasty rat with curly hair"s, DocumentStatus::ACTUAL, {1, 2});

		std::cout << "Before duplicates removed: "s << search_server.GetDocumentCount() << std::endl;
		RemoveDuplicates(search_server);
		std::cout << "After duplicates removed: "s << search_server.GetDocumentCount() << std::endl;
	}

	// Делаем параллельные запросы к поисковой системе
	{
		SearchServer search_server("and with"s);
		int id = 0;
		for (
			const std::string& text : {
				"funny pet and nasty rat"s,
				"funny pet with curly hair"s,
				"funny pet and not very nasty rat"s,
				"pet with rat and rat and rat"s,
				"nasty rat with curly hair"s,
			}
		) {
			search_server.AddDocument(++id, text, DocumentStatus::ACTUAL, {1, 2});
		}
		const std::vector<std::string> queries = {
			"nasty rat -not"s,
			"not very funny nasty pet"s,
			"curly hair"s
		};
		id = 0;
		for (
			const auto& documents : ProcessQueries(search_server, queries)
		) {
			std::cout << documents.size() << " documents for query ["s << queries[id++] << "]"s << std::endl;
		}
	}

	// Делаем параллельные запросы к поисковой системе
	// возвращаем результат в "плоском" виде
	{
		SearchServer search_server("and with"s);
		int id = 0;
		for (
			const std::string& text : {
				"funny pet and nasty rat"s,
				"funny pet with curly hair"s,
				"funny pet and not very nasty rat"s,
				"pet with rat and rat and rat"s,
				"nasty rat with curly hair"s,
			}
		) {
			search_server.AddDocument(++id, text, DocumentStatus::ACTUAL, {1, 2});
		}
		const std::vector<std::string> queries = {
			"nasty rat -not"s,
			"not very funny nasty pet"s,
			"curly hair"s
		};
		for (const Document& document : ProcessQueriesJoined(search_server, queries)) {
			std::cout << "Document "s << document.id << " matched with relevance "s << document.relevance << std::endl;
		}
	}

	// Параллелим удаление документов
	{
		SearchServer search_server("and with"s);

		int id = 0;
		for (
			const std::string& text : {
				"funny pet and nasty rat"s,
				"funny pet with curly hair"s,
				"funny pet and not very nasty rat"s,
				"pet with rat and rat and rat"s,
				"nasty rat with curly hair"s,
			}
		) {
			search_server.AddDocument(++id, text, DocumentStatus::ACTUAL, {1, 2});
		}

		const std::string query = "curly and funny"s;

		auto report = [&search_server, &query] {
			std::cout << search_server.GetDocumentCount() << " documents total, "s
				<< search_server.FindTopDocuments(query).size() << " documents for query ["s << query << "]"s << std::endl;
		};

		report();
		// однопоточная версия
		search_server.RemoveDocument(5);
		report();
		// однопоточная версия
		search_server.RemoveDocument(std::execution::seq, 1);
		report();
		// многопоточная версия
		search_server.RemoveDocument(std::execution::par, 2);
		report();
	}

	// Параллелим поиск совпадений в документах
	{
		SearchServer search_server("and with"s);

		int id = 0;
		for (
			const std::string& text : {
				"funny pet and nasty rat"s,
				"funny pet with curly hair"s,
				"funny pet and not very nasty rat"s,
				"pet with rat and rat and rat"s,
				"nasty rat with curly hair"s,
			}
		) {
			search_server.AddDocument(++id, text, DocumentStatus::ACTUAL, {1, 2});
		}

		const std::string query = "curly and funny -not"s;

		{
			const auto [words, status] = search_server.MatchDocument(query, 1);
			std::cout << words.size() << " words for document 1"s << std::endl;
			// 1 words for document 1
		}

		{
			const auto [words, status] = search_server.MatchDocument(std::execution::seq, query, 2);
			std::cout << words.size() << " words for document 2"s << std::endl;
			// 2 words for document 2
		}

		{
			const auto [words, status] = search_server.MatchDocument(std::execution::par, query, 3);
			std::cout << words.size() << " words for document 3"s << std::endl;
			// 0 words for document 3
		}
	}

	// Параллелим поиск по документам
	{
		SearchServer search_server("and with"s);
		int id = 0;
		for (
			const std::string& text : {
				"white cat and yellow hat"s,
				"curly cat curly tail"s,
				"nasty dog with big eyes"s,
				"nasty pigeon john"s,
			}
		) {
			search_server.AddDocument(++id, text, DocumentStatus::ACTUAL, {1, 2});
		}
		std::cout << "ACTUAL by default:"s << std::endl;
		// последовательная версия
		for (const Document& document : search_server.FindTopDocuments("curly nasty cat"s)) {
			std::cout << document << std::endl;
		}
		std::cout << "BANNED:"s << std::endl;
		// последовательная версия
		for (const Document& document : search_server.FindTopDocuments(std::execution::seq, "curly nasty cat"s, DocumentStatus::BANNED)) {
			std::cout << document << std::endl;
		}
		std::cout << "Even ids:"s << std::endl;
		// параллельная версия
		for (const Document& document : search_server.FindTopDocuments(
			std::execution::par, "curly nasty cat"s,
			[](int document_id, [[maybe_unused]] DocumentStatus status, [[maybe_unused]]int rating) {
				return document_id % 2 == 0;
			}))
		{
			std::cout << document << std::endl;
		}
	}

	// Benchmarks
	{
		std::mt19937 generator;
		const auto dictionary = GenerateDictionary(generator, 2'000, 25);
		const auto documents = GenerateQueries(generator, dictionary, 20'000, 10);
		SearchServer search_server(dictionary[0]);
		for (size_t i = 0; i < documents.size(); ++i) {
			search_server.AddDocument(i, documents[i], DocumentStatus::ACTUAL, {1, 2, 3});
		}
		const auto queries = GenerateQueries(generator, dictionary, 2'000, 7);
		std::cerr << "ProcessQueries"s << std::endl;
		TEST_TIME_QUERIES_PROCESSOR(ProcessQueries);
	}
	{
		std::mt19937 generator;

		const auto dictionary = GenerateDictionary(generator, 10'000, 25);
		const auto documents = GenerateQueries(generator, dictionary, 10'000, 100);
		std::cerr << "RemoveDocument"s << std::endl;
		{
			SearchServer search_server(dictionary[0]);
			for (size_t i = 0; i < documents.size(); ++i) {
				search_server.AddDocument(i, documents[i], DocumentStatus::ACTUAL, {1, 2, 3});
			}

			TEST_MULTI_THREAD_REMOVING(seq);
		}
		{
			SearchServer search_server(dictionary[0]);
			for (size_t i = 0; i < documents.size(); ++i) {
				search_server.AddDocument(i, documents[i], DocumentStatus::ACTUAL, {1, 2, 3});
			}

			TEST_MULTI_THREAD_REMOVING(par);
		}
	}
	{
		std::mt19937 generator;

		const auto dictionary = GenerateDictionary(generator, 1000, 10);
		const auto documents = GenerateQueriesStrict(generator, dictionary, 10'000, 70);

		const std::string query = GenerateQueryStrict(generator, dictionary, 500, 0.1);

		SearchServer search_server(dictionary[0]);
		for (size_t i = 0; i < documents.size(); ++i) {
			search_server.AddDocument(i, documents[i], DocumentStatus::ACTUAL, {1, 2, 3});
		}
		std::cerr << "MatchDocument"s << std::endl;
		TEST_MULTI_THREAD_MATCHING(seq);
		TEST_MULTI_THREAD_MATCHING(par);
	}
	{
		std::mt19937 generator;
		const auto dictionary = GenerateDictionary(generator, 1000, 10);
		const auto documents = GenerateQueries(generator, dictionary, 10'000, 70);
		SearchServer search_server(dictionary[0]);
		for (size_t i = 0; i < documents.size(); ++i) {
			search_server.AddDocument(i, documents[i], DocumentStatus::ACTUAL, {1, 2, 3});
		}
		const auto queries = GenerateQueries(generator, dictionary, 100, 70);
		std::cerr << "FindTopDocuments"s << std::endl;
		TEST_MULTI_THREAD_FINDING(seq);
		TEST_MULTI_THREAD_FINDING(par);
	}
	return 0;
}

// Код должен вывести:
//Постраничная выдача
/*
{ document_id = 2, relevance = 0.402359, rating = 2 }{ document_id = 4, relevance = 0.229073, rating = 2 }
Page break
{ document_id = 5, relevance = 0.229073, rating = 1 }
Page break
*/

// Очередь запросов
/* Total empty requests: 1437 */

// Логирование времени работы
/*
{ document_id = 1, status = 0, words = nasty}
Operation time: 0 ms
Результаты поиска по запросу: curly dog
{ document_id = 2, relevance = 0.402359, rating = 2 }
{ document_id = 4, relevance = 0.229073, rating = 2 }
{ document_id = 5, relevance = 0.229073, rating = 1 }
Operation time: 0 ms
*/

// Перебор id документа
/* 1 2 3 6 8 */

//Удаление дубликатов
/*
Before duplicates removed: 9
Found duplicate document id 3
Found duplicate document id 4
Found duplicate document id 5
Found duplicate document id 7
After duplicates removed: 5
*/

// Делаем параллельные запросы к поисковой системе
/*
3 documents for query [nasty rat -not]
5 documents for query [not very funny nasty pet]
2 documents for query [curly hair]
*/

// Возвращаем результат в "плоском" виде
/*
Document 1 matched with relevance 0.183492
Document 5 matched with relevance 0.183492
Document 4 matched with relevance 0.167358
Document 3 matched with relevance 0.743945
Document 1 matched with relevance 0.311199
Document 2 matched with relevance 0.183492
Document 5 matched with relevance 0.127706
Document 4 matched with relevance 0.0557859
Document 2 matched with relevance 0.458145
Document 5 matched with relevance 0.458145
*/

// Параллелим удаление документов
/*
5 documents total, 4 documents for query [curly and funny]
4 documents total, 3 documents for query [curly and funny]
3 documents total, 2 documents for query [curly and funny]
2 documents total, 1 documents for query [curly and funny]
*/

// Параллелим поиск совпадений в документах
/*
1 words for document 1
2 words for document 2
0 words for document 3
*/

// Параллелим поиск по документам
/*
ACTUAL by default:
{ document_id = 2, relevance = 0.866434, rating = 1 }
{ document_id = 4, relevance = 0.231049, rating = 1 }
{ document_id = 1, relevance = 0.173287, rating = 1 }
{ document_id = 3, relevance = 0.173287, rating = 1 }
BANNED:
Even ids:
{ document_id = 2, relevance = 0.866434, rating = 1 }
{ document_id = 4, relevance = 0.231049, rating = 1 }
*/
