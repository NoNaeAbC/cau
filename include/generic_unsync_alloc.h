
#include "small_allocator.h"
#include "utils.h"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <unordered_set>

namespace cau {
	constexpr i_allocator default_allocator = {[](size_t size) -> allocation {
												   auto *ptr = (uint8_t *) malloc(size);
												   return {ptr, ptr + size};
											   },
											   [](allocation alloc) { free(alloc.begin); }};


	/**
 *  This is a generic allocator, that wraps around another slow allocator.
 *  It doesn't provide thread safety.
 *  It does guarantee, if there is no memory allocated through this allocator anymore,
 *  then all memory is freed from the wrapped allocator.
 *  This allocator uses a granulation of 64 bytes. (refer to issue) This is useful for AVX-512.
 *  So it's a bit wasteful for single int allocations, but it's not a big deal.
 * @tparam allocator base allocator to use this can be the default allocator.
 * @tparam IC Invariant checking level.
 *  None doesn't check anything.
 *  Constant checks everything, that can be checked in constant time.
 *  Full checks everything.
 *  Assuming the allocator is correct, None is sufficient.
 *  Else Constant can detect bugs in the allocator, and Full can find even more.
 *  Full comes with a significant performance penalty. So it's not recommended outside unit tests.
 *
 */
	template<i_allocator allocator, INVARIANT_CHECKING IC = INVARIANT_CHECKING::NONE>
	struct generic_allocator {
		Small_Allocator<64, IC> small_allocator{
				.allocator = allocator,
		};


		template<class T>
		struct STD_small_allocator {

			using value_type      = T;
			using pointer         = T *;
			using const_pointer   = const T *;
			using reference       = T &;
			using const_reference = const T &;
			using size_type       = std::size_t;
			using difference_type = std::ptrdiff_t;

			template<class U>
			struct rebind {
				using other = STD_small_allocator<U>;
			};

			Small_Allocator<64, IC> &small_allocator;


			STD_small_allocator(Small_Allocator<64, IC> &smallAllocator) noexcept : small_allocator(smallAllocator) {}
			STD_small_allocator(const STD_small_allocator &other) noexcept : small_allocator(other.small_allocator) {}
			template<class U>
			STD_small_allocator(const STD_small_allocator<U> &other) noexcept
				: small_allocator(other.small_allocator) {}

			pointer allocate(size_type n) { return (pointer) small_allocator.allocate(n * sizeof(T)).begin; }

			void deallocate(pointer p, size_type n) {
				small_allocator.dealloc(allocation{(uint8_t *) p, (uint8_t *) p + n * sizeof(T)});
			}

			bool operator==(const STD_small_allocator &other) const {
				return &small_allocator == &other.small_allocator;
			}
		};

		// TODO replace with platform independent set
		using Set = std::unordered_set<void *, std::hash<void *>, std::equal_to<void *>, STD_small_allocator<void *>>;
		Set large_allocations{small_allocator};

		allocation do_large_allocation(size_t size) {
			auto alloc = allocator.alloc(size);
			large_allocations.insert(alloc.begin);
			return alloc;
		}

		allocation alloc(size_t size) {

			if (size > 32'000) { return do_large_allocation(size); }


			allocation a = small_allocator.allocate(size);
			return {
					std::assume_aligned<64>(a.begin),
					std::assume_aligned<64>(a.end),
			};
		}

		template<class T>
		T *alloc(size_t count) {
			return (T *) alloc(sizeof(T) * count).begin;
		}

		/*
	 * Since the large allocations interface is not final yet, ent in alloc must be provided for correctness.
	 * The allocation::begin must be the exact allocation::begin provided with the allocation call. The end can be a bit off.
	 * @param alloc
	 */
		void dealloc(allocation alloc) {

			if (large_allocations.find(alloc.begin) != large_allocations.end()) {
				allocator.dealloc(alloc);
				large_allocations.erase(alloc.begin);
				return;
			}
			small_allocator.dealloc(alloc);
		}

		template<class T>
		void dealloc(T *ptr, uint64_t count) {
			for (uint64_t i = 0; i < count; i++) { ptr[i].~T(); }

			dealloc({(uint8_t *) ptr, (uint8_t *) ptr + count * sizeof(T)});
		}
	};

	extern generic_allocator<default_allocator> *global_file_allocator;

	/**
 * This allocator is compatible with the std::allocator interface.
 * It uses a global global_file_allocator to not introduce state into the allocator.
 * The global_file_allocator must be set before using this allocator.
 * The global_file_allocator is not thread safe, so this allocator is not thread safe either.
 * It's recommended to use a wrapper around the global_file_allocator, that is thread safe.
 * Therefor this is a utility for debugging and testing.
 * @tparam T type to allocate
 */
	template<class T>
	struct STD_allocator {

		using value_type      = T;
		using pointer         = T *;
		using const_pointer   = const T *;
		using reference       = T &;
		using const_reference = const T &;
		using size_type       = std::size_t;
		using difference_type = std::ptrdiff_t;

		template<class U>
		struct rebind {
			using other = STD_allocator<U>;
		};

		// Constructors and destructors
		STD_allocator() noexcept                      = default;
		STD_allocator(const STD_allocator &) noexcept = default;
		template<class U>
		STD_allocator(const STD_allocator<U> &) noexcept {}

		// Allocation and deallocation
		pointer allocate(size_type n) { return (pointer) global_file_allocator->alloc(n * sizeof(T)).begin; }

		void deallocate(pointer p, size_type n) {
			global_file_allocator->dealloc(allocation{(uint8_t *) p, (uint8_t *) p + n * sizeof(T)});
		}

		// Comparison operators
		bool operator==(const STD_allocator &) const { return true; }
		// bool operator!=(const STD_allocator &other) const { return !(*this == other); }
	};
} // namespace cau
