#pragma once

#include <string>
#include <iostream>

enum class DocumentStatus {
	ACTUAL,
	IRRELEVANT,
	BANNED,
	REMOVED
};

std::string StatusAsString(DocumentStatus status);

struct Document {
	Document() = default;
	Document(int id, double relevance, int rating);

	int id = 0;
	double relevance = 0;
	int rating = 0;
};

std::ostream & operator << (std::ostream & out, const Document & document);
