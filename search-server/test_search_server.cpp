#include "test_engine.h"
#include "search_server.h"
#include <string>
#include <vector>
#include <cmath>

template <typename ExecutionPolicy>
std::string PolicyToString([[maybe_unused]]const ExecutionPolicy & policy) {
	if constexpr (std::is_same_v<std::decay_t<ExecutionPolicy>, std::execution::parallel_policy>) {
		return "parallel version";
	} else {
		return "sequenced version";
	}
}

// Тест проверяет, что поисковая система исключает стоп-слова при добавлении документов
void TestExcludeStopWordsFromAddedDocumentContent() {
	const int doc_id = 42;
	const std::string content = "cat in the city"s;
	const std::vector<int> ratings = {1, 2, 3};
	// Проверяем, что запрос с несуществующими данными возвращает пустой результат
	{
		SearchServer server;
		server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
		ASSERT_HINT(server.FindTopDocuments("lion"s).empty(),
			"Result must be empty, if words are not found"s);
	}

	// Сначала убеждаемся, что поиск слова, не входящего в список стоп-слов,
	// находит нужный документ
	{
		SearchServer server;
		server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
		const auto found_docs = server.FindTopDocuments("in"s);
		ASSERT_EQUAL(found_docs.size(), 1u);
		const Document& doc0 = found_docs.at(0);
		ASSERT_EQUAL(doc0.id, doc_id);
	}

	// Затем убеждаемся, что поиск этого же слова, входящего в список стоп-слов,
	// возвращает пустой результат если стоп-слова инициализированы строкой
	{
		SearchServer server("in the"s);
		server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
		ASSERT_HINT(server.FindTopDocuments("in"s).empty(),
			"Stop words must be excluded from documents"s);
	}

	// Затем убеждаемся, что поиск этого же слова, входящего в список стоп-слов,
	// возвращает пустой результат если стоп-слова инициализированы через vector
	{
		const std::vector<std::string> stop_words_vector = {"in"s, "in"s, "the"s, ""s};
		SearchServer server(stop_words_vector);
		server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
		ASSERT_HINT(server.FindTopDocuments("in"s).empty(),
			"Stop words must be excluded from documents"s);
	}

	// Затем убеждаемся, что поиск этого же слова, входящего в список стоп-слов,
	// возвращает пустой результат если стоп-слова инициализированы через set
	{
		const std::set<std::string> stop_words_vector = {"in"s, "the"s, ""s};
		SearchServer server(stop_words_vector);
		server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
		ASSERT_HINT(server.FindTopDocuments("in"s).empty(),
			"Stop words must be excluded from documents"s);
	}
}

// Тест проверяет, что результаты поиска не включают документы с минус-словами
void TestExcludeDocumentsWithMinusWordsFromTopDocuments() {
	const int doc_id = 42;
	const std::string content = "cat in the city"s;
	const std::vector<int> ratings = {1, 2, 3};
	//Проверяем ситуацию если минус-слова не входят в список стоп-слов
	{
		SearchServer server;
		server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
		ASSERT_HINT(server.FindTopDocuments("cat -in"s).empty(),
			"Result must be empty, if minus word is not a stop word"s);
	}

	//Проверяем ситуацию если минус-слова включают стоп-слов
	{
		SearchServer server("in the"s);
		server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
		const auto found_docs = server.FindTopDocuments("cat -in"s);
		ASSERT_EQUAL_HINT(found_docs.size(), 1u,
			"If minus words is a stop word, it must be ignore in query"s);
		ASSERT_EQUAL(found_docs.at(0).id, doc_id);
	}
}

