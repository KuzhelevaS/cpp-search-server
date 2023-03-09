#include <iostream>
#include <string>
#include <vector>
#include <set>
#include <algorithm>
#include <map>
#include <cmath>

using namespace std;

const int MAX_RESULT_DOCUMENT_COUNT = 5;

enum class DocumentStatus {
	ACTUAL,
	IRRELEVANT,
	BANNED,
	REMOVED
};

string StatusAsString(DocumentStatus status) {
	switch (status) {
		case DocumentStatus::ACTUAL :
			return "ACTUAL"s;
		case DocumentStatus::IRRELEVANT :
			return "IRRELEVANT"s;
		case DocumentStatus::BANNED :
			return "BANNEDL"s;
		case DocumentStatus::REMOVED :
			return "REMOVED"s;
		default:
			return ""s;
	}
}

struct Document {
	int id;
	double relevance;
	int rating;
};

string ReadLine() {
	string query;
	getline(cin, query);
	return query;
}

int ReadLineWithNumber() {
	int result = 0;
	cin >> result;
	ReadLine();
	return result;
}

class SearchServer {
	public:
		// Добавляет документ
		void AddDocument(int document_id, const string & document, DocumentStatus status, const vector<int> & ratings ) {
			const vector<string> words = SplitIntoWordsNoStop(document);
			document_count_++;
			document_info_[document_id] = {ComputeAverageRating(ratings), status};
			double tf_coeff = 1.0 / static_cast<double>(words.size());
			for(const auto & word : words) {
				documents_with_tf_[word][document_id] += tf_coeff;
			}
		}
		// Задает список стоп-слов
		void SetStopWords(const string& text) {
			for (const string& word : SplitIntoWords(text)) {
				stop_words_.insert(word);
			}
		}
		// Возвращает топ-5 самых релевантных документов
		vector<Document> FindTopDocuments(const string& raw_query) const {
			return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
		}

		vector<Document> FindTopDocuments(const string& raw_query, DocumentStatus status) const {
			return FindTopDocuments(raw_query,
				[status](int document_id, DocumentStatus document_status, int rating) {
					return document_status == status;
				});
		}

		template <typename Filter>
		vector<Document> FindTopDocuments(const string& raw_query, Filter filter) const {
			const Query query_words = ParseQuery(raw_query);
			vector<Document> result = FindAllDocuments(query_words, filter);

			sort(result.begin(), result.end(),
				[](const Document & lhs, const Document & rhs) {
					if (abs(lhs.relevance - rhs.relevance) < 1e-6) {
						return lhs.rating > rhs.rating;
					} else {
						return lhs.relevance > rhs.relevance;
					}
				});

			if (result.size() > MAX_RESULT_DOCUMENT_COUNT) {
				result.resize(MAX_RESULT_DOCUMENT_COUNT);
			}

			return result;
		}

		int GetDocumentCount() const {
			return static_cast<int>(document_info_.size());
		}

		tuple<vector<string>, DocumentStatus> MatchDocument(const string& raw_query, int document_id) const {
			const Query query_words = ParseQuery(raw_query);

			bool need_check_plus_words = true;
			for (const string & minus_word : query_words.minus_words) {
				if (HasWordInDocument(minus_word, document_id)) {
					need_check_plus_words = false;
				}
			}

			vector<string> matched_words;
			if (need_check_plus_words) {
				for (const string & plus_word : query_words.plus_words) {
					if (HasWordInDocument(plus_word, document_id)) {
						matched_words.push_back(plus_word);
					}
				}
				sort(matched_words.begin(), matched_words.end());
			}
			return {matched_words, document_info_.at(document_id).status};
		}

	private:
		map<string, map<int, double>> documents_with_tf_; // слово - id док-та, tf
		set<string> stop_words_;
		int document_count_ = 0;
		struct DocumentInfo {
			int rating;
			DocumentStatus status;
		};
		map<int, DocumentInfo> document_info_;

		struct Query {
			set<string> plus_words;
			set<string> minus_words;
		};

		struct QueryWord {
			string word;
			bool is_minus;
			bool is_stop;
		};

		bool IsStopWord(const string & word) const {
			return stop_words_.count(word) != 0;
		}

		//Разбивает строку на массив строк без стоп-слов
		vector<string> SplitIntoWordsNoStop(const string& text) const {
			vector<string> words;
			for (const string& word : SplitIntoWords(text)) {
				if (!IsStopWord(word)) {
					words.push_back(word);
				}
			}
			return words;
		}

