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
	// Default upper limit for the HashTable load factor 
	constexpr double MAX_LOAD_FACTOR = 0.8;
	// Default lower limit for the HashTable load factor 
	constexpr double MIN_LOAD_FACTOR = MAX_LOAD_FACTOR / 4;
	// Minimum capacity allowed for the HashTable
	constexpr size_t MIN_CPTY = 2;

	template<typename Key, typename Value>
	class HashTable {
	public:
		// Pointer to a (key, value) pair
		using Slot_t = std::shared_ptr<std::pair<Key, Value>>;
		// Collection of slots
		using Map_t = std::vector<Slot_t>;
		// Hash function interface
		using HashFunction = std::function<size_t(const Key&)>;

		HashTable(HashFunction customFn = nullptr, size_t initialCapacity = MIN_CPTY, double minLoadFactor = MIN_LOAD_FACTOR, double maxLoadFactor = MAX_LOAD_FACTOR)
			: _size(0), _customHashFn(customFn), _capacity(initialCapacity), _minLoadFactor(minLoadFactor), _maxLoadFactor(maxLoadFactor) {
			if (_capacity < MIN_CPTY) 
				throw std::invalid_argument("Invalid initial capacity.");
			if (_customHashFn == nullptr)
				if (!(std::is_fundamental_v<Key> || std::is_pointer_v<Key> || std::is_array_v<Key>))
					throw std::invalid_argument("The default hash function accepts only contiguously allocated keys.");
			map.resize(initialCapacity);
		}

		// Copy constructor
		HashTable(const HashTable<Key, Value>& obj) 
			: _size(obj._size), _customHashFn(obj._customHashFn), _capacity(obj._capacity), _minLoadFactor(obj._minLoadFactor), _maxLoadFactor(obj._maxLoadFactor), map(obj.map.begin(), obj.map.end()) {}

		// Set upper and lower thresholds to the load factor
		void setLoadFactors(double max, double min) {
			if (min <= 0.0 || min > 1.0 || max <= 0.0 || max > 1.0 || max <= min) {
				throw std::invalid_argument("Invalid load factors.");
			}
			_minLoadFactor = min;
			_maxLoadFactor = max;

			if (maxLoadFactorExceeded()) {
				rehash(2 * _capacity);
			}
			else if (minLoadFactorExceeded()) {
				rehash((size_t)ceil(_capacity / 2.0));
			}
		}

		// Get upper limit defined for hash table load factor 
		double getMaxLoadFactor() const {
			return _maxLoadFactor;
		}

		// Get lower limit defined for hash table load factor 
		double getMinLoadFactor() const {
			return _minLoadFactor;
		}

		// Update map capacity with newCapacity and rehash
		void rehash(size_t newCapacity) {
			if (newCapacity < _size) {
				throw std::invalid_argument("The HashTable capacity shouldn't be smaller than its size.");
			}
			size_t oldCapacity = _capacity;
			_capacity = newCapacity;

			Map_t temp;
			temp.resize(_capacity);

			// Transfer non-empty slots from the current map to temp
			for (size_t i = 0; i < oldCapacity; i++) {
				if (map[i]) {
					size_t h = hash(map[i]->first, _capacity);

					if (!temp[h]) {
						temp[h] = map[i];
						continue;
					}
					// A collision has occurred. Try to insert in another place
					// using linear probing
					size_t h2 = (h + 1) % _capacity;

					do {
						if (!temp[h2]) {
							temp[h2] = map[i];
							break;
						}
						h2 = (h2 + 1) % _capacity;
					} while (h2 != h);

					if (h2 == h)
						throw std::exception("Error: HashTable could not be resized.");
				}
			}
			// Swap the contents of the current map with the temporary map
			map.swap(temp);
		}

		// Search for item with key 'k' in the HashTable
		// If found, its value is returned; otherwise, a std::nullopt is returned
		std::optional<Value> search(const Key& k) const {
			size_t h = hash(k, _capacity);

			if (!map[h]) {
				return std::nullopt; // Key not found
			}
			if (map[h]->first == k) {
				// Return slot value wrapped on a std::optional class object
				std::optional<Value> r = map[h]->second;
				return r;
			}
			// Probe through HashTable searching for the required item
			size_t h2 = (h + 1) % _capacity;

			do {
				if (!map[h2]) {
					return std::nullopt;
				}
				if (map[h2]->first == k) {
					std::optional<Value> r = map[h2]->second;
					return r;
				}
				h2 = (h2 + 1) % _capacity;
			} while (h2 != h);

			return std::nullopt;
		}

		// Insert new item in the HashTable
		// Returns true if insertion was effectively done; otherwise, returns false
		bool insert(const Key& k, const Value& v) {
			size_t h = hash(k, _capacity);

			if (!map[h]) {
				// Insert new pair
				map[h] = std::make_shared<std::pair<Key, Value>>(k, v);
				_size++;
				// Check if load factor exceeded its limit and rehash if necessary
				if (maxLoadFactorExceeded()) {
					rehash(2 * _capacity);
				}
				return true; // Insertion done successfully
			}
			if (map[h]->first == k)
				// Don't insert duplicates
				return false;

			// A collision has occurred. Try to insert in another place
			// using linear probing
			size_t h2 = (h + 1) % _capacity;

			do {
				if (!map[h2]) {
					map[h2] = std::make_shared<std::pair<Key, Value>>(k, v);
					_size++;
					if (maxLoadFactorExceeded()) {
						rehash(2 * _capacity);
					}
					return true; 
				}
				if (map[h2]->first == k) {
					return false;
				}
				h2 = (h2 + 1) % _capacity;
			} while (h2 != h);

			throw std::exception("Error: item could not be inserted in HashTable.");
		}

		// Search for slot with key 'k' in the HashTable and, if found, remove it 
		// and returns its value; otherwise, returns a std::nullopt
		std::optional<Value> remove(const Key& k) {
			size_t h = hash(k, _capacity);

			if (!map[h]) {
				return std::nullopt; // Key not found
			}
			if (map[h]->first == k) {
				// Return value wrapped on a std::optional class object
				std::optional<Value> r = map[h]->second;
				// Empty slot
				map[h].reset();
				_size--;
				// Check if load factor exceeded its limit and rehash if necessary
				if (minLoadFactorExceeded() && _capacity > 1) {
					rehash((size_t)ceil(_capacity / 2.0));
				}
				return r; // Return removed slot value
			}
			// Probe through HashTable searching for the required item
			size_t h2 = (h + 1) & _capacity;

			do {
				if (!map[h2]) {
					return std::nullopt;
				}
				if (map[h2]->first == k) {
					std::optional<Value> r = map[h2]->second;
					map[h2].reset();
					_size--;
					if (minLoadFactorExceeded() && _capacity > 1) {
						rehash((size_t)ceil(_capacity / 2.0));
					}
					return r; 
				}
				h2 = (h2 + 1) % _capacity;
			} while (h2 != h);

			return std::nullopt;
		}

		// Get a vector with the values of all items stored in the HashTable
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
			return _size == 0;
		}

	private:
		Map_t map;

		// Total number of slots
		size_t _capacity;
		// Number of occupied slots
		size_t _size;

		// Maximum size/capacity value
		double _maxLoadFactor;
		// Minimum size/capacity value
		double _minLoadFactor;

		// Custom hash function
		HashFunction _customHashFn;

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
		// The retuned value will be in the range [0, range - 1]
		size_t hash(const Key& k, size_t range) const {
			// Use the custom hash function if provided; otherwise, use the default hash function
			auto hashFn = _customHashFn ? _customHashFn : [this, range](const Key& k) -> size_t {
				// 'ptr' points to the memory representation of 'k'
				const char* ptr = nullptr;
				ptr = reinterpret_cast<const char*>(&k);

				if (!ptr) {
					throw std::runtime_error("Invalid key type for default hash function.");
				}
				// Assuming the input alphabet can contain all UTF-8 characters
				static const uint16_t alphabetSize = UCHAR_MAX + 1;
				// Some prime number greater than the input alphabet size
				static const uint16_t somePrime = alphabetSize + 1;

				const size_t blockSize = sizeof(k);
				size_t primePow = somePrime;
				size_t hashCode = 0;

				// Treat 'k' as a byte string and apply polynomial rolling hash
				for (size_t i = 0; i < blockSize; i++) {
					hashCode = (hashCode + (static_cast<size_t>(*(ptr + i)) + 1) * primePow) % range;
					primePow = (primePow * somePrime) % range;
				}
				return hashCode;
				};
			// Calculate hash code using the selected hash function
			return hashFn(k) % range;
		}

		// Check if the load factor has reached its maximum value
		bool maxLoadFactorExceeded() const {
			return (static_cast<double>(_size) / _capacity) > _maxLoadFactor;
		}
		// Check if the load factor has reached its minimum value
		bool minLoadFactorExceeded() const {
			return (static_cast<double>(_size) / _capacity) < _minLoadFactor;
		}
	};
}