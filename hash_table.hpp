#pragma once

#include <iostream>
#include <memory>
#include <vector>
#include <string>
#include <utility>
#include <optional>
#include <functional>
#include <type_traits>
#include <stdexcept>
#include <format>

namespace ht {
	constexpr double MAX_LOAD_FACTOR = 0.75;  // Default maximum load factor 
	constexpr double MIN_LOAD_FACTOR = 0.25;  // Default minimum load factor
	constexpr int NBHD_SIZE = 32;  // Size of bucket neighborhoods (referred to as "H" in the original paper on Hopscotch Hashing)
	constexpr int INITIAL_CPTY = NBHD_SIZE;  // Initial hash table capacity

	class HashTableException : public std::runtime_error {
	public:
		HashTableException(const std::string& message) 
			: std::runtime_error(message) {}
	};

	class InvalidLoadFactors : public HashTableException {
	public:
		InvalidLoadFactors(double min, double max)
			: HashTableException(std::format("Invalid load factors: min = {}, max = {}.", min, max)) {}
	};

	class ResizeFailed : public HashTableException {
	public:
		ResizeFailed()
			: HashTableException("Failed to resize hash table") {}
	};

	class InsertionFailed : public HashTableException {
	public:
		InsertionFailed()
			: HashTableException("Failed to insert an item in hash table") {}
	};

	template<typename Key, typename Value>
	class HashTable {
	public:
		using HashFunction = std::function<size_t(const Key&)>;  // Custom hash function interface

		HashTable(HashFunction _customHashFn = nullptr, double _minLoadFactor = MIN_LOAD_FACTOR, double _maxLoadFactor = MAX_LOAD_FACTOR, size_t expectedSize = INITIAL_CPTY)
			: size(0), customHashFn(_customHashFn), minLoadFactor(_minLoadFactor), maxLoadFactor(_maxLoadFactor) {
			if (expectedSize < INITIAL_CPTY) {
				capacity = INITIAL_CPTY;
			} else {
				capacity = expectedSize;
			}
			table.resize(capacity);

			if (_customHashFn == nullptr && !(std::is_fundamental_v<Key> || std::is_pointer_v<Key> || std::is_array_v<Key>)) {
				throw std::exception("The default hash function accepts only contiguously allocated keys.");
			}
		}

		// Copy constructor
		HashTable(const HashTable<Key, Value>& obj) 
			: size(obj.size), customHashFn(obj.customHashFn), capacity(obj.capacity), minLoadFactor(obj.minLoadFactor), maxLoadFactor(obj.maxLoadFactor), table(obj.table.begin(), obj.table.end()) {}

		// Set maximum and minimum load factors
		void setLoadFactors(double max, double min) {
			if (min <= 0.0 || min > 1.0 || max <= 0.0 || max > 1.0 || max <= min) {
				throw InvalidLoadFactors(min, max);
			}
			minLoadFactor = min;
			maxLoadFactor = max;
			if (maxLoadFactorExceeded()) {
				resize(2 * capacity);
			} else if (minLoadFactorExceeded() && capacity > INITIAL_CPTY) {
				resize((capacity + 1) / 2);
			}
		}
		double getMaxLoadFactor() const { return maxLoadFactor; }
		double getMinLoadFactor() const { return minLoadFactor; }

		// Check if the hash table contains an item with key k
		bool contains(const Key& k) const {
			size_t h = hash(k, capacity), i = 0;

			do {
				if (table[(h + i) % capacity] && table[(h + i) % capacity]->first == k)
					return true;
				i++;
			} while (i < NBHD_SIZE);

			return false;
		}

		// Search for key-value pair with key k and returns its value if found
		// If not found, a std::nullopt is returned
		std::optional<Value> getValue(const Key& k) const {
			size_t h = hash(k, capacity), i = 0;

			do {
				if (table[(h + i) % capacity] && table[(h + i) % capacity]->first == k)
					return std::make_optional<Value>(table[(h + i) % capacity]->second);
				i++;
			} while (i < NBHD_SIZE);

			return std::nullopt;
		}

		// Search for key-value pair with key k and returns it if found
		// If not found, a std::nullopt is returned
		std::optional<std::pair<Key, Value>> getItem(const Key& k) const {
			size_t h = hash(k, capacity), i = 0;

			do {
				if (table[(h + i) % capacity] && table[(h + i) % capacity]->first == k)
					return std::make_optional<std::pair<Key, Value>>(*table[(h + i) % capacity]);
				i++;
			} while (i < NBHD_SIZE);

			return std::nullopt;
		}

		// Insert new key-value pair in the hash table
		// Returns true if insertion was effectively done; otherwise, returns false
		bool insert(const Key& k, const Value& v) {
			size_t h = hash(k, capacity), i = 0;

			// Check if there is any empty bucket within h neighborhood
			do {
				// If an empty bucket was found in h neighborhood, insert new key-value pair on it
				if (!table[(h + i) % capacity]) {
					table[(h + i) % capacity] = std::make_shared<std::pair<Key, Value>>(k, v);
					size++;
					if (maxLoadFactorExceeded()) {
						try {
							resize(2 * capacity);
						} catch (ResizeFailed& e) {
							std::cout << e.what() << "\n";
							throw;
						}
					}
					return true;
				} 
				if (table[(h + i) % capacity]->first == k) {
					// Don't insert duplicates
					return false;
				}
				i++;
			} while (i < NBHD_SIZE);

			// There is no empty bucket within h neighborhood
			// Try to free space by moving buckets in h neighborhood to another location
			while ((h + i) % capacity != h) {
				if (!table[(h + i) % capacity]) {
					size_t j = i;
					i -= NBHD_SIZE - 1;

					table[(h + j) % capacity] = table[(h + i) % capacity];
					table[(h + i) % capacity].reset();

					// If empty bucket is within h neighborhood, insert new key-value pair on it
					if (i < NBHD_SIZE) {
						table[(h + i) % capacity] = std::make_shared<std::pair<Key, Value>>(k, v);
						size++;
						if (maxLoadFactorExceeded()) {
							try {
								resize(2 * capacity);
							} catch (ResizeFailed& e) {
								std::cout << e.what() << "\n";
								throw;
							}
						}
						return true;
					}
					continue;
				}
				i++;
			}
			// Failed to insert new key-value pair
			throw InsertionFailed();
		}