		QueryWord CheckWord(const string & word) const {
			QueryWord checked_word;

			if (word[0] == '-') {
				checked_word.is_minus = true;
			} else {
				checked_word.is_minus = false;
			}

			checked_word.word = checked_word.is_minus ? word.substr(1) : word;

			if (IsStopWord(checked_word.word)) {
				checked_word.is_stop = true;
			} else {
				checked_word.is_stop = false;
			}

			return checked_word;
		}

		//Разбивает строку на упорядоченный массив строк без повторений без стоп-слов
		Query ParseQuery(const string& text) const {
			Query query;
			for (const string& word : SplitIntoWords(text)) {
				QueryWord checked_word = CheckWord(word);
				if (checked_word.is_stop) {
					continue;
				}
				if (checked_word.is_minus) {
					query.minus_words.insert(checked_word.word);
				} else {
					query.plus_words.insert(checked_word.word);
				}
			}
			return query;
		}

		template <typename Filter>
		vector<Document> FindAllDocuments(const Query& query_words, Filter filter) const {
			map<int, double> matched_documents;
			for(const auto & plus : query_words.plus_words) {
				// нужно, т.к. дальше вызываем documents_with_tf_.at()
				// также предупреждаем деление на 0
				if (documents_with_tf_.count(plus) == 0) {
					continue;
				}
				double idf = CalcIdf(plus);
				for (const auto & [doc_id, tf] : documents_with_tf_.at(plus)) {
					matched_documents[doc_id] += idf * tf;
				}
			}
			for(const auto & minus : query_words.minus_words) {
				// нужно, т.к. дальше вызываем documents_with_tf_.at()
				// также предупреждаем деление на 0
				if (documents_with_tf_.count(minus) == 0) {
					continue;
				}
				for (const auto & [doc_id, tf] : documents_with_tf_.at(minus)) {
					matched_documents.erase(doc_id);
				}
			}
			vector<Document> result;
			for (const auto & [id, rel] : matched_documents) {
				if (filter(id,
					document_info_.at(id).status,
					document_info_.at(id).rating))
				{
					int rating = document_info_.at(id).rating;
					result.push_back({id, rel, rating});
				}
			}
			return result;
		}


		double CalcIdf(const string & word) const {
			return log(static_cast<double>(document_count_) / static_cast<double>(documents_with_tf_.at(word).size()));
		}

		bool HasWordInDocument(const string & word, int document_id) const {
			return documents_with_tf_.count(word) != 0
				&& documents_with_tf_.at(word).count(document_id) != 0;
		}

		//Разбивает строку на массив строк, разделитель пробел(ы)
		static vector<string> SplitIntoWords(const string& text) {
			vector<string> words;
			string word = ""s;
			for (const char c : text) {
				if (c == ' ') {
					if (!word.empty()) {
						words.push_back(word);
						word.clear();
					}
				} else {
					word += c;
				}
			}
			if (!word.empty()) {
				words.push_back(word);
			}
			return words;
		}

		//Возвращаем среднее значение рейтинга
		static int ComputeAverageRating(const vector<int>& ratings) {
			if (ratings.empty()) {
				return 0;
			} else {
				int ratings_sum = 0;
				for (int single_rate : ratings) {
					ratings_sum += single_rate;
				}
				int average = ratings_sum / static_cast<int>(ratings.size());
				return average;
			}
		}
};

template <typename Container>
void Print(ostream & out, const Container & container) {
	bool is_first = true;
	for (const auto & element : container) {
		if (is_first) {
			is_first = false;
		} else {
			out << ", ";
		}
		out << element;
	}
}

template <typename T>
ostream & operator << (ostream & out, const vector<T> & v) {
	out << "["s;
	Print(out, v);
	out << "]"s;
	return out;
}

template <typename T>
ostream & operator << (ostream & out, const set<T> & s) {
	out << "{"s;
	Print(out, s);
	out << "}"s;
	return out;
}

template <typename Key, typename Value>
ostream & operator << (ostream & out, const map<Key,Value> & m) {
	out << "{"s;
	Print(out, m);
	out << "}"s;
	return out;
}

template <typename Key, typename Value>
ostream & operator << (ostream & out, const pair<Key,Value> & p) {
	out << p.first << ": "s << p.second;
	return out;
}

