#include "test_paginator.h"
#include "test_engine.h"
#include "paginator.h"
#include <vector>
#include <sstream>
#include <string>

// Проверяет работу, если елементов меньше, чем помещается на странице
void TestIfElementsLessThenPageSizeWithInt() {
	unsigned page_size = 2;
	std::vector<int> data {11};
	const auto pages = Paginate(data, page_size);

	ASSERT_EQUAL(pages.size(), 1u);
	ASSERT_EQUAL(*(*pages.begin()).begin(), 11);
}

// Проверяет работу, если елементов больше, чем помещается на странице,
// но не кратно количеству страниц
void TestIfElementsMoreThenPageSizeWithInt() {
	unsigned page_size = 2;
	std::vector<int> data {11, 22, 33};
	const auto pages = Paginate(data, page_size);

	ASSERT_EQUAL(pages.size(), 2u);
	ASSERT_EQUAL(*(*pages.begin()).begin(), 11);
	ASSERT_EQUAL(*((*pages.begin()).begin() + 1), 22);
	ASSERT_EQUAL(*(*(pages.begin() + 1)).begin(), 33);
}

// Проверяет работу, если количество элементов кратно размеру страницы
void TestIfElementsEqualPageSizeWithInt() {
	unsigned page_size = 2;
	std::vector<int> data {11, 22};
	const auto pages = Paginate(data, page_size);

	ASSERT_EQUAL(pages.size(), 1u);

	std::ostringstream result;
	result << *(pages.begin());
	ASSERT_EQUAL(result.str(), "1122");
}

// Проверяет работу со строковыми данными
void TestElementsWithStr() {
	unsigned page_size = 2;
	std::vector<std::string> data {"abc", "ij", "xyz"};
	const auto pages = Paginate(data, page_size);

	ASSERT_EQUAL(pages.size(), 2u);
	ASSERT_EQUAL(*(*pages.begin()).begin(), "abc");
	ASSERT_EQUAL(*((*pages.begin()).begin() + 1), "ij");
	ASSERT_EQUAL(*(*(pages.begin() + 1)).begin(), "xyz");
}

// Проверяет перегрузку оператора вывода
void TestPrint() {
	unsigned page_size = 2;
	std::vector<int> data {11, 22, 33};
	const auto pages = Paginate(data, page_size);

	std::ostringstream result;
	result << *(pages.begin());
	ASSERT_EQUAL(result.str(), "1122");

	result.str(""s);
	result.clear();
	result << *(++pages.begin());
	ASSERT_EQUAL(result.str(), "33");
}

void TestPaginator() {
	RUN_TEST(TestIfElementsLessThenPageSizeWithInt);
	RUN_TEST(TestIfElementsMoreThenPageSizeWithInt);
	RUN_TEST(TestIfElementsEqualPageSizeWithInt);
	RUN_TEST(TestElementsWithStr);
	RUN_TEST(TestPrint);
}
