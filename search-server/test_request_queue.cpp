#include "test_request_queue.h"
#include "request_queue.h"
#include <vector>
#include <string>
#include "test_engine.h"

using std::literals::string_literals::operator""s;

// Проверяет, что все обертки над FindTopDocuments работают корректно
void TestRequestQueueAddingByAllMeans() {
	int doc_id_cat = 42;
	int doc_id_lion = 33;
	DocumentStatus status = DocumentStatus::ACTUAL;
	SearchServer server;
	RequestQueue request_queue(server);
	server.AddDocument(doc_id_cat, "cat in the city"s, status, {1, 2, 3});
	server.AddDocument(doc_id_lion, "the lion"s, DocumentStatus::BANNED, {1, 2, 3});

	// Нерелевантный запрос
	auto result_wrong = request_queue.AddFindRequest("dog"s);
	ASSERT(result_wrong.empty());

	// Запрос только по ключевым словам
	auto result_query = request_queue.AddFindRequest("cat"s);
	ASSERT_EQUAL(result_query.size(), 1u);
	ASSERT_EQUAL(result_query.at(0).id, doc_id_cat);

	// Запрос по ключевым словам и статусу
	auto result_query_status = request_queue.AddFindRequest("the"s, status);
	ASSERT_EQUAL(result_query_status.size(), 1u);
	ASSERT_EQUAL(result_query_status.at(0).id, doc_id_cat);

	// Запрос по ключевым словам и лямбда-функции
	auto result_query_predicate = request_queue.AddFindRequest("the"s,
		[](int document_id, DocumentStatus, int){
			return document_id % 2 == 1;
		});
	ASSERT_EQUAL(result_query_predicate.size(), 1u);
	ASSERT_EQUAL(result_query_predicate.at(0).id, doc_id_lion);
}

