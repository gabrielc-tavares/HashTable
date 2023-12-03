/**
 * @file hash_table.hpp
 * @brief Hash table implementation using Hopscotch Hashing.
 */

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

/**
 * @namespace ht
 * @brief This namespace contains the implementation of a hash table template class.
 *
 * The 'ht' namespace includes the 'HashTable' class, which provides a hash table implementation
 * using hopscotch hashing. It supports various operations such as insertion, removal, and retrieval
 * of key-value pairs. The hash table dynamically resizes itself to maintain an optimal load factor.
 */
namespace ht {
	constexpr double MAX_LOAD_FACTOR = 0.75;  ///< Default maximum load factor for the hash table.
	constexpr double MIN_LOAD_FACTOR = 0.25;  ///< Default minimum load factor for the hash table.
	constexpr int NBHD_SIZE = 32;  ///< Size of bucket neighborhoods (referred to as "H" in the original paper on Hopscotch Hashing).
	constexpr int INITIAL_CPTY = NBHD_SIZE;  ///< Initial hash table capacity.

	/**
	 * @class HashTableException
	 * @brief Exception class for hash table-related errors.
	 */
	class HashTableException : public std::runtime_error {
	public:
		/**
		 * @brief Constructor with an error message.
		 * @param message The error message.
		 */
		HashTableException(const std::string& message) 
			: std::runtime_error(message) {}
	};

	/**
	 * @class InvalidLoadFactors
	 * @brief Exception class for invalid load factor values.
	 */
	class InvalidLoadFactors : public HashTableException {
	public:
		/**
		 * @brief Constructor with the specified range of invalid load factors.
		 * @param min The minimum load factor.
		 * @param max The maximum load factor.
		 */
		InvalidLoadFactors(double min, double max)
			: HashTableException(std::format("Invalid load factors: min = {}, max = {}", min, max)) {}
	};

	/**
	 * @class InvalidKeyType
	 * @brief Exception class for usage of invalid key types with default hash function.
	 */
	class InvalidKeyType : public HashTableException {
	public:
		/**
		 * @brief Constructor for the invalid key type exception.
		 */
		InvalidKeyType()
			: HashTableException("Invalid key type for default hash function (it must be contiguously allocated)") {}
	};

	/**
	 * @ResizeFailed
	 * @brief Exception class for hash table resizing failures.
	 */
	class ResizeFailed : public HashTableException {
	public:
		/**
		 * @brief Constructor for the resizing failure exception.
		 */
		ResizeFailed()
			: HashTableException("Failed to resize hash table") {}
	};

	/**
	 * @InsertionFailed
	 * @brief Exception class for hash table insertion failures.
	 */
	class InsertionFailed : public HashTableException {
	public:
		/**
		* @brief Constructor for the insertion failure exception.
		*/
		InsertionFailed()
			: HashTableException("Failed to insert an item in hash table") {}
	};

	/**
	 * @class HashTable
	 * @brief Hash table implementation using Hopscotch Hashing.
	 * @tparam Key The key type.
	 * @tparam Value The value type.
	 */
	template<typename Key, typename Value>
	class HashTable {
	public:
		/**
		 * @typedef HashFunction
		 * @brief Custom hash function interface.
		 */
		using HashFunction = std::function<size_t(const Key&)>;  // Custom hash function interface

		/**
		 * @brief Constructor for the hash table.
		 * @param _customHashFn Custom hash function.
		 * @param _minLoadFactor Minimum load factor.
		 * @param _maxLoadFactor Maximum load factor.
		 * @param expectedSize Expected initial size of the hash table.
		 */
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

		/**
		 * @brief Copy constructor for the HashTable.
		 * @param obj The HashTable to be copied.
		 */
		HashTable(const HashTable<Key, Value>& obj) 
			: size(obj.size), customHashFn(obj.customHashFn), capacity(obj.capacity), minLoadFactor(obj.minLoadFactor), maxLoadFactor(obj.maxLoadFactor), table(obj.table.begin(), obj.table.end()) {}

		/**
		 * @brief Set the maximum and minimum load factors for the HashTable.
		 * @param max The maximum load factor.
		 * @param min The minimum load factor.
		 * @throws InvalidLoadFactors Thrown if the provided load factors are invalid.
		 */
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

		/**
		 * @brief Get the maximum load factor.
		 * @return The maximum load factor.
		 */
		double getMaxLoadFactor() const { 
			return maxLoadFactor; 
		}

		/**
		 * @brief Get the minimum load factor.
		 * @return The minimum load factor.
		 */
		double getMinLoadFactor() const { 
			return minLoadFactor; 
		}

		/**
		 * @brief Check if the HashTable contains an item with key k.
		 * @param k The key to check.
		 * @return True if the key is found, false otherwise.
		 */
		bool contains(const Key& k) const {
			size_t h = hash(k, capacity), i = 0;

			do {
				if (table[(h + i) % capacity] != nullptr && table[(h + i) % capacity]->first == k)
					return true;
				i++;
			} while (i < NBHD_SIZE);

			return false;
		}

		/**
		 * @brief Get the value associated with the key k.
		 * @param k The key to search for.
		 * @return The value if found, std::nullopt otherwise.
		 */
		std::optional<Value> getValue(const Key& k) const {
			size_t h = hash(k, capacity), i = 0;

			do {
				if (table[(h + i) % capacity] != nullptr && table[(h + i) % capacity]->first == k)
					return std::make_optional<Value>(table[(h + i) % capacity]->second);
				i++;
			} while (i < NBHD_SIZE);

			return std::nullopt;
		}