// Тест проверяет, соответствие документа поисковому запросу
template <typename ExecutionPolicy>
void TestMatchingDocumentsForQueryPolicy(const ExecutionPolicy & policy) {
	std::string policy_str = PolicyToString(policy);

	// При матчинге документа по поисковому запросу
	// должны быть возвращены все слова из поискового запроса, присутствующие в документе.
	const int doc_id = 42;
	const std::string content = "cat in the city"s;
	const std::vector<int> ratings = {1, 2, 3};
	const DocumentStatus status = DocumentStatus::ACTUAL;
	{
		SearchServer server;
		server.AddDocument(doc_id, content, status, ratings);
		const auto [result_matched, result_status] = server.MatchDocument(policy, "the cat"s, doc_id);
		ASSERT_EQUAL_HINT(StatusAsString(result_status), StatusAsString(status), policy_str);
		std::vector<std::string_view> waiting_words {"cat", "the"};
		ASSERT_EQUAL_HINT(result_matched, waiting_words, policy_str);
	}
	// Если есть соответствие хотя бы по одному минус-слову, должен возвращаться пустой список слов.
	{
		SearchServer server;
		server.AddDocument(doc_id, content, status, ratings);
		const auto [result_matched, result_status] = server.MatchDocument("the cat -in"s, doc_id);
		ASSERT_EQUAL_HINT(StatusAsString(result_status), StatusAsString(status), policy_str);
		ASSERT_HINT(result_matched.empty(),
			("Document with minus-words must not be found "s + policy_str));
	}

	//Поиск по стоп-слову должен возвращать пустой результат
	{
		SearchServer server("in the"s);
		server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
		const auto [result_matched, result_status] = server.MatchDocument(policy, "in"s, doc_id);
		ASSERT_HINT(result_matched.empty(),
			("Don't matching by stop-words "s + policy_str));
	}

	//Если если минус-слово входит в список стоп-слов, оно должно быть просто проигнорировано
	{
		SearchServer server("in the"s);
		server.AddDocument(doc_id, content, status, ratings);
		const auto [result_matched, result_status] = server.MatchDocument(policy, "-the cat"s, doc_id);
		ASSERT_EQUAL_HINT(StatusAsString(result_status), StatusAsString(status), policy_str);
		std::vector<std::string_view> waiting_words {"cat"};
		ASSERT_EQUAL_HINT(result_matched, waiting_words, policy_str);
	}

	//Проверяем, что функция корректно отрабатывает ошибочные входные данные
	{
		SearchServer server;
		server.AddDocument(doc_id, content, status, ratings);

		// В корректном запросе отсутствуют исключения
		bool has_exception_if_match_correct_query = false;
		try {
			server.MatchDocument(policy, "-the cat"s, doc_id);
		} catch (...) {
			has_exception_if_match_correct_query = true;
		}
		ASSERT_HINT(!has_exception_if_match_correct_query, policy_str);

		// Проверяем исключение, если минус-слово содержит 2 минуса подряд
		bool has_invalid_argument_exception_if_match_two_minus = false;
		try {
			server.MatchDocument(policy, "--the cat"s, doc_id);
		} catch (const std::invalid_argument & exception) {
			has_invalid_argument_exception_if_match_two_minus = true;
		} catch (...) {
		}
		ASSERT_HINT(has_invalid_argument_exception_if_match_two_minus, policy_str);

		// Проверяем исключение, если в запросе пустое минус-слово
		bool has_invalid_argument_exception_if_match_empty_minus = false;
		try {
			server.MatchDocument(policy, "black -"s, doc_id);
		} catch (const std::invalid_argument & exception) {
			has_invalid_argument_exception_if_match_empty_minus = true;
		} catch (...) {
		}
		ASSERT_HINT(has_invalid_argument_exception_if_match_empty_minus, policy_str);

		// Проверяем исключение, если в запросе слово со спец.символами
		bool has_invalid_argument_exception_if_match_special_characters = false;
		try {
			server.MatchDocument(policy, "do\x12g"s, doc_id);
		} catch (const std::invalid_argument & exception) {
			has_invalid_argument_exception_if_match_special_characters = true;
		} catch (...) {
		}
		ASSERT_HINT(has_invalid_argument_exception_if_match_special_characters, policy_str);

		// Проверяем исключение, если получили запрос с несуществующим id
		bool has_out_of_range_exception_if_document_id_not_exist = false;
		try {
			server.MatchDocument(policy, content, doc_id * 2);
		} catch (const std::out_of_range & exception) {
			has_out_of_range_exception_if_document_id_not_exist = true;
		} catch (...) {
		}
		ASSERT_HINT(has_out_of_range_exception_if_document_id_not_exist, policy_str);
	}
}