// Проверяет, что возвращается корректное количество пустых запросов
void TestRequestQueueCountEmptyRequests() {
	// Запросов меньше, чем максимальное количество за сутки
	{
		SearchServer server("and in at"s);
		RequestQueue request_queue(server);
		server.AddDocument(42, "cat in the city"s, DocumentStatus::ACTUAL, {1, 2, 3});
		// Нет пустых запросов если запросов еще не было сделано
		ASSERT_EQUAL_HINT(request_queue.GetNoResultRequests(), 0,
			"Must be empty if has no requests at all");

		// Сделали пустой запрос
		request_queue.AddFindRequest("empty request"s);
		ASSERT_EQUAL_HINT(request_queue.GetNoResultRequests(), 1,
			"Must increase if do empty request");

		// Сделали не пустой запрос
		request_queue.AddFindRequest("cat"s);
		ASSERT_EQUAL_HINT(request_queue.GetNoResultRequests(), 1,
			"Must not increase if do existing request");
	}

	// Запросов больше, чем максимальное количество за сутки
	{
		// Заполнили очередь пустыми запросами
		{
			SearchServer server("and in at"s);
			RequestQueue request_queue(server);
			server.AddDocument(42, "cat in the city"s, DocumentStatus::ACTUAL, {1, 2, 3});

			for (int i = 0; i <= 1439; ++i) {
				request_queue.AddFindRequest("empty request"s);
			}

			ASSERT_EQUAL_HINT(request_queue.GetNoResultRequests(), 1440,
				"Value must be 1440, if all requests by 1440 minutes (24 hours) was empty");
		}

		// Сделали пустой запрос при полной истории пустых запросов
		{
			SearchServer server("and in at"s);
			RequestQueue request_queue(server);
			server.AddDocument(42, "cat in the city"s, DocumentStatus::ACTUAL, {1, 2, 3});

			for (int i = 0; i <= 1439; ++i) {
				request_queue.AddFindRequest("empty request"s);
			}

			request_queue.AddFindRequest("empty request"s);
			ASSERT_EQUAL_HINT(request_queue.GetNoResultRequests(), 1440,
				"Must not may be more then 1440 (amount minutes in 24 hours)");
		}

		// Сделали не пустой запрос при полной истории пустых запросов
		{
			SearchServer server("and in at"s);
			RequestQueue request_queue(server);
			server.AddDocument(42, "cat in the city"s, DocumentStatus::ACTUAL, {1, 2, 3});

			for (int i = 0; i <= 1439; ++i) {
				request_queue.AddFindRequest("empty request"s);
			}

			request_queue.AddFindRequest("cat"s);
			ASSERT_EQUAL_HINT(request_queue.GetNoResultRequests(), 1439,
				"Must decrease if do existing request in full request queue");
		}

		// Сделали пустой запрос при 1 непустом запросе и полной истории пустых запросов
		{
			SearchServer server("and in at"s);
			RequestQueue request_queue(server);
			server.AddDocument(42, "cat in the city"s, DocumentStatus::ACTUAL, {1, 2, 3});

			for (int i = 0; i <= 1439; ++i) {
				request_queue.AddFindRequest("empty request"s);
			}
			request_queue.AddFindRequest("cat"s);

			request_queue.AddFindRequest("empty request"s);
			ASSERT_EQUAL_HINT(request_queue.GetNoResultRequests(), 1439,
				"Noempty reqest move by 1 position, must be as in the prev test");
		}

		// Заполнили очередь не пустыми запросами
		{
			SearchServer server("and in at"s);
			RequestQueue request_queue(server);
			server.AddDocument(42, "cat in the city"s, DocumentStatus::ACTUAL, {1, 2, 3});

			for (int i = 0; i <= 1439; ++i) {
				request_queue.AddFindRequest("cat"s);
			}

			ASSERT_EQUAL_HINT(request_queue.GetNoResultRequests(), 0,
				"After 1440 not empty requests result must be empty");
		}

		// Вытолкнули непустой запрос из очереди
		{
			SearchServer server("and in at"s);
			RequestQueue request_queue(server);
			server.AddDocument(42, "cat in the city"s, DocumentStatus::ACTUAL, {1, 2, 3});
			// Смоделировали ситуацию, где непустой запрос протолкнули на последнюю позицию
			request_queue.AddFindRequest("cat"s);
			for (int i = 0; i <= 1438; ++i) {
				request_queue.AddFindRequest("empty request"s);
			}

			// Убедились, что не пустой запрос все еще есть в очереди
			ASSERT_EQUAL_HINT(request_queue.GetNoResultRequests(), 1439,
				"Noempty request must be in a last position");

			// Убедились, что не пустой запрос вытолкнут
			request_queue.AddFindRequest("empty request"s);
			ASSERT_EQUAL_HINT(request_queue.GetNoResultRequests(), 1440,
				"Must be only empty requests, after adding empty request, if noempty was in the last position");
		}

		// Вытолкнули пустой запрос из очереди
		{
			SearchServer server("and in at"s);
			RequestQueue request_queue(server);
			server.AddDocument(42, "cat in the city"s, DocumentStatus::ACTUAL, {1, 2, 3});
			// Смоделировали ситуацию, где непустой запрос протолкнули на последнюю позицию
			request_queue.AddFindRequest("empty request"s);
			for (int i = 0; i <= 1438; ++i) {
				request_queue.AddFindRequest("cat"s);
			}

			// Убедились, что пустой запрос все еще есть в очереди
			ASSERT_EQUAL_HINT(request_queue.GetNoResultRequests(), 1,
				"Еmpty request must be in a last position");

			// Убедились, что пустой запрос вытолкнут
			request_queue.AddFindRequest("cat"s);
			ASSERT_EQUAL_HINT(request_queue.GetNoResultRequests(), 0,
				"Must be only noempty requests, after adding noempty request, if empty was in the last position");
		}
	}
}

void TestRequestQueue() {
	RUN_TEST(TestRequestQueueAddingByAllMeans);
	RUN_TEST(TestRequestQueueCountEmptyRequests);
}
