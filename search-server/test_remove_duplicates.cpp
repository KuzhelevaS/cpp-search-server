#include "test_remove_duplicates.h"
#include "test_engine.h"
#include "search_server.h"
#include "remove_duplicates.h"

// Проверяем, что в случае отсутствия дубликатов функция ничего не делает
void TestRemoveWithoutDuplicates() {
	SearchServer search_server("and with"s);
	search_server.AddDocument(1, "funny pet and nasty rat"s, DocumentStatus::ACTUAL, {7, 2, 7});
	search_server.AddDocument(2, "funny pet with curly hair"s, DocumentStatus::ACTUAL, {1, 2});
	RemoveDuplicates(search_server);
	ASSERT_EQUAL(search_server.GetDocumentCount(), 2);
}

// Проверяем, что в тест удаляет идентичные документы
void TestRemoveFullDuplicates() {
	// 1 дубликат
	{
		SearchServer search_server("and with"s);
		search_server.AddDocument(1, "funny pet and nasty rat"s, DocumentStatus::ACTUAL, {7, 2, 7});
		search_server.AddDocument(2, "funny pet and nasty rat"s, DocumentStatus::ACTUAL, {7, 2, 7});
		RemoveDuplicates(search_server);
		ASSERT_EQUAL(search_server.GetDocumentCount(), 1);
		ASSERT_EQUAL(*search_server.begin(), 1);
	}

	// 2 дубликата
	{
		SearchServer search_server("and with"s);
		search_server.AddDocument(1, "funny pet and nasty rat"s, DocumentStatus::ACTUAL, {7, 2, 7});
		search_server.AddDocument(2, "funny pet and nasty rat"s, DocumentStatus::ACTUAL, {7, 2, 7});
		search_server.AddDocument(3, "funny pet and nasty rat"s, DocumentStatus::ACTUAL, {7, 2, 7});
		RemoveDuplicates(search_server);
		ASSERT_EQUAL(search_server.GetDocumentCount(), 1);
		ASSERT_EQUAL(*search_server.begin(), 1);
	}
}

// Проверяем, что в тест удаляет документы c одинаковыми словами, но разной частотой
void TestRemoveOnlyWordSetDuplicates() {
	SearchServer search_server("and with"s);
	search_server.AddDocument(1, "funny pet and nasty rat"s, DocumentStatus::ACTUAL, {7, 2, 7});
	search_server.AddDocument(5, "funny funny pet and nasty nasty rat"s, DocumentStatus::ACTUAL, {1, 2});
	RemoveDuplicates(search_server);
	ASSERT_EQUAL(search_server.GetDocumentCount(), 1);
	ASSERT_EQUAL(*search_server.begin(), 1);
}

void TestRemoveDuplicates() {
	RUN_TEST(TestRemoveWithoutDuplicates);
	RUN_TEST(TestRemoveFullDuplicates);
	RUN_TEST(TestRemoveOnlyWordSetDuplicates);
}
