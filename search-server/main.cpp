#include <iostream>
#include <string>
#include <vector>
#include <set>
#include <algorithm>
#include <map>
#include <cmath>

using namespace std;

const int MAX_RESULT_DOCUMENT_COUNT = 5;

struct Document {
    int id;
    double relevance;
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
        void AddDocument(int document_id, const std::string & document) {
            const std::vector<std::string> words = SplitIntoWordsNoStop(document);
            for(const auto & w : words) {
                documents_with_tf_[w][document_id] += 1.0 / words.size();
            }
            document_count_++;
        }

        void SetStopWords(const string& text) {
            for (const string& word : SplitIntoWords(text)) {
                stop_words_.insert(word);
            }
        }

        vector<Document> FindTopDocuments(const string& raw_query) const {
            const Query query_words = ParseQuery(raw_query);
            vector<Document> result = FindAllDocuments(query_words);

            sort(result.begin(), result.end(),
                [](const Document & lhs, const Document & rhs) {
                    return lhs.relevance > rhs.relevance;
                });

            if (result.size() > MAX_RESULT_DOCUMENT_COUNT) {
                result.resize(MAX_RESULT_DOCUMENT_COUNT);
            }

            return result;
        }
    private:
        map<string, map<int, double>> documents_with_tf_; 
        std::set<std::string> stop_words_;
        int document_count_ = 0;

        struct Query {
            std::set<string> plus_words;
            std::set<string> minus_words;
        };

        vector<string> SplitIntoWordsNoStop(const string& text) const {
            vector<string> words;
            for (const string& word : SplitIntoWords(text)) {
                if (stop_words_.count(word) == 0) {
                    words.push_back(word);
                }
            }
            return words;
        }

        Query ParseQuery(const string& text) const {
            Query query;
            for (const string& word : SplitIntoWordsNoStop(text)) {
                if (word[0] == '-') {
                    query.minus_words.insert(word.substr(1));
                } else {
                    query.plus_words.insert(word);
                }
            }
            return query;
        }

        vector<Document> FindAllDocuments(const Query& query_words) const {
            map<int, double> matched_documents;
            for(const auto & plus : query_words.plus_words) {
                // делаем проверку, чтобы вызвать documents_with_tf_.at()
                if (documents_with_tf_.count(plus) == 0) {
                    continue;
                }
                double idf = log(static_cast<double>(document_count_) / documents_with_tf_.at(plus).size());
                for (const auto & [doc_id, tf] : documents_with_tf_.at(plus)) {
                    matched_documents[doc_id] += idf * tf;
                }
            }
            for(const auto & minus : query_words.minus_words) {
                // делаем проверку, чтобы вызвать documents_with_tf_.at()
                if (documents_with_tf_.count(minus) == 0) {
                    continue;
                }
                for (const auto & [doc_id, tf] : documents_with_tf_.at(minus)) {
                    matched_documents.erase(doc_id);
                }
            }
            vector<Document> result;
            for (const auto & [id, rel] : matched_documents) {
                result.push_back({id, rel});
            }
            return result;
        }

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
};

SearchServer CreateSearchServer() {
    SearchServer s;
    s.SetStopWords(ReadLine());
    const int document_count = ReadLineWithNumber();
    for (int document_id = 0; document_id < document_count; ++document_id) {
        s.AddDocument(document_id, ReadLine());
    }
    return s;
}

int main() {

    const SearchServer searchingPetsServer = CreateSearchServer();

    const string query = ReadLine();
    for (auto [document_id, relevance] : searchingPetsServer.FindTopDocuments(query)) {
        cout << "{ document_id = "s << document_id << ", relevance = "s << relevance << " }"s << endl;
    }

}