template <typename T, typename U>
void AssertEqualImpl(const T& t, const U& u, const string& t_str, const string& u_str, const string& file,
		const string& func, unsigned line, const string& hint) {
	if (t != u) {
		cerr << boolalpha;
		cerr << file << "("s << line << "): "s << func << ": "s;
		cerr << "ASSERT_EQUAL("s << t_str << ", "s << u_str << ") failed: "s;
		cerr << t << " != "s << u << "."s;
		if (!hint.empty()) {
			cerr << " Hint: "s << hint;
		}
		cerr << endl;
		abort();
	}
}

#define ASSERT_EQUAL(a, b) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_EQUAL_HINT(a, b, hint) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, (hint))

void AssertImpl(bool value, const string& expr, const string& file, unsigned line, const string& func, const string& hint = ""s) {
	if (!value) {
		cerr << file << "("s << line << "): "s << func << ": "s;
		cerr << "ASSERT("s << expr << ") failed."s;
		if (!hint.empty()) {
			cerr << " Hint: "s << hint;
		}
		cerr << endl;
		abort();
	}
}

#define ASSERT(expr) AssertImpl((expr), #expr, __FILE__, __LINE__, __FUNCTION__)

#define ASSERT_HINT(expr, hint) AssertImpl((expr), #expr, __FILE__, __LINE__, __FUNCTION__, hint)

template <typename Function>
void RunTestImpl(Function function, const std::string & function_name) {
	function();
	cerr << function_name << " OK" << endl;
}

#define RUN_TEST(func)  RunTestImpl((func), #func)

// -------- Начало модульных тестов поисковой системы ----------

// Тест проверяет, что поисковая система исключает стоп-слова при добавлении документов
void TestExcludeStopWordsFromAddedDocumentContent() {
	const int doc_id = 42;
	const string content = "cat in the city"s;
	const vector<int> ratings = {1, 2, 3};
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
		const Document& doc0 = found_docs[0];
		ASSERT_EQUAL(doc0.id, doc_id);
	}

	// Затем убеждаемся, что поиск этого же слова, входящего в список стоп-слов,
	// возвращает пустой результат
	{
		SearchServer server;
		server.SetStopWords("in the"s);
		server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
		ASSERT_HINT(server.FindTopDocuments("in"s).empty(),
			"Stop words must be excluded from documents"s);
	}

}

// Тест проверяет, что результаты поиска не включают документы с минус-словами
void TestExcludeDocumentsWithMinusWordsFromTopDocuments() {
	const int doc_id = 42;
	const string content = "cat in the city"s;
	const vector<int> ratings = {1, 2, 3};
	//Проверяем ситуацию если минус-слова не входят в список стоп-слов
	{
		SearchServer server;
		server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
		ASSERT_HINT(server.FindTopDocuments("cat -in"s).empty(),
			"Result must be empty, if minus word is not a stop word"s);
	}

	//Проверяем ситуацию если минус-слова включают стоп-слов
	{
		SearchServer server;
		server.SetStopWords("in the"s);
		server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
		const auto found_docs = server.FindTopDocuments("cat -in"s);
		ASSERT_EQUAL_HINT(found_docs.size(), 1u,
			"If minus words is a stop word, it must be ignore in query"s);
		ASSERT_EQUAL(found_docs.at(0).id, doc_id);
	}
}

// Тест проверяет, соответствие документа поисковому запросу
void TestMatchingDocumentsForQuery() {
	// При матчинге документа по поисковому запросу
	// должны быть возвращены все слова из поискового запроса, присутствующие в документе.
	const int doc_id = 42;
	const string content = "cat in the city"s;
	const vector<int> ratings = {1, 2, 3};
	const DocumentStatus status = DocumentStatus::ACTUAL;
	{
		SearchServer server;
		server.AddDocument(doc_id, content, status, ratings);
		const auto [result_matched, result_status] = server.MatchDocument("the cat"s, doc_id);
		ASSERT_EQUAL(StatusAsString(result_status), StatusAsString(status));
		vector<string> waiting_words = {"cat"s, "the"s};
		ASSERT_EQUAL(result_matched, waiting_words);
	}
	
	// Если есть соответствие хотя бы по одному минус-слову, должен возвращаться пустой список слов.
	{
		SearchServer server;
		server.AddDocument(doc_id, content, status, ratings);
		const auto [result_matched, result_status] = server.MatchDocument("the cat -in"s, doc_id);
		ASSERT_EQUAL(StatusAsString(result_status), StatusAsString(status));
		ASSERT_HINT(result_matched.empty(),
			"Document with minus-words must not be found"s);
	}

	//Поиск по стоп-слову должен возвращать пустой результат
	{
		SearchServer server;
		server.SetStopWords("in the"s);
		server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
		const auto [result_matched, result_status] = server.MatchDocument("in"s, doc_id);
		ASSERT_HINT(result_matched.empty(),
			"Don't matching by stop-words"s);
	}

	//Если если минус-слово входит в список стоп-слов, оно должно быть просто проигнорировано
	{
		SearchServer server;
		server.SetStopWords("in the"s);
		server.AddDocument(doc_id, content, status, ratings);
		const auto [result_matched, result_status] = server.MatchDocument("-the cat"s, doc_id);
		ASSERT_EQUAL(StatusAsString(result_status), StatusAsString(status));
		vector<string> waiting_words = {"cat"s};
		ASSERT_EQUAL(result_matched, waiting_words);
	}
}

