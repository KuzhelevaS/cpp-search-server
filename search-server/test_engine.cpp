#include "test_engine.h"

void AssertImpl(bool value, const std::string& expr, const std::string& file,
	unsigned line, const std::string& func, const std::string& hint)
{
	if (!value) {
		std::cerr << file << "("s << line << "): "s << func << ": "s;
		std::cerr << "ASSERT("s << expr << ") failed."s;
		if (!hint.empty()) {
			std::cerr << " Hint: "s << hint;
		}
		std::cerr << std::endl;
		abort();
	}
}
