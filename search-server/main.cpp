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

void PrintDocument(const Document& document) {
    cout << "{ "s
        << "document_id = "s << document.id << ", "s
        << "relevance = "s << document.relevance << ", "s
        << "rating = "s << document.rating
        << " }"s << endl;
}

int main() {
    SearchServer search_server;
    search_server.SetStopWords("и в на"s);
    search_server.AddDocument(0, "белый кот и модный ошейник"s,        DocumentStatus::ACTUAL, {8, -3});
    search_server.AddDocument(1, "пушистый кот пушистый хвост"s,       DocumentStatus::ACTUAL, {7, 2, 7});
    search_server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, {5, -12, 2, 1});
    search_server.AddDocument(3, "ухоженный скворец евгений"s,         DocumentStatus::BANNED, {9});
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
