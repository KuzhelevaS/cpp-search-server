#include <iostream>
#include <string>

#include "search_server.h"
#include "paginator.h"
#include "request_queue.h"
#include "server_wrapers.h"
#include "remove_duplicates.h"

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
	return 0;
}

// Код должен вывести:
/*
{ document_id = 2, relevance = 0.402359, rating = 2 }{ document_id = 4, relevance = 0.229073, rating = 2 }
Page break
{ document_id = 5, relevance = 0.229073, rating = 1 }
Page break
*/
/* Total empty requests: 1437 */
/* 
{ document_id = 1, status = 0, words = nasty}
Operation time: 0 ms
Результаты поиска по запросу: curly dog
{ document_id = 2, relevance = 0.402359, rating = 2 }
{ document_id = 4, relevance = 0.229073, rating = 2 }
{ document_id = 5, relevance = 0.229073, rating = 1 }
Operation time: 0 ms
*/
/* 1 2 3 6 8 */
/*
Before duplicates removed: 9
Found duplicate document id 3
Found duplicate document id 4
Found duplicate document id 5
Found duplicate document id 7
After duplicates removed: 5
*/
