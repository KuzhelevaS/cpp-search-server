#include "read_input_functions.h"

#include <iostream>
#include <string>

std::string ReadLine() {
	std::string query;
	getline(std::cin, query);
	return query;
}

int ReadLineWithNumber() {
	int result = 0;
	std::cin >> result;
	ReadLine();
	return result;
}
