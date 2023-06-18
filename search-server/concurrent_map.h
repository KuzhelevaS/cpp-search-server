#pragma once

#include <deque>
#include <map>
#include <mutex>

using namespace std::string_literals;

template <typename Key, typename Value>
class ConcurrentMap {
private:
	struct Bucket {
		std::mutex mutex_;
		std::map<Key, Value> map_;
	};
public:
	static_assert(std::is_integral_v<Key>, "ConcurrentMap supports only integer keys"s);

	struct Access {
		Access(Bucket & bucket, const Key & key)
			: guard(std::lock_guard<std::mutex> (bucket.mutex_)),
			ref_to_value(bucket.map_[key]) {}

		const std::lock_guard<std::mutex> guard;
		Value& ref_to_value;
	};

	explicit ConcurrentMap(size_t bucket_count) : storage_(bucket_count) {}

	Access operator[](const Key& key) {
		size_t index = getIndex(key);
		return {storage_[index], key};
	}

	std::map<Key, Value> BuildOrdinaryMap() {
		std::map<Key, Value> result;
		for (size_t i = 0; i < storage_.size(); ++i) {
			std::lock_guard<std::mutex> guard (storage_[i].mutex_);
			result.merge(storage_[i].map_);
		}
		return result;
	}

	void Erase(const Key& key) {
		size_t index = getIndex(key);
		std::lock_guard<std::mutex> guard(storage_[index].mutex_);
		storage_[index].map_.erase(key);
	}

private:
	std::vector<Bucket> storage_;

	size_t getIndex(const Key& key) {
		return static_cast<uint64_t>(key) % storage_.size();
	}
};
