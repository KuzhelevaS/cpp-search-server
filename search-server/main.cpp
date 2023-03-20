#include <iostream>
#include <string>
#include <vector>
#include <set>
#include <algorithm>
#include <map>
#include <cmath>
#include <numeric>

using namespace std;

constexpr int MAX_RESULT_DOCUMENT_COUNT = 5;
constexpr double EPSILON = 1e-6;

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
	Document() = default;
	Document(int id, double relevance, int rating)
		: id(id), relevance(relevance), rating(rating) {}

	int id = 0;
	double relevance = 0;
	int rating = 0;
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
		SearchServer(const string& text = ""s)
			: stop_words_(MakeStopWords(SplitIntoWords(text)))
		{}

		template <typename Container>
		SearchServer(const Container & container)
			: stop_words_(MakeStopWords(container))
		{}

		// Добавляет документ
		void AddDocument(int document_id, const string & document,
			DocumentStatus status, const vector<int> & ratings)
		{
			if (document_id < 0) {
				throw invalid_argument("Id less then zero");
			}
			if (document_info_.count(document_id)) {
				throw invalid_argument("Document with this id already exists");
			}

			const vector<string> words = SplitIntoWordsNoStop(document);
			if (!IsValidAllWords(words)) {
				throw invalid_argument("Document contain special characters");
			}
			adding_history_.push_back(document_id);
			document_count_++;
			document_info_[document_id] = {ComputeAverageRating(ratings), status};
			double tf_coeff = 1.0 / static_cast<double>(words.size());
			for(const auto & word : words) {
				documents_with_tf_[word][document_id] += tf_coeff;
			}
		}

		// Возвращает топ-5 самых релевантных документов
		vector<Document> FindTopDocuments(const string& raw_query,
			DocumentStatus status = DocumentStatus::ACTUAL) const
		{
			return FindTopDocuments(raw_query,
				[status](int document_id, DocumentStatus document_status, int rating) {
					return document_status == status;
					(void)document_id; (void)rating;
				});
		}

		template <typename Filter>
		vector<Document> FindTopDocuments(const string& raw_query, Filter filter) const {
			const Query query_words = ParseQuery(raw_query);

			vector<Document> result = FindAllDocuments(query_words, filter);

			sort(result.begin(), result.end(),
				[](const Document & lhs, const Document & rhs) {
					if (abs(lhs.relevance - rhs.relevance) < EPSILON) {
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

		int GetDocumentId(int index) const {
			return adding_history_.at(static_cast<unsigned>(index));
		}

		tuple<vector<string>, DocumentStatus> MatchDocument(const string& raw_query, int document_id) const
		{
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
			return tie(matched_words, document_info_.at(document_id).status);
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
		vector<int> adding_history_;

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
				if (!IsValidWord(word)) {
					throw invalid_argument("Query contain special characters");
				}
				QueryWord checked_word = CheckWord(word);
				if (checked_word.is_stop) {
					continue;
				}
				if (checked_word.is_minus) {
					if (checked_word.word.empty()) {
						throw invalid_argument("Empty minus-word in query");
					}
					if (checked_word.word[0] == '-') {
						throw invalid_argument("More then 1 minus-character in minus-word in query");
					}
					query.minus_words.insert(checked_word.word);
				} else {
					query.plus_words.insert(checked_word.word);
				}
			}
			return query;
		}

				// Задает список стоп-слов
		template <typename Container>
		set<string> MakeStopWords(const Container & container) {
			set<string> stop_words;
			for (const string& word : container) {
				if (!IsValidWord(word)) {
					string message = "Stop word \"" + word + "\" contain special characters"s;
					throw invalid_argument(message);
				}
				if (!word.empty()) {
					stop_words.insert(word);
				}
			}
			return stop_words;
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

		template <typename WordsContainer>
		static bool IsValidAllWords(WordsContainer & words) {
			for (const auto & word : words) {
				if (!IsValidWord(word)) {
					return false;
				}
			}
			return true;
		}

		static bool IsValidWord(const string& word) {
			// A valid word must not contain special characters
			return none_of(word.begin(), word.end(), [](char c) {
				return c >= '\0' && c < ' ';
			});
		}

		//Возвращаем среднее значение рейтинга
		static int ComputeAverageRating(const vector<int>& ratings) {
			if (ratings.empty()) {
				return 0;
			} else {
				int ratings_sum = accumulate(ratings.begin(), ratings.end(), 0);
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

#define ASSERT(expr) AssertImpl(!!(expr), #expr, __FILE__, __LINE__, __FUNCTION__)

#define ASSERT_HINT(expr, hint) AssertImpl(!!(expr), #expr, __FILE__, __LINE__, __FUNCTION__, hint)

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
		const vector<string> stop_words_vector = {"in"s, "in"s, "the"s, ""s};
		SearchServer server(stop_words_vector);
		server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
		ASSERT_HINT(server.FindTopDocuments("in"s).empty(),
			"Stop words must be excluded from documents"s);
	}

	// Затем убеждаемся, что поиск этого же слова, входящего в список стоп-слов,
	// возвращает пустой результат если стоп-слова инициализированы через set
	{
		const set<string> stop_words_vector = {"in"s, "the"s, ""s};
		SearchServer server(stop_words_vector);
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
		SearchServer server("in the"s);
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
		SearchServer server("in the"s);
		server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
		const auto [result_matched, result_status] = server.MatchDocument("in"s, doc_id);
		ASSERT_HINT(result_matched.empty(),
			"Don't matching by stop-words"s);
	}

	//Если если минус-слово входит в список стоп-слов, оно должно быть просто проигнорировано
	{
		SearchServer server("in the"s);
		server.AddDocument(doc_id, content, status, ratings);
		const auto [result_matched, result_status] = server.MatchDocument("-the cat"s, doc_id);
		ASSERT_EQUAL(StatusAsString(result_status), StatusAsString(status));
		vector<string> waiting_words = {"cat"s};
		ASSERT_EQUAL(result_matched, waiting_words);
	}

	//Проверяем, что функция корректно отрабатывает ошибочные входные данные
	{
		SearchServer server;
		server.AddDocument(doc_id, content, status, ratings);

		// В корректном запросе отсутствуют исключения
		bool has_exception_if_match_correct_query = false;
		try {
			server.MatchDocument("-the cat"s, doc_id);
		} catch (...) {
			has_exception_if_match_correct_query = true;
		}
		ASSERT(!has_exception_if_match_correct_query);

		// Проверяем исключение, если минус-слово содержит 2 минуса подряд
		bool has_invalid_argument_exception_if_match_two_minus = false;
		try {
			server.MatchDocument("--the cat"s, doc_id);
		} catch (const invalid_argument & exception) {
			has_invalid_argument_exception_if_match_two_minus = true;
		} catch (...) {
		}
		ASSERT(has_invalid_argument_exception_if_match_two_minus);

		// Проверяем исключение, если в запросе пустое минус-слово
		bool has_invalid_argument_exception_if_match_empty_minus = false;
		try {
			server.MatchDocument("black -"s, doc_id);
		} catch (const invalid_argument & exception) {
			has_invalid_argument_exception_if_match_empty_minus = true;
		} catch (...) {
		}
		ASSERT(has_invalid_argument_exception_if_match_empty_minus);

		// Проверяем исключение, если в запросе слово со спец.символами
		bool has_invalid_argument_exception_if_match_special_characters = false;
		try {
			server.MatchDocument("do\x12g"s, doc_id);
		} catch (const invalid_argument & exception) {
			has_invalid_argument_exception_if_match_special_characters = true;
		} catch (...) {
		}
		ASSERT(has_invalid_argument_exception_if_match_special_characters);
	}
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

		const auto result_documents = search_server.FindTopDocuments("black cat"s);
		ASSERT_EQUAL(result_documents.size(), 2u);
		ASSERT(abs(result_documents.at(0).relevance - result_documents.at(1).relevance) < EPSILON);
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
void TestPredicateInFindTopDocument() {
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
		auto predicate_test_id = server.FindTopDocuments("cat"s,[](int document_id,
			DocumentStatus status, int rating)
		{
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
		auto predicate_test_status = server.FindTopDocuments("cat"s,[](int document_id,
			DocumentStatus status, int rating)
		{
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
		auto predicate_test_rating = server.FindTopDocuments("cat"s,[](int document_id,
			DocumentStatus status, int rating)
		{
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

	const auto result_documents = search_server.FindTopDocuments("black cat"s);
	ASSERT_EQUAL(result_documents.size(), 1u);
	const double idf_black = log(2.0 / 1.0); //all docs / docs with word
	const double tf_black_in_doc = 1.0 / 4.0; // query word in doc / all words in doc
	const double waiting_idf_tf = idf_black * tf_black_in_doc; // idf-tf for cat is 0
	ASSERT(abs(result_documents.at(0).relevance - waiting_idf_tf) < EPSILON);
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
		} catch (const invalid_argument & exception) {
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
			vector<string> stop_words {"in"s, "the"s};
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
			vector<string> stop_words {"in"s, "do\x12g"s};
			SearchServer search_server(stop_words);
		} catch (const invalid_argument & exception) {
			has_invalid_argument_exception_in_container = true;
		} catch (...) {
		}
		ASSERT_HINT(has_invalid_argument_exception_in_container,
			"Stop words with special characters must generate invalid_argument exception"s);
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
		} catch (const invalid_argument & exception) {
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
		} catch (const invalid_argument & exception) {
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
		} catch (const invalid_argument & exception) {
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
void TestReturnCodesFromFindTopDocuments() {
	SearchServer search_server;
	search_server.AddDocument(3, "lion"s, DocumentStatus::ACTUAL, {1});
	// В корректном запросе отсутствуют исключения
	bool has_exception_if_find_correct_query = false;
	try {
		search_server.FindTopDocuments("black -cat"s);
	} catch (...) {
		has_exception_if_find_correct_query = true;
	}
	ASSERT(!has_exception_if_find_correct_query);

	// Проверяем исключение, если минус-слово содержит 2 минуса подряд
	bool has_invalid_argument_exception_if_find_two_minus = false;
	try {
		search_server.FindTopDocuments("black --cat"s);
	} catch (const invalid_argument & exception) {
		has_invalid_argument_exception_if_find_two_minus = true;
	} catch (...) {
	}
	ASSERT(has_invalid_argument_exception_if_find_two_minus);

	// Проверяем исключение, если в запросе пустое минус-слово
	bool has_invalid_argument_exception_if_find_empty_minus = false;
	try {
		search_server.FindTopDocuments("black -"s);
	} catch (const invalid_argument & exception) {
		has_invalid_argument_exception_if_find_empty_minus = true;
	} catch (...) {
	}
	ASSERT(has_invalid_argument_exception_if_find_empty_minus);

	// Проверяем исключение, если в запросе слово со спец.символами
	bool has_invalid_argument_exception_if_find_special_characters = false;
	try {
		search_server.FindTopDocuments("do\x12g"s);
	} catch (const invalid_argument & exception) {
		has_invalid_argument_exception_if_find_special_characters = true;
	} catch (...) {
	}
	ASSERT(has_invalid_argument_exception_if_find_special_characters);
}

// Тест функции GetDocumentId
void TestGetDocumentId() {
	SearchServer search_server;
	int doc_id_0 = 3333;
	int doc_id_1 = 3;
	int doc_id_2 = 100;
	search_server.AddDocument(doc_id_0, "lion"s, DocumentStatus::ACTUAL, {1});
	search_server.AddDocument(doc_id_1, "lion"s, DocumentStatus::ACTUAL, {1});
	search_server.AddDocument(doc_id_2, "lion"s, DocumentStatus::ACTUAL, {1});

	ASSERT_EQUAL(search_server.GetDocumentId(0), doc_id_0);
	ASSERT_EQUAL(search_server.GetDocumentId(1), doc_id_1);
	ASSERT_EQUAL(search_server.GetDocumentId(2), doc_id_2);

	// Проверяем исключение, если индекс отрицательный
	bool has_out_of_range_exception_if_doc_index_less_zero = false;
	try {
		search_server.GetDocumentId(-1);
	} catch (const out_of_range & exception) {
		has_out_of_range_exception_if_doc_index_less_zero = true;
	} catch (...) {}
	ASSERT(has_out_of_range_exception_if_doc_index_less_zero);

	// Проверяем исключение, если индекс больше, чем число документов
	bool has_out_of_range_exception_if_doc_index_more_doc_amount = false;
	try {
		search_server.GetDocumentId(3);
	} catch (const out_of_range & exception) {
		has_out_of_range_exception_if_doc_index_more_doc_amount = true;
	} catch (...) {}
	ASSERT(has_out_of_range_exception_if_doc_index_more_doc_amount);

}

// Функция TestSearchServer является точкой входа для запуска тестов
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
	RUN_TEST(TestGetDocumentId);
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
	// Если вы видите эту строку, значит все тесты прошли успешно
	cout << "Search server testing finished"s << endl;

	const vector<string> stop_words_vector = {"и"s, "в"s, "на"s, ""s, "в"s};
	SearchServer search_server(stop_words_vector);
	search_server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, {8, -3});
	search_server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, {7, 2, 7});

	try {
		search_server.AddDocument(1, "пушистый пёс и модный ошейник"s, DocumentStatus::ACTUAL, {1, 2});
	} catch( const invalid_argument & exception) {
		cout << "Документ не был добавлен. Причина: "
			<< exception.what() << endl;
	}

	try {
		search_server.AddDocument(-1, "пушистый пёс и модный ошейник"s, DocumentStatus::ACTUAL, {1, 2});
	} catch( const invalid_argument & exception) {
		cout << "Документ не был добавлен. Причина: "
			<< exception.what() << endl;
	}

	try {
		search_server.AddDocument(3, "большой пёс скво\x12рец"s, DocumentStatus::ACTUAL, {1, 3, 2});
	} catch( const invalid_argument & exception) {
		cout << "Документ не был добавлен. Причина: "
			<< exception.what() << endl;
	}

	search_server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, {5, -12, 2, 1});
	search_server.AddDocument(3, "ухоженный скворец евгений"s, DocumentStatus::BANNED, {9});

	cout << "ACTUAL by default:"s << endl;
	try {
		for (const Document& document : search_server.FindTopDocuments("пушистый ухоженный кот"s)) {
			PrintDocument(document);
		}
	} catch (const invalid_argument & exception) {
		cout << "Ошибка в поисковом запросе. Причина: "s << exception.what() << endl;
	} catch (...) {
		cout << "Неизвестная ошибка."s << endl;
	}
	cout << "BANNED:"s << endl;
	try {
		for (const Document& document : search_server.FindTopDocuments("пушистый ухоженный кот"s, DocumentStatus::BANNED)) {
			PrintDocument(document);
		}
	} catch (const invalid_argument & exception) {
		cout << "Ошибка в поисковом запросе. Причина: "s << exception.what() << endl;
	} catch (...) {
		cout << "Неизвестная ошибка."s << endl;
	}
	cout << "Even ids:"s << endl;
	try {
		for (const Document& document : search_server.FindTopDocuments("пушистый ухоженный кот"s, [](int document_id, DocumentStatus status, int rating) { return document_id % 2 == 0; (void)status; (void)rating;})) {
			PrintDocument(document);
		}
	} catch (const invalid_argument & exception) {
		cout << "Ошибка в поисковом запросе. Причина: "s << exception.what() << endl;
	} catch (...) {
		cout << "Неизвестная ошибка."s << endl;
	}

	return 0;
}

/*
ACTUAL by default:
{ document_id = 1, relevance = 0.866434, rating = 5 }
{ document_id = 0, relevance = 0.173287, rating = 2 }
{ document_id = 2, relevance = 0.173287, rating = -1 }
BANNED:
{ document_id = 3, relevance = 0.231049, rating = 9 }
Even ids:
{ document_id = 0, relevance = 0.173287, rating = 2 }
{ document_id = 2, relevance = 0.173287, rating = -1 }
*/

