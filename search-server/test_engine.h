#pragma once

#include <string>
#include <vector>
#include <set>
#include <map>
#include <iostream>

using std::literals::string_literals::operator""s;

template <typename Container>
void Print(std::ostream & out, const Container & container);

template <typename T>
std::ostream & operator << (std::ostream & out, const std::vector<T> & v);

template <typename T>
std::ostream & operator << (std::ostream & out, const std::set<T> & s);

template <typename Key, typename Value>
std::ostream & operator << (std::ostream & out, const std::map<Key,Value> & m);

template <typename Key, typename Value>
std::ostream & operator << (std::ostream & out, const std::pair<Key,Value> & p);


template <typename T, typename U>
void AssertEqualImpl(const T& t, const U& u, const std::string& t_str,
	const std::string& u_str, const std::string& file,
	const std::string& func, unsigned line, const std::string& hint);

#define ASSERT_EQUAL(a, b) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, ""s)
#define ASSERT_EQUAL_HINT(a, b, hint) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, (hint))

void AssertImpl(bool value, const std::string& expr, const std::string& file,
	unsigned line, const std::string& func, const std::string& hint = ""s);

#define ASSERT(expr) AssertImpl(!!(expr), #expr, __FILE__, __LINE__, __FUNCTION__)
#define ASSERT_HINT(expr, hint) AssertImpl(!!(expr), #expr, __FILE__, __LINE__, __FUNCTION__, hint)

template <typename Function>
void RunTestImpl(Function function, const std::string & function_name);

#define RUN_TEST(func)  RunTestImpl((func), #func)


template <typename T, typename U>
void AssertEqualImpl(const T& t, const U& u, const std::string& t_str,
	const std::string& u_str, const std::string& file,
	const std::string& func, unsigned line, const std::string& hint)
{
	if (t != u) {
		std::cerr << std::boolalpha;
		std::cerr << file << "("s << line << "): "s << func << ": "s;
		std::cerr << "ASSERT_EQUAL("s << t_str << ", "s << u_str << ") failed: "s;
		std::cerr << t << " != "s << u << "."s;
		if (!hint.empty()) {
			std::cerr << " Hint: "s << hint;
		}
		std::cerr << std::endl;
		abort();
	}
}

template <typename Function>
void RunTestImpl(Function function, const std::string & function_name) {
	function();
	std::cerr << function_name << " OK" << std::endl;
}

template <typename Container>
void Print(std::ostream & out, const Container & container) {
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
std::ostream & operator << (std::ostream & out, const std::vector<T> & v) {
	out << "["s;
	Print(out, v);
	out << "]"s;
	return out;
}

template <typename T>
std::ostream & operator << (std::ostream & out, const std::set<T> & s) {
	out << "{"s;
	Print(out, s);
	out << "}"s;
	return out;
}

template <typename Key, typename Value>
std::ostream & operator << (std::ostream & out, const std::map<Key,Value> & m) {
	out << "{"s;
	Print(out, m);
	return out;
}

template <typename Key, typename Value, typename Less>
std::ostream & operator << (std::ostream & out, const std::map<Key,Value,Less> & m) {
	out << "{"s;
	Print(out, m);
	out << "}"s;
	return out;
}

template <typename Key, typename Value>
std::ostream & operator << (std::ostream & out, const std::pair<Key,Value> & p) {
	out << p.first << ": "s << p.second;
	return out;
}
