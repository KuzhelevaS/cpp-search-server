#include "document.h"

using std::literals::string_literals::operator""s;

std::string StatusAsString(DocumentStatus status) {
	switch (status) {
		case DocumentStatus::ACTUAL :
			return "ACTUAL"s;
		case DocumentStatus::IRRELEVANT :
			return "IRRELEVANT"s;
		case DocumentStatus::BANNED :
			return "BANNED"s;
		case DocumentStatus::REMOVED :
			return "REMOVED"s;
		default:
			return ""s;
	}
}

Document::Document(int id, double relevance, int rating)
	: id(id), relevance(relevance), rating(rating) {}

std::ostream & operator << (std::ostream & out, const Document & document) {
	out << "{ "s
		<< "document_id = "s << document.id << ", "s
		<< "relevance = "s << document.relevance << ", "s
		<< "rating = "s << document.rating
		<< " }"s;
	return out;
}