void TestMatchingDocumentsForQuery() {
	TestMatchingDocumentsForQueryPolicy(std::execution::seq);
	TestMatchingDocumentsForQueryPolicy(std::execution::par);
}

// Тест проверяет сортировку по релевантности
void TestSortingOfSearchingResultByRelevance() {
	// Если у всех документов разная релевантность
	{
		SearchServer search_server;
		search_server.AddDocument(0, "white cat black hat"s, DocumentStatus::ACTUAL, {8, -3});
		search_server.AddDocument(1, "black dog green eyes"s, DocumentStatus::ACTUAL, {7, 2, 7});
		search_server.AddDocument(2, "black cat with name cat"s, DocumentStatus::ACTUAL, {5, -12, 2, 1});
		search_server.AddDocument(3, "lion"s, DocumentStatus::ACTUAL, {5, -12, 2, 1}); // tf-idf = 0

		std::vector<Document> result_documents = search_server.FindTopDocuments("black cat"s);
		ASSERT_EQUAL(result_documents.size(), 3u);
		ASSERT(result_documents.at(0).relevance > result_documents.at(1).relevance);
		ASSERT(result_documents.at(1).relevance > result_documents.at(2).relevance);
	}
	// Если одинаковая релевантность, сортируем по рейтингу
	{
		SearchServer search_server;
		search_server.AddDocument(0, "white cat with hat"s, DocumentStatus::ACTUAL, {8, -3});
		search_server.AddDocument(1, "black dog green eyes"s, DocumentStatus::ACTUAL, {7, 2, 7});
		search_server.AddDocument(3, "lion"s, DocumentStatus::ACTUAL, {5, -12, 2, 1}); // tf-idf = 0

		const auto result_documents = search_server.FindTopDocuments("black cat"s);
		ASSERT_EQUAL(result_documents.size(), 2u);
		ASSERT(std::abs(result_documents.at(0).relevance - result_documents.at(1).relevance) < EPSILON);
		ASSERT(result_documents.at(0).rating > result_documents.at(1).rating);
	}
}

//Тест проверяет расчет рейтинга как среднее арифметическое
void TestСalculationDocumentRating() {
	// Все числа положительные
	{
		SearchServer server;
		server.AddDocument(0, "dog"s, DocumentStatus::ACTUAL, {7, 2});
		ASSERT_EQUAL(server.FindTopDocuments("dog"s).at(0).rating, (7 + 2) / 2);
	}
	// Все числа отрицательные
	{
		SearchServer server;
		server.AddDocument(0, "dog"s, DocumentStatus::ACTUAL, {-7, -2});
		ASSERT_EQUAL(server.FindTopDocuments("dog"s).at(0).rating, (-7 + -2) / 2);
	}
	// Есть и положительные, и отрицательные числа
	{
		SearchServer server;
		server.AddDocument(0, "dog"s, DocumentStatus::ACTUAL, {-7, 2});
		ASSERT_EQUAL(server.FindTopDocuments("dog"s).at(0).rating, (-7 + 2) / 2);
	}
}

