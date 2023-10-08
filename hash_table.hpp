#pragma once

#include <iostream>
#include <memory>
#include <vector>
#include <string>
#include <utility>
#include <optional>
#include <functional>
#include <type_traits>

namespace ht {
	constexpr double DEFAULT_MAX_LOAD_FACTOR = 0.8;
	constexpr double DEFAULT_MIN_LOAD_FACTOR = DEFAULT_MAX_LOAD_FACTOR / 4;

	constexpr size_t DEFAULT_INITIAL_CPTY = 3;

	// Smallest prime number greater than the size of the 
	// input alphabet (considering all UTF-8 character codes)
	constexpr size_t PRIME_GT_ALPHABET_SIZE = 257;

	template<typename Key, typename Value>
	class HashTable {
	public:
		// Pointer to a (key, value) pair
		using Slot_t = std::shared_ptr<std::pair<Key, Value>>;
		// Collection of slots
		using Map_t = std::vector<Slot_t>;
		// Hash function interface
		using HashFunction = std::function<size_t(const Key&)>;

		// Default constructor
		HashTable(HashFunction _customHashFn = nullptr) :
			size(0),
			capacity(DEFAULT_INITIAL_CPTY),
			maxLoadFactor(DEFAULT_MAX_LOAD_FACTOR),
			minLoadFactor(DEFAULT_MIN_LOAD_FACTOR),
			map(DEFAULT_INITIAL_CPTY, nullptr),
			customHashFn(_customHashFn)
		{
			if (customHashFn == nullptr) {
				if (!(std::is_fundamental_v<Key> || std::is_pointer_v<Key> || std::is_array_v<Key>)) {
					throw std::invalid_argument("The default hash function accepts only contiguously allocated keys.");
				}
			}
		}
		// Constructor with user-defined load factors
		HashTable(double _maxLoadFactor, double _minLoadFactor, HashFunction _customHashFn = nullptr) :
			size(0),
			capacity(DEFAULT_INITIAL_CPTY),
			maxLoadFactor(_maxLoadFactor),
			minLoadFactor(_minLoadFactor),
			map(DEFAULT_INITIAL_CPTY, nullptr),
			customHashFn(_customHashFn)
		{
			if (customHashFn == nullptr) {
				if (!(std::is_fundamental_v<Key> || std::is_pointer_v<Key> || std::is_array_v<Key>)) {
					throw std::invalid_argument("The default hash function accepts only contiguously allocated keys.");
				}
			}
		}
		// Constructor with user-defined load factors and capacity
		HashTable(double _maxLoadFactor, double _minLoadFactor, size_t _capacity, HashFunction _customHashFn = nullptr) :
			size(0),
			capacity(_capacity),
			maxLoadFactor(_maxLoadFactor),
			minLoadFactor(_minLoadFactor),
			map(_capacity, nullptr),
			customHashFn(_customHashFn)
		{
			if (capacity == 0) {
				throw std::invalid_argument("The hash table capacity must not be 0");
			}
			if (customHashFn == nullptr) {
				if (!(std::is_fundamental_v<Key> || std::is_pointer_v<Key> || std::is_array_v<Key>)) {
					// If there is no custom hash function defined and 'Key' is not contiguously allocated,
					// an exception is thrown
					throw std::invalid_argument("The default hash function accepts only contiguously allocated keys.");
				}
			}
		}
		// Copy constructor
		HashTable(const HashTable<Key, Value>& obj) :
			size(obj.size),
			capacity(obj.capacity),
			maxLoadFactor(obj.maxLoadFactor),
			minLoadFactor(obj.minLoadFactor),
			map(obj.map.begin(), obj.map.end()),
			customHashFn(obj.customHashFn)
		{}

		// Set upper and lower thresholds to the load factor
		void setLoadFactors(double max, double min) {
			if (min <= 0.0 || min > 1.0 || max <= 0.0 || max > 1.0 || max <= min) {
				throw std::invalid_argument("Invalid load factors");
			}
			minLoadFactor = min;
			maxLoadFactor = max;

			if (maxLoadFactorExceeded()) {
				rehash(2 * capacity);
			}
			else if (minLoadFactorExceeded()) {
				rehash((size_t)ceil(capacity / 2.0));
			}
		}
		// Get upper limit defined for hash table load factor 
		double getMaxLoadFactor() const {
			return maxLoadFactor;
		}
		// Get lower limit defined for hash table load factor 
		double getMinLoadFactor() const {
			return minLoadFactor;
		}

		// Update map capacity with newCapacity and rehash
		void rehash(size_t newCapacity) {
			if (newCapacity < size) {
				throw std::invalid_argument("The map capacity shouldn't be smaller than its size.");
			}
			size_t oldCapacity = capacity;
			capacity = newCapacity;

			Map_t temp;
			temp.resize(capacity);

			// Transfer non-empty slots from the current map to temp
			for (size_t i = 0; i < oldCapacity; i++) {
				if (map[i]) {
					size_t h = hash(map[i]->first, capacity);

					if (!temp[h]) {
						temp[h] = map[i];
					}
					else {
						size_t h2 = (h + 1) % capacity;

						do {
							if (!temp[h2]) {
								temp[h2] = map[i];
								break;
							}
							h2 = (h2 + 1) % capacity;
						} while (h2 != h);

						if (h2 == h)
							throw std::exception("Error rehashing HashTable.");
					}
				}
			}
			// Swap the contents of the current map with the temporary map
			map.swap(temp);
		}