// Тест проверяет сортировку по релевантности
void TestSortingOfSearchingResultByRelevance() {
	// Если у всех документов разная релевантность
	{
		SearchServer search_server;
		search_server.AddDocument(0, "white cat black hat"s, DocumentStatus::ACTUAL, {8, -3}); // tf-idf = 0.245207313252932
		search_server.AddDocument(1, "black dog green eyes"s, DocumentStatus::ACTUAL, {7, 2, 7}); // tf-idf = 0.0719205181129452
		search_server.AddDocument(2, "black cat with name cat"s, DocumentStatus::ACTUAL, {5, -12, 2, 1}); // tf-idf = 0.334795286714334
		search_server.AddDocument(3, "lion"s, DocumentStatus::ACTUAL, {5, -12, 2, 1}); // tf-idf = 0

		vector<Document> result_documents = search_server.FindTopDocuments("black cat"s);
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

		vector<Document> result_documents = search_server.FindTopDocuments("black cat"s);
		ASSERT_EQUAL(result_documents.size(), 2u);
		ASSERT(abs(result_documents.at(0).relevance - result_documents.at(1).relevance) < EPSILON);
		ASSERT(result_documents.at(0).rating >= result_documents.at(1).rating);
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
void TestPredicatInFindTopDocument() {
	const int doc_id_1 = 42;
	const int doc_id_2 = 33;
	const string content_1 = "cat in the city"s;
	const string content_2 = "cat in the city"s;
	const vector<int> ratings_1 = {1, 2, 3};
	const vector<int> ratings_2 = {5, 2, 3};
	const DocumentStatus status_1 = DocumentStatus::ACTUAL;
	const DocumentStatus status_2 = DocumentStatus::BANNED;
	// тестируем параметр id
	{
		SearchServer server;
		server.AddDocument(doc_id_1, content_1, status_1, ratings_1);
		server.AddDocument(doc_id_2, content_2, status_2, ratings_2);
		auto predicate_test_id = server.FindTopDocuments("cat"s,[](int document_id, DocumentStatus status, int rating) {
				(void)status; (void)rating;
				return document_id % 2 == 0;
			});
		ASSERT_EQUAL(predicate_test_id.size(), 1u);
		ASSERT_EQUAL(predicate_test_id.at(0).id, 42);
	}
	
	// тестируем параметр status
	{
		SearchServer server;
		server.AddDocument(doc_id_1, content_1, status_1, ratings_1);
		server.AddDocument(doc_id_2, content_2, status_2, ratings_2);
		auto predicate_test_status = server.FindTopDocuments("cat"s,[](int document_id, DocumentStatus status, int rating) {
				(void)document_id; (void)rating;
				return status == DocumentStatus::BANNED;
			});
		ASSERT_EQUAL(predicate_test_status.size(), 1u);
		ASSERT_EQUAL(predicate_test_status.at(0).id, 33);
	}
	
	// тестируем параметр rating
	{
		SearchServer server;
		server.AddDocument(doc_id_1, content_1, status_1, ratings_1);
		server.AddDocument(doc_id_2, content_2, status_2, ratings_2);
		auto predicate_test_rating = server.FindTopDocuments("cat"s,[](int document_id, DocumentStatus status, int rating) {
				(void)document_id; (void)status;
				return rating >= 2;
			});
		ASSERT_EQUAL(predicate_test_rating.size(), 2u);
		ASSERT_EQUAL(predicate_test_rating.at(0).id, 33);
		ASSERT_EQUAL(predicate_test_rating.at(1).id, 42);
	}
}

//Поиск документов, имеющих заданный статус.
void TestFindingDocumentsByStatus() {
	// ситуация, когда находим документ по статусу
	{
		SearchServer server;
		server.AddDocument(5, "dog"s, DocumentStatus::BANNED, {7, 2, 7});
		auto result_by_status = server.FindTopDocuments("dog"s, DocumentStatus::BANNED);
		ASSERT_EQUAL(result_by_status.size(), 1u);
		ASSERT_EQUAL(result_by_status.at(0).id, 5);
	}
	
	// ситуация, когда подходящих документов нет
	{
		SearchServer server;
		server.AddDocument(5, "dog"s, DocumentStatus::BANNED, {7, 2, 7});
		auto result_by_status = server.FindTopDocuments("dog"s, DocumentStatus::REMOVED);
		ASSERT_HINT(result_by_status.empty(),
			"Result must be empty, if we don't have documents with requested status"s);
	}
}

//Корректное вычисление релевантности найденных документов.
void TestCalculateRelevanceOfFindingDocs() {
	SearchServer search_server;
	search_server.AddDocument(0, "black dog green eyes"s, DocumentStatus::ACTUAL, {7, 2, 7});  
	search_server.AddDocument(3, "lion"s, DocumentStatus::ACTUAL, {5, -12, 2, 1}); // tf-idf = 0

	vector<Document> result_documents = search_server.FindTopDocuments("black cat"s);
	ASSERT_EQUAL(result_documents.size(), 1u);
	const double idf_black = log(2.0 / 1.0); //all docs / docs with word
	const double tf_black_in_doc = 1.0 / 4.0; // query word in doc / all words in doc
	const double waiting_idf_tf = idf_black * tf_black_in_doc; // idf-tf for cat is 0
	ASSERT(abs(result_documents.at(0).relevance - waiting_idf_tf) < EPSILON);
}

// Тест проверяет изменение количества документов при добавлении
void TestDocumentCountBeforeAndAfterAdding() {
	SearchServer search_server;
	ASSERT_EQUAL_HINT(search_server.GetDocumentCount(), 0,
		"Before first adding the number of documents must be zero"s);
	search_server.AddDocument(3, "lion"s, DocumentStatus::ACTUAL, {1});
	ASSERT_EQUAL_HINT(search_server.GetDocumentCount(), 1,
		"After adding the number of documents must increase"s);
}

// Функция TestSearchServer является точкой входа для запуска тестов
void TestSearchServer() {
	RUN_TEST(TestExcludeStopWordsFromAddedDocumentContent);
	RUN_TEST(TestExcludeDocumentsWithMinusWordsFromTopDocuments);
	RUN_TEST(TestMatchingDocumentsForQuery);
	RUN_TEST(TestSortingOfSearchingResultByRelevance);
	RUN_TEST(TestСalculationDocumentRating);
	RUN_TEST(TestPredicatInFindTopDocument);
	RUN_TEST(TestFindingDocumentsByStatus);
	RUN_TEST(TestCalculateRelevanceOfFindingDocs);
	RUN_TEST(TestDocumentCountBeforeAndAfterAdding);
}

// --------- Окончание модульных тестов поисковой системы -----------

void PrintDocument(const Document& document) {
	cout << "{ "s
		<< "document_id = "s << document.id << ", "s
		<< "relevance = "s << document.relevance << ", "s
		<< "rating = "s << document.rating
		<< " }"s << endl;
}

int main() {
	TestSearchServer();
	
	SearchServer search_server;
	search_server.SetStopWords("и в на"s);
	search_server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, {8, -3});
	search_server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, {7, 2, 7});
	search_server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, {5, -12, 2, 1});
	search_server.AddDocument(3, "ухоженный скворец евгений"s, DocumentStatus::BANNED, {9});
	cout << "ACTUAL by default:"s << endl;
	for (const Document& document : search_server.FindTopDocuments("пушистый ухоженный кот"s)) {
		PrintDocument(document);
	}
	cout << "BANNED:"s << endl;
	for (const Document& document : search_server.FindTopDocuments("пушистый ухоженный кот"s, DocumentStatus::BANNED)) {
		PrintDocument(document);
	}
	cout << "Even ids:"s << endl;
	for (const Document& document : search_server.FindTopDocuments("пушистый ухоженный кот"s, [](int document_id, DocumentStatus status, int rating) { return document_id % 2 == 0; })) {
		PrintDocument(document);
	}
	return 0;
}