//Фильтрация результатов поиска с использованием предиката, задаваемого пользователем.
template <typename ExecutionPolicy>
void TestPredicateInFindTopDocumentPolicy(const ExecutionPolicy & policy) {
	std::string policy_str = PolicyToString(policy);

	const int doc_id_1 = 42;
	const int doc_id_2 = 33;
	const std::string content_1 = "cat in the city"s;
	const std::string content_2 = "cat in the city"s;
	const std::vector<int> ratings_1 = {1, 2, 3};
	const std::vector<int> ratings_2 = {5, 2, 3};
	const DocumentStatus status_1 = DocumentStatus::ACTUAL;
	const DocumentStatus status_2 = DocumentStatus::BANNED;
	// тестируем параметр id
	{
		SearchServer server;
		server.AddDocument(doc_id_1, content_1, status_1, ratings_1);
		server.AddDocument(doc_id_2, content_2, status_2, ratings_2);
		auto predicate_test_id = server.FindTopDocuments(policy, "cat"s,
			[](int document_id, DocumentStatus, int) {
				return document_id % 2 == 0;
			});
		ASSERT_EQUAL_HINT(predicate_test_id.size(), 1u, policy_str);
		ASSERT_EQUAL_HINT(predicate_test_id.at(0).id, 42, policy_str);
	}
	// тестируем параметр status
	{
		SearchServer server;
		server.AddDocument(doc_id_1, content_1, status_1, ratings_1);
		server.AddDocument(doc_id_2, content_2, status_2, ratings_2);
		auto predicate_test_status = server.FindTopDocuments(policy, "cat"s,
			[](int, DocumentStatus status, int ) {
				return status == DocumentStatus::BANNED;
			});
		ASSERT_EQUAL_HINT(predicate_test_status.size(), 1u, policy_str);
		ASSERT_EQUAL_HINT(predicate_test_status.at(0).id, 33, policy_str);
	}
	// тестируем параметр rating
	{
		SearchServer server;
		server.AddDocument(doc_id_1, content_1, status_1, ratings_1);
		server.AddDocument(doc_id_2, content_2, status_2, ratings_2);
		auto predicate_test_rating = server.FindTopDocuments(policy, "cat"s,
			[](int, DocumentStatus, int rating) {
				return rating >= 2;
			});
		ASSERT_EQUAL_HINT(predicate_test_rating.size(), 2u, policy_str);
		ASSERT_EQUAL_HINT(predicate_test_rating.at(0).id, 33, policy_str);
		ASSERT_EQUAL_HINT(predicate_test_rating.at(1).id, 42, policy_str);
	}
}

void TestPredicateInFindTopDocument() {
	TestPredicateInFindTopDocumentPolicy(std::execution::seq);
	TestPredicateInFindTopDocumentPolicy(std::execution::par);
}

//Поиск документов, имеющих заданный статус.
template <typename ExecutionPolicy>
void TestFindingDocumentsByStatusPolicy(const ExecutionPolicy & policy) {
	std::string policy_str = PolicyToString(policy);
	// ситуация, когда находим документ по статусу
	{
		SearchServer server;
		server.AddDocument(5, "dog"s, DocumentStatus::BANNED, {7, 2, 7});
		auto result_by_status = server.FindTopDocuments(policy, "dog"s, DocumentStatus::BANNED);
		ASSERT_EQUAL_HINT(result_by_status.size(), 1u, policy_str);
		ASSERT_EQUAL_HINT(result_by_status.at(0).id, 5, policy_str);
	}
	// ситуация, когда подходящих документов нет
	{
		SearchServer server;
		server.AddDocument(5, "dog"s, DocumentStatus::BANNED, {7, 2, 7});
		auto result_by_status = server.FindTopDocuments(policy,"dog"s, DocumentStatus::REMOVED);
		ASSERT_HINT(result_by_status.empty(),
			"Result must be empty, if we don't have documents with requested status"s + policy_str);
	}
}

void TestFindingDocumentsByStatus() {
	TestFindingDocumentsByStatusPolicy(std::execution::seq);
	TestFindingDocumentsByStatusPolicy(std::execution::par);
}

//Корректное вычисление релевантности найденных документов.
void TestCalculateRelevanceOfFindingDocs() {
	SearchServer search_server;
	search_server.AddDocument(0, "black dog green eyes"s, DocumentStatus::ACTUAL, {7, 2, 7});
	search_server.AddDocument(3, "lion"s, DocumentStatus::ACTUAL, {5, -12, 2, 1}); // tf-idf = 0

	const auto result_documents = search_server.FindTopDocuments("black cat"s);
	ASSERT_EQUAL(result_documents.size(), 1u);
	const double idf_black = std::log(2.0 / 1.0); //all docs / docs with word
	const double tf_black_in_doc = 1.0 / 4.0; // query word in doc / all words in doc
	const double waiting_idf_tf = idf_black * tf_black_in_doc; // idf-tf for cat is 0
	ASSERT(std::abs(result_documents.at(0).relevance - waiting_idf_tf) < EPSILON);
}

//Корректное вычисление частоты теримнов TF (term frequency) слов в документе
void TestWordsTfInDocument() {
	int doc_id = 1;
	SearchServer search_server;
	search_server.AddDocument(doc_id, "big dog big eyes"s, DocumentStatus::ACTUAL, {7, 2, 7});

	// Проверяему существующий документ
	std::map<std::string_view, double> words_tf = {
		{"big", 2.0 / 4.0},
		{"dog", 1.0 / 4.0},
		{"eyes", 1.0 / 4.0},
	};
	ASSERT_EQUAL(search_server.GetWordFrequencies(doc_id), words_tf);

	// Проверяему нусуществующий документ
	std::map<std::string_view, double> words_tf_empty = {};
	ASSERT_EQUAL(search_server.GetWordFrequencies(2), words_tf_empty);
}

// Тест проверяет исключения в конструкторе
void TestCreateSearchServer() {
	// Проверяем исключение при создании сервера без стоп-слов
	{
		bool has_exception_without_stop_words = false;
		try {
			SearchServer search_server;
		} catch (...) {
			has_exception_without_stop_words = true;
		}
		ASSERT_HINT(!has_exception_without_stop_words,
			"Search server without stop-words must not generate invalid_argument exception"s);
	}

	// Проверяем исключение при создании сервера без спец.символов в строке стоп-слов
	{
		bool has_exception_without_special_char_in_string = false;
		try {
			SearchServer search_server("dog"s);
		} catch (...) {
			has_exception_without_special_char_in_string = true;
		}
		ASSERT_HINT(!has_exception_without_special_char_in_string,
			"Stop words without special characters must not generate invalid_argument exception"s);
	}

	// Проверяем исключение при создании сервера со спец.символами в строке стоп-слов
	{
		bool has_invalid_argument_exception_in_string = false;
		try {
			SearchServer search_server("do\x12g"s);
		} catch (const std::invalid_argument & exception) {
			has_invalid_argument_exception_in_string = true;
		} catch (...) {
		}
		ASSERT_HINT(has_invalid_argument_exception_in_string,
			"Stop words with special characters must generate invalid_argument exception"s);
	}

	// Проверяем исключение при создании сервера без спец.символов в контейнере стоп-слов
	{
		bool has_exception_without_special_char_in_container = false;
		try {
			std::vector<std::string> stop_words {"in"s, "the"s};
			SearchServer search_server(stop_words);
		} catch (...) {
			has_exception_without_special_char_in_container = true;
		}
		ASSERT_HINT(!has_exception_without_special_char_in_container,
			"Stop words without special characters must not generate invalid_argument exception"s);
	}

	// Проверяем исключение при создании сервера со спец.символами в контейнере стоп-слов
	{
		bool has_invalid_argument_exception_in_container = false;
		try {
			std::vector<std::string> stop_words {"in"s, "do\x12g"s};
			SearchServer search_server(stop_words);
		} catch (const std::invalid_argument & exception) {
			has_invalid_argument_exception_in_container = true;
		} catch (...) {
		}
		ASSERT_HINT(has_invalid_argument_exception_in_container,
			"Stop words with special characters must generate invalid_argument exception"s);
	}

	//Проверяем создание из string_view
	{
		std::string_view stop_words {"dog"s};
		bool has_exception_without_special_char_in_string = false;
		try {
			SearchServer search_server(stop_words);
		} catch (...) {
			has_exception_without_special_char_in_string = true;
		}
		ASSERT_HINT(!has_exception_without_special_char_in_string,
			"Stop words without special characters must not generate invalid_argument exception"s);
	}
}

// Тест проверяет корректность добавления документов
void TestAddingDocuments() {
	// Проверяем изменение количества документов при добавлении
	{
		SearchServer search_server;
		ASSERT_EQUAL_HINT(search_server.GetDocumentCount(), 0,
			"Before first adding the number of documents must be zero"s);
		search_server.AddDocument(3, "lion"s, DocumentStatus::ACTUAL, {1});
		ASSERT_EQUAL_HINT(search_server.GetDocumentCount(), 1,
			"After adding the number of documents must increase"s);
	}

	// Проверяем, что корректно отрабатывают исключительные ситуации
	{
		// Корректное добавление документа
		SearchServer search_server;
		ASSERT_EQUAL_HINT(search_server.GetDocumentCount(), 0,
			"Before first adding the number of documents must be zero"s);
		bool can_add_correct_doc = true;
		try {
			search_server.AddDocument(0, "dog"s, DocumentStatus::ACTUAL, {1});
		} catch (...) {
			can_add_correct_doc = false;
		}
		ASSERT(can_add_correct_doc);
		ASSERT_EQUAL_HINT(search_server.GetDocumentCount(), 1,
			"After successful adding the number of documents must increase"s);

		// Добавление документа с существующим id
		bool has_invalid_argument_exception_if_id_exist = false;
		try {
			search_server.AddDocument(0, "dog"s, DocumentStatus::ACTUAL, {1});
		} catch (const std::invalid_argument & exception) {
			has_invalid_argument_exception_if_id_exist = true;
		} catch (...) {
			has_invalid_argument_exception_if_id_exist = false;
		}
		ASSERT(has_invalid_argument_exception_if_id_exist);
		ASSERT_EQUAL_HINT(search_server.GetDocumentCount(), 1,
			"After fain adding the number of documents must not increase"s);

		// Добавление документа с отрицательным id
		bool has_invalid_argument_exception_if_id_negative = false;
		try {
			search_server.AddDocument(-2, "dog"s, DocumentStatus::ACTUAL, {1});
		} catch (const std::invalid_argument & exception) {
			has_invalid_argument_exception_if_id_negative = true;
		} catch (...) {
			has_invalid_argument_exception_if_id_negative = false;
		}
		ASSERT(has_invalid_argument_exception_if_id_negative);
		ASSERT_EQUAL_HINT(search_server.GetDocumentCount(), 1,
			"After fain adding the number of documents must not increase"s);

		// Добавление документа со спец.символами
		bool has_invalid_argument_exception_if_doc_with_special_characters = false;
		try {
			search_server.AddDocument(3, "do\x12g"s, DocumentStatus::ACTUAL, {1});
		} catch (const std::invalid_argument & exception) {
			has_invalid_argument_exception_if_doc_with_special_characters = true;
		} catch (...) {
			has_invalid_argument_exception_if_doc_with_special_characters = false;
		}
		ASSERT(has_invalid_argument_exception_if_doc_with_special_characters);
		ASSERT_EQUAL_HINT(search_server.GetDocumentCount(), 1,
			"After fain adding the number of documents must not increase"s);
	}
}

// Тест проверяет корректность кодов возврата метода FindTopDocuments
template <typename ExecutionPolicy>
void TestReturnCodesFromFindTopDocumentsPolicy(const ExecutionPolicy & policy) {
	std::string policy_str = PolicyToString(policy);

	SearchServer search_server;
	search_server.AddDocument(3, "lion"s, DocumentStatus::ACTUAL, {1});
	// В корректном запросе отсутствуют исключения
	bool has_exception_if_find_correct_query = false;
	try {
		search_server.FindTopDocuments(policy,"black -cat"s);
	} catch (...) {
		has_exception_if_find_correct_query = true;
	}
	ASSERT_HINT(!has_exception_if_find_correct_query, policy_str);

	// Проверяем исключение, если минус-слово содержит 2 минуса подряд
	bool has_invalid_argument_exception_if_find_two_minus = false;
	try {
		search_server.FindTopDocuments(policy, "black --cat"s);
	} catch (const std::invalid_argument & exception) {
		has_invalid_argument_exception_if_find_two_minus = true;
	} catch (...) {
	}
	ASSERT_HINT(has_invalid_argument_exception_if_find_two_minus, policy_str);

	// Проверяем исключение, если в запросе пустое минус-слово
	bool has_invalid_argument_exception_if_find_empty_minus = false;
	try {
		search_server.FindTopDocuments(policy, "black -"s);
	} catch (const std::invalid_argument & exception) {
		has_invalid_argument_exception_if_find_empty_minus = true;
	} catch (...) {
	}
	ASSERT_HINT(has_invalid_argument_exception_if_find_empty_minus, policy_str);

	// Проверяем исключение, если в запросе слово со спец.символами
	bool has_invalid_argument_exception_if_find_special_characters = false;
	try {
		search_server.FindTopDocuments(policy, "do\x12g"s);
	} catch (const std::invalid_argument & exception) {
		has_invalid_argument_exception_if_find_special_characters = true;
	} catch (...) {
	}
	ASSERT_HINT(has_invalid_argument_exception_if_find_special_characters, policy_str);
}

void TestReturnCodesFromFindTopDocuments() {
	TestReturnCodesFromFindTopDocumentsPolicy(std::execution::seq);
	TestReturnCodesFromFindTopDocumentsPolicy(std::execution::par);
}

// Тест функций begin и end
void TestBeginEnd() {
	SearchServer search_server;
	int doc_id_0 = 3333;
	int doc_id_1 = 3;
	int doc_id_2 = 100;
	search_server.AddDocument(doc_id_0, "lion"s, DocumentStatus::ACTUAL, {1});
	search_server.AddDocument(doc_id_1, "lion"s, DocumentStatus::ACTUAL, {1});
	search_server.AddDocument(doc_id_2, "lion"s, DocumentStatus::ACTUAL, {1});

	std::set<int> history_set = {doc_id_0, doc_id_1, doc_id_2};
	const std::vector<int> history(history_set.begin(), history_set.end());
	//Ожидаем 3 корректные проверки
	unsigned i = 0;
	for (const int document_id : search_server) {
		ASSERT_EQUAL(document_id, history.at(i));
		++i;
	}
}

// Тест проверяет корректность удаление документа
template <typename ExecutionPolicy>
void TestRemoveDocumentPolicy(const ExecutionPolicy & policy) {
	std::string policy_str = PolicyToString(policy);

	SearchServer search_server;
	int before_id = 1;
	int deleted_id = 3;
	int after_id = 5;
	search_server.AddDocument(before_id, "dog and cat"s, DocumentStatus::ACTUAL, {1});
	search_server.AddDocument(deleted_id, "cat"s, DocumentStatus::ACTUAL, {1});
	search_server.AddDocument(after_id, "cat"s, DocumentStatus::ACTUAL, {1});
	search_server.RemoveDocument(policy, deleted_id);

	// Проверяем, что после удаления уменьшилось количество документов
	ASSERT_EQUAL_HINT(search_server.GetDocumentCount(), 2, policy_str);

	// Проверяем, что после удаления id не доступен через итератор
	std::vector<int> after_deleting_ids = {before_id, after_id};
	unsigned i = 0;
	for (auto doc_id : search_server) {
		ASSERT_EQUAL_HINT(doc_id, after_deleting_ids.at(i), policy_str);
		++i;
	}

	// Проверяем, что matching бросает исключение по удалённому id
	bool has_out_of_range_exception_if_document_id_not_exist = false;
	try {
		search_server.MatchDocument("cat"s, deleted_id);
	} catch (const std::out_of_range & exception) {
		has_out_of_range_exception_if_document_id_not_exist = true;
	} catch (...) {
	}
	ASSERT_HINT(has_out_of_range_exception_if_document_id_not_exist, policy_str);

	// Проверяем, что FindTopDocuments не находит удаленный документ
	auto finding_result = search_server.FindTopDocuments("cat"s);
	ASSERT_EQUAL_HINT(finding_result.size(), 2u, policy_str);
	ASSERT_EQUAL_HINT(finding_result.at(0).id, before_id, policy_str);
	ASSERT_EQUAL_HINT(finding_result.at(1).id, after_id, policy_str);
}

void TestRemoveDocument() {
	TestRemoveDocumentPolicy(std::execution::seq);
	TestRemoveDocumentPolicy(std::execution::par);
}

void TestSearchServer() {
	RUN_TEST(TestCreateSearchServer);
	RUN_TEST(TestAddingDocuments);
	RUN_TEST(TestReturnCodesFromFindTopDocuments);
	RUN_TEST(TestExcludeStopWordsFromAddedDocumentContent);
	RUN_TEST(TestExcludeDocumentsWithMinusWordsFromTopDocuments);
	RUN_TEST(TestMatchingDocumentsForQuery);
	RUN_TEST(TestSortingOfSearchingResultByRelevance);
	RUN_TEST(TestСalculationDocumentRating);
	RUN_TEST(TestPredicateInFindTopDocument);
	RUN_TEST(TestFindingDocumentsByStatus);
	RUN_TEST(TestCalculateRelevanceOfFindingDocs);
	RUN_TEST(TestWordsTfInDocument);
	RUN_TEST(TestBeginEnd);
	RUN_TEST(TestRemoveDocument);
}