		/**
		 * @brief Get the key-value pair associated with the key k.
		 * @param k The key to search for.
		 * @return The key-value pair if found, std::nullopt otherwise.
		 */
		std::optional<std::pair<Key, Value>> getItem(const Key& k) const {
			size_t h = hash(k, capacity), i = 0;

			do {
				if (table[(h + i) % capacity] != nullptr && table[(h + i) % capacity]->first == k)
					return std::make_optional<std::pair<Key, Value>>(*table[(h + i) % capacity]);
				i++;
			} while (i < NBHD_SIZE);

			return std::nullopt;
		}

		/**
		 * @brief Insert a new key-value pair into the HashTable.
		 * @param k The key.
		 * @param v The value.
		 * @return True if the insertion was successful, false if the key already exists.
		 * @throws ResizeFailed Thrown if resizing the table fails.
		 */
		bool insert(const Key& k, const Value& v) {
			size_t h = hash(k, capacity), i = 0;

			// Check if there is any empty bucket within h neighborhood
			do {
				// If an empty bucket was found in h neighborhood, insert new key-value pair on it
				if (table[(h + i) % capacity] == nullptr) {
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
				if (table[(h + i) % capacity] == nullptr) {
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

		/**
		 * @brief Remove the item with key k from the HashTable.
		 * @param k The key to remove.
		 * @return The value of the removed item if successful, std::nullopt otherwise.
		 */
		std::optional<Value> remove(const Key& k) {
			size_t h = hash(k, capacity), i = 0;

			do {
				if (table[(h + i) % capacity] != nullptr && table[(h + i) % capacity]->first == k) {
					// Assign return value with item value
					std::optional<Value> r = table[(h + i) % capacity]->second;
					// Empty bucket
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

		/**
		 * @brief Get a vector with all the values in the HashTable.
		 * @return A vector containing all the values.
		 */
		std::vector<Value> getAll() const {
			std::vector<Value> v;
			for (Bucket bucket : table) {
				if (bucket) 
					v.emplace_back(bucket->second);
			}
			return v;
		}

		/**
		 * @brief Check if the HashTable is empty.
		 * @return True if the HashTable is empty, false otherwise.
		 */
		bool isEmpty() const { return size == 0; }

		/**
		 * @brief Overload the [] operator for key-based lookup and insertion.
		 * @tparam T The value type.
		 * @param key The key for lookup or insertion.
		 * @return A reference to the value associated with the key.
		 */
		template <typename T = Value>
		typename std::enable_if<std::is_default_constructible<T>::value, T&>::type operator[](const Key& key) {
			Bucket bucket = getBucket(key);
			if (bucket != nullptr)
				return bucket->second;
			insert(key, T()); // Default-construct a value of type T
			return getBucket(key)->second;
		}

		/**
		 * @brief Copy assignment operator for the HashTable.
		 * @param obj The HashTable to be copied.
		 */
		void operator=(const HashTable& obj) {
			capacity = obj.capacity;
			size = obj.size;
			maxLoadFactor = obj.maxLoadFactor;
			minLoadFactor = obj.minLoadFactor;
			customHashFn = obj.customHashFn;

			table.clear();
			table.resize(capacity);

			for (size_t i = 0; i < capacity; i++) 
				if (obj.table[i] != nullptr)
					table[i] = std::make_shared<std::pair<Key, Value>>(*obj.table[i]);
		}

	private:
		/**
		 * @brief Alias for a shared pointer to a key-value pair.
		 */
		using Bucket = std::shared_ptr<std::pair<Key, Value>>;

		std::vector<Bucket> table; ///< The underlying table storing the key-value pairs.
		size_t capacity; ///< Total number of buckets in the hash table.
		size_t size; ///< Number of occupied buckets in the hash table.
		double maxLoadFactor; ///< Maximum load factor (size/capacity) for the hash table.
		double minLoadFactor; ///< Minimum load factor (size/capacity) for the hash table.
		HashFunction customHashFn; ///< Custom hash function defined by the client.

		/**
		 * @brief Check if a given number is a prime.
		 * @param n The number to check.
		 * @return True if the number is prime, false otherwise.
		 */
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

		/**
		 * @brief Calculate the hash code for a key k.
		 * @param k The key.
		 * @param range The range for the hash code.
		 * @return The calculated hash code.
		 */
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

		/**
		 * @brief Resize and rehash the hash table.
		 * @param newCapacity The new capacity for the hash table.
		 */
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

		/**
		 * @brief Search for the bucket that points to the key-value pair with key k.
		 * @param k The key to search for.
		 * @return The bucket if found, nullptr otherwise.
		 */
		Bucket getBucket(const Key& k) const {
			size_t h = hash(k, capacity), i = 0;

			do {
				if (table[(h + i) % capacity] && table[(h + i) % capacity]->first == k)
					return table[(h + i) % capacity];
				i++;
			} while (i < NBHD_SIZE);

			return nullptr;
		}

		/**
		 * @brief Check if the maximum load factor has been exceeded.
		 * @return True if the maximum load factor is exceeded, false otherwise.
		 */
		bool maxLoadFactorExceeded() const { 
			return (static_cast<double>(size) / capacity) > maxLoadFactor; 
		}

		/**
		 * @brief Check if the minimum load factor has been exceeded.
		 * @return True if the minimum load factor is exceeded, false otherwise.
		 */
		bool minLoadFactorExceeded() const { 
			return (static_cast<double>(size) / capacity) < minLoadFactor; 
		}

		/**
		 * @brief Clears the HashTable by resetting all buckets to nullptr.
		 *
		 * This method iterates through all the buckets in the HashTable
		 * and resets each bucket to nullptr, effectively clearing the table.
		 * The size of the HashTable is not modified.
		 */
		void clearTable() {
			for (Bucket bucket : table)
				if (bucket != nullptr)
					bucket.reset();
		}
	};
}