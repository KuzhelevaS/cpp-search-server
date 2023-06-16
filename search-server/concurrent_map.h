#pragma once

#include <deque>
#include <map>
#include <mutex>

using namespace std::string_literals;

template <typename Key, typename Value>
class ConcurrentMap {
public:
	static_assert(std::is_integral_v<Key>, "ConcurrentMap supports only integer keys"s);

	struct Access {
		Access(std::map<Key, Value> & submap, const Key & key, size_t index, std::deque<std::mutex> & mutexes)
		: guard(std::lock_guard<std::mutex> (mutexes[index])), ref_to_value(submap[key]) {
		}
		const std::lock_guard<std::mutex> guard;
		Value& ref_to_value;
	};

	explicit ConcurrentMap(size_t bucket_count) {
		storage_.resize(bucket_count);
		mutexes_.resize(bucket_count);
	}

	Access operator[](const Key& key) {
		size_t index = getIndex(key);
		return {storage_[index], key, index, mutexes_};
	}

	std::map<Key, Value> BuildOrdinaryMap() {
		std::map<Key, Value> result;
		for (size_t i = 0; i < storage_.size(); ++i) {
			std::lock_guard<std::mutex> guard (mutexes_[i]);
			result.merge(storage_[i]);
		}
		return result;
	}

	void Erase(const Key& key) {
		size_t index = getIndex(key);
		std::lock_guard<std::mutex> guard(mutexes_[index]);
		storage_[index].erase(key);
	}

private:
	std::deque<std::map<Key, Value>> storage_;
	std::deque<std::mutex> mutexes_;

	size_t getIndex(const Key& key) {
		return static_cast<uint64_t>(key) % storage_.size();
	}
};