		// If the hash table contains an item with key k, removes it and returns its value;
		// otherwise, returns a std::nullopt
		std::optional<Value> remove(const Key& k) {
			size_t h = hash(k, capacity), i = 0;

			do {
				if (table[(h + i) % capacity] && table[(h + i) % capacity]->first == k) {
					// Assign return value with item value and empty bucket
					std::optional<Value> r = table[(h + i) % capacity]->second;
					table[(h + i) % capacity].reset();
					size--;
					// Rehash if needed
					if (minLoadFactorExceeded() && capacity > INITIAL_CPTY) {
						resize((capacity + 1) / 2);
					}
					return r; // Return value from removed bucket
				}
				i++;
			} while (i < NBHD_SIZE);

			return std::nullopt;
		}

		// Get a vector with all the hash table items
		std::vector<Value> getAll() const {
			std::vector<Value> v;
			for (Bucket bucket : table) {
				if (bucket) 
					v.emplace_back(bucket->second);
			}
			return v;
		}

		// Check if the hash table is empty
		bool isEmpty() const { return size == 0; }

		// Overload the [] operator for key-based lookup and insertion
		template <typename T = Value>
		typename std::enable_if<std::is_default_constructible<T>::value, T&>::type operator[](const Key& key) {
			if (contains(key))
				return getBucket(key)->second;
			insert(key, T()); // Default-construct a value of type T
			return getBucket(key)->second;
		}

	private:
		using Bucket = std::shared_ptr<std::pair<Key, Value>>;

		std::vector<Bucket> table;
		size_t capacity; // Total number of buckets
		size_t size; // Number of occupied buckets
		double maxLoadFactor; // Maximum size/capacity value
		double minLoadFactor; // Minimum size/capacity value
		HashFunction customHashFn; // Custom hash function defined by the client

		// Check if n is a prime number
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

		// Calculate hash code for key k
		size_t hash(const Key& k, size_t range) const {
			// Use the custom hash function if provided; otherwise, use the default hash function
			auto hashFn = customHashFn ? customHashFn : [this, range](const Key& k) -> size_t {
				// "bytes" points to the memory representation of k
				const char* bytes = reinterpret_cast<const char*>(&k);
				if (!bytes) {
					throw std::runtime_error("Invalid key type for default hash function.");
				}
				const size_t blockSize = sizeof(k);
				const size_t p = 257;  // Smallest prime number greater than the alphabet size
				size_t primePow = p, hashCode = 0;

				// Treat k as a byte string and apply polynomial rolling hash
				for (size_t i = 0; i < blockSize; i++) {
					hashCode = (hashCode + (static_cast<size_t>(*(bytes + i)) + 1) * primePow) % range;
					primePow = (primePow * p) % range;
				}
				return hashCode;
			};
			// Calculate hash code using the selected hash function
			return hashFn(k) % range;
		}

		// Resize and rehash hash table
		void resize(size_t newCapacity) {
			size_t oldCapacity = capacity;
			std::vector<Bucket> temp;

			capacity = newCapacity;
			temp.resize(capacity);

			// Transfer non-empty buckets from the current table to temp
			for (size_t i = 0; i < oldCapacity; i++) {
				if (!table[i])
					continue;

				size_t h = hash(table[i]->first, capacity), j = 0;

				// Check if there is any empty bucket within h neighborhood
				do {
					// If an empty bucket was found in h neighborhood, insert new key-value pair on it
					if (!temp[(h + j) % capacity]) {
						temp[(h + j) % capacity] = table[i];
						break;
					}
					j++;
				} while (j < NBHD_SIZE);

				if (j < NBHD_SIZE)
					continue;

				// There is no empty bucket within h neighborhood
				// Try to free space by moving buckets in h neighborhood to another location
				while ((h + j) % capacity != h) {
					if (!temp[(h + j) % capacity]) {
						size_t k = j;
						j -= NBHD_SIZE - 1;

						temp[(h + k) % capacity] = temp[(h + j) % capacity];
						temp[(h + j) % capacity].reset();

						// If an empty bucket was found in h neighborhood, insert new key-value pair on it
						if (j < NBHD_SIZE) {
							temp[(h + j) % capacity] = table[i];
							break;
						}
						continue;
					}
					i++;
				}

				// Failed to rehash hash table
				if ((h + i) % capacity == h) {
					throw ResizeFailed();
				}
			}
			// Swap the contents of the current table with the temporary table
			table.swap(temp);
		}

		// Search for bucket that points to key-value pair with key k and returns its value if found
		// If not found, a nullptr is returned
		Bucket getBucket(const Key& k) const {
			size_t h = hash(k, capacity), i = 0;

			do {
				if (table[(h + i) % capacity] && table[(h + i) % capacity]->first == k)
					return table[(h + i) % capacity];
				i++;
			} while (i < NBHD_SIZE);

			return nullptr;
		}

		// Check if the load factor has reached its maximum value
		bool maxLoadFactorExceeded() const { return (static_cast<double>(size) / capacity) > maxLoadFactor; }
		// Check if the load factor has reached its minimum value
		bool minLoadFactorExceeded() const { return (static_cast<double>(size) / capacity) < minLoadFactor; }
	};
}