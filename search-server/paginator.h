#pragma once

#include <iostream>
#include <vector>

template <typename Iterator>
class IteratorRange {
public:
	IteratorRange(Iterator begin, Iterator end, unsigned size)
		: begin_(begin), end_(end), size_(size) {}

	Iterator begin() const {
		return begin_;
	}

	Iterator end() const {
		return end_;
	}

	unsigned size() const {
		return size_;
	}

private:
	Iterator begin_;
	Iterator end_;
	unsigned size_;
};

template <typename Iterator>
std::ostream & operator << (std::ostream & out, IteratorRange<Iterator> iterator) {
	for (auto current = iterator.begin(); current != iterator.end(); advance(current, 1)) {
		out << *current;
	}
	return out;
}

template <typename Iterator>
class Paginator {
public:
	Paginator(Iterator begin_iterator, Iterator end_iterator, int page_size) {
		auto current_iterator = begin_iterator;
		while(true) {
			int remains = distance(current_iterator, end_iterator);
			if (remains <= 0) {
				break;
			} else if (remains <= page_size) {
				pagination.push_back(IteratorRange<Iterator>(current_iterator,
					end_iterator, static_cast<unsigned>(remains)));
				break;
			} else {
				auto next_iterator = next(current_iterator, page_size);
				pagination.push_back(IteratorRange<Iterator>(current_iterator,
					next_iterator, static_cast<unsigned>(page_size)));
				current_iterator = next_iterator;
			}
		}
	}

	auto begin() const {
		return pagination.begin();
	}

	auto end() const {
		return pagination.end();
	}

	auto size() const {
		return pagination.size();
	}

private:
	std::vector<IteratorRange<Iterator>> pagination;
};

template <typename Container>
auto Paginate(const Container & c, size_t page_size) {
    return Paginator(begin(c), end(c), page_size);
}
