#pragma once

#include "search_server.h"

void AddDocument(SearchServer & server, int document_id, const std::string & document,
	DocumentStatus status, const std::vector<int> & ratings);
void MatchDocuments(const SearchServer & server, std::string query, int id);
void FindTopDocuments(const SearchServer & server, std::string query);