		// Search for value paired with key 'k'
		// If found, returns it; otherwise, returns a std::nullopt
		std::optional<Value> search(const Key& k) const {
			size_t h = hash(k, capacity);

			if (!map[h]) {
				return std::nullopt; // Key not found
			}
			if (map[h]->first == k) {
				// Return slot value wrapped on a std::optional class object
				std::optional<Value> r = map[h]->second;
				return r;
			}
			size_t h2 = (h + 1) % capacity;

			do {
				if (!map[h2]) {
					return std::nullopt; // Key not found
				}
				if (map[h2]->first == k) {
					// Return slot value wrapped on a std::optional class object
					std::optional<Value> r = map[h2]->second;
					return r;
				}
				h2 = (h2 + 1) % capacity;
			} while (h2 != h);

			return std::nullopt;
		}

		// Insert new (key, value) pair in the hash table
		// Returns true if insertion was effectively done; otherwise, returns false
		bool insert(const Key& k, const Value& v) {
			size_t h = hash(k, capacity);

			if (!map[h]) {
				// Insert new pair
				map[h] = std::make_shared<std::pair<Key, Value>>(k, v);
				size++;
				// Check if load factor exceeded its limit and rehash if necessary
				if (maxLoadFactorExceeded()) {
					rehash(2 * capacity);
				}
				return true; // Insertion done successfully
			}
			if (map[h]->first == k)
				// Don't insert duplicates
				return false;

			size_t h2 = (h + 1) % capacity;

			do {
				if (!map[h2]) {
					// Insert new pair
					map[h2] = std::make_shared<std::pair<Key, Value>>(k, v);
					size++;
					// Check if load factor exceeded its limit and rehash if necessary
					if (maxLoadFactorExceeded()) {
						rehash(2 * capacity);
					}
					return true; // Insertion done successfully
				}
				if (map[h2]->first == k) {
					// Don't insert duplicates
					return false;
				}
				h2 = (h2 + 1) % capacity;
			} while (h2 != h);

			throw std::exception("Error inserting value into HashTable.");
		}

		// Search for slot with key 'k' in the hash table and, if found, 
		// remove it and returns its value; otherwise, returns a std::nullopt
		std::optional<Value> remove(const Key& k) {
			size_t h = hash(k, capacity);

			if (!map[h]) {
				return std::nullopt; // Key not found
			}
			if (map[h]->first == k) {
				// Return value wrapped on a std::optional class object
				std::optional<Value> r = map[h]->second;
				// Empty slot
				map[h].reset();
				size--;
				// Check if load factor exceeded its limit and rehash if necessary
				if (minLoadFactorExceeded() && capacity > 1) {
					rehash((size_t)ceil(capacity / 2.0));
				}
				return r; // Return removed slot value
			}
			size_t h2 = (h + 1) & capacity;

			do {
				if (!map[h2]) {
					return std::nullopt; // Key not found
				}
				if (map[h2]->first == k) {
					// Return value wrapped on a std::optional class object
					std::optional<Value> r = map[h2]->second;
					// Empty slot
					map[h2].reset();
					size--;
					// Check if load factor exceeded its limit and rehash if necessary
					if (minLoadFactorExceeded() && capacity > 1) {
						rehash((size_t)ceil(capacity / 2.0));
					}
					return r; // Return removed slot value
				}
				h2 = (h2 + 1) % capacity;
			} while (h2 != h);

			return std::nullopt;
		}

		std::vector<Value> getAll() const {
			std::vector<Value> result;

			for (Slot_t slot : map) {
				if (slot)
					result.emplace_back(slot->second);
			}
			return result;
		}

		// Check if the hash table is empty
		bool isEmpty() const {
			return size == 0;
		}

	private:
		Map_t map;

		// Total number of slots
		size_t capacity;
		// Number of occupied slots
		size_t size;

		// Maximum size/capacity value
		double maxLoadFactor;
		// Minimum size/capacity value
		double minLoadFactor;

		// Custom hash function
		HashFunction customHashFn;

		// Check if 'n' is a prime number
		bool isPrime(size_t n) const {
			if (n <= 1) return false;
			if (n == 2 || n == 3) return true;
			if (n % 2 == 0 || n % 3 == 0) return false;
			if ((n - 1) % 6 != 0 && (n + 1) % 6 != 0) return false;
			for (size_t i = 5; i * i <= n; i += 6) {
				if (n % i == 0 || n % (i + 6) == 0) return false;
			}
			return true;
		}

		// Calculate the hash code of the key 'k'
		size_t hash(const Key& k, size_t range) const {
			// Use the custom hash function if provided; otherwise, use the default hash function
			auto hashFn = customHashFn ? customHashFn : [this, range](const Key& k) -> size_t {
				// 'ptr' points to the memory representation of 'k'
				const char* ptr = nullptr;
				ptr = reinterpret_cast<const char*>(&k);

				if (!ptr) {
					throw std::runtime_error("Invalid key type for default hash function.");
				}
				const size_t blockSize = sizeof(k);
				size_t primePow = PRIME_GT_INPUT_ALPH;
				size_t hashCode = 0;

				// Treat 'k' as a byte string and apply polynomial rolling hash
				for (size_t i = 0; i < blockSize; i++) {
					hashCode = (hashCode + (static_cast<size_t>(*(ptr + i)) + 1) * primePow) % range;
					primePow = (primePow * PRIME_GT_INPUT_ALPH) % range;
				}
				return hashCode;
				};
			// Calculate hash code using the selected hash function
			return hashFn(k) % range;
		}

		// Check if the load factor has reached its maximum value
		bool maxLoadFactorExceeded() const {
			return (static_cast<double>(size) / capacity) >= maxLoadFactor;
		}
		// Check if the load factor has reached its minimum value
		bool minLoadFactorExceeded() const {
			return (static_cast<double>(size) / capacity) < minLoadFactor;
		}
	};
}