//
// Created by af on 18/09/23.
//

#ifndef CUSTOM_ALLOCATOR_SMALL_ALLOCATION_BUCKET_H
#define CUSTOM_ALLOCATOR_SMALL_ALLOCATION_BUCKET_H

#include "utils.h"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <optional>

namespace cau::sab {
	struct free_list_iterator {
		uint8_t *current_byte;
		uint8_t  current_bit;

		void next() {
			if (current_bit == 7) {
				current_bit = 0;
				current_byte++;
			} else {
				current_bit++;
			}
		}

		bool get() { return (*current_byte >> current_bit) & 1; }

		bool operator==(const free_list_iterator &other) const {
			return current_byte == other.current_byte && current_bit == other.current_bit;
		}
	};


	template<uint64_t ALIGNMENT = 64>
	void flag_range_in_free_list(uint8_t *free_list, uint8_t *begin_of_allocatable_memory, uint8_t *begin, uint8_t *end,
								 bool flag) {

		free_list_iterator it     = {free_list + ((begin - begin_of_allocatable_memory) / ALIGNMENT) / 8,
									 uint8_t(((begin - begin_of_allocatable_memory) / ALIGNMENT) % 8)};
		free_list_iterator it_end = {free_list + ((end - begin_of_allocatable_memory) / ALIGNMENT) / 8,
									 uint8_t(((end - begin_of_allocatable_memory) / ALIGNMENT) % 8)};

		if (flag) {
			while (!(it == it_end)) {
				*it.current_byte |= 1 << it.current_bit;
				it.next();
			}
		} else {
			while (!(it == it_end)) {
				*it.current_byte &= ~uint8_t(1 << it.current_bit);
				it.next();
			}
		}
	}

	struct bucket_range {
		uint8_t *begin;
		uint8_t *end;
	};


	bool free_list_is_empty(bucket_range free_list) {
		free_list_iterator it  = {free_list.begin, 0};
		free_list_iterator end = {free_list.end, 0};
		while (true) {
			if (it == end) { return true; }
			if (it.get()) { return false; }
			it.next();
		}
	}

	template<uint64_t ALIGNMENT = 64>
	uint8_t *from_iterator(bucket_range free_list, free_list_iterator it, uint8_t *begin_of_allocatable_memory) {
		return begin_of_allocatable_memory + ((it.current_byte - free_list.begin) * 8 + it.current_bit) * ALIGNMENT;
	}

	template<uint64_t ALIGNMENT = 64>
	bucket_range get_first_fit(bucket_range free_list, uint64_t total_free, uint64_t size,
							   uint8_t *begin_of_allocatable_memory) {
		free_list_iterator it  = {free_list.begin, 0};
		free_list_iterator end = {free_list.end, 0};

		uint64_t           total_free_space_left = total_free;
		uint64_t           current_streak        = 0;
		free_list_iterator first_in_streak       = it;
		while (true) {

			if (current_streak == size) {
				return {from_iterator<ALIGNMENT>(free_list, first_in_streak, begin_of_allocatable_memory),
						from_iterator<ALIGNMENT>(free_list, it, begin_of_allocatable_memory)};
			}
			if (it == end) { return {nullptr, nullptr}; }
			if (total_free_space_left == 0) { return {nullptr, nullptr}; }

			const bool is_free = !it.get();

			if (!is_free) {
				current_streak = 0;
			} else {
				total_free_space_left--;
				if (current_streak == 0) { first_in_streak = it; }
				current_streak++;
			}
			it.next();
		}
	}

	uint64_t count_free_slots(bucket_range free_list) {
		uint64_t           total_free_space_left = 0;
		free_list_iterator it                    = {free_list.begin, 0};
		free_list_iterator end                   = {free_list.end, 0};
		while (true) {
			if (it == end) { return total_free_space_left; }
			if (!it.get()) { total_free_space_left++; }
			it.next();
		}
	}

	bucket_range align_to(bucket_range range, uint64_t alignment) {

		return {
				.begin = (uint8_t *) intptr_t(round_up_to_multiple(uint64_t(range.begin), alignment)),
				.end   = (uint8_t *) intptr_t(round_down_to_multiple(uint64_t(range.end), alignment)),
		};
	}

	void validate_range(bucket_range range) {
		if (range.begin > range.end) { throw std::runtime_error("range.begin > range.end"); }
		if (range.begin == nullptr) { throw std::runtime_error("range.begin == nullptr"); }
	}

	bool check_alignment(bucket_range range, uint64_t alignment) {
		return ((intptr_t) range.begin) % alignment == 0 && ((intptr_t) range.end) % alignment == 0;
	}

	template<uint64_t ALIGNMENT = 64, INVARIANT_CHECKING ic = INVARIANT_CHECKING::NONE>
	struct bucket {
		constexpr static uint64_t MAGIC_NUMBER       = 0x47a4b3c2d1e0f9a8;
		uint64_t                  magic_number       = MAGIC_NUMBER;
		uint64_t                  initialized        = 0;
		uint8_t                  *begin              = nullptr;
		uint8_t                  *begin_of_memory    = nullptr; // begin+ alignment
		uint8_t                  *begin_of_free_list = nullptr; // end of memory, start of free list
		uint8_t                  *end_of_free_list   = nullptr; // end of free list
		uint8_t                  *end                = nullptr; // unused space
		uint64_t                  free_elements      = 0;
		void                     *container          = nullptr;


		[[nodiscard]] bool corrupted() const {
			if (magic_number != MAGIC_NUMBER) { return true; }

			if (!initialized) { return false; }

			// check, that begin_of_memory and begin_of_free_list are aligned
			if (!check_alignment({begin_of_memory, begin_of_free_list}, ALIGNMENT)) { return true; }


			if (!(begin <= begin_of_memory && begin_of_memory <= begin_of_free_list &&
				  begin_of_free_list <= end_of_free_list && end_of_free_list <= end)) {
				return true;
			}

			if (begin == nullptr) { return true; }

			if (free_elements != count_free_slots({begin_of_free_list, end_of_free_list})) { return true; }

			return false;
		}

		[[nodiscard]] bool is_initialized() const { return initialized == 1; }

		bucket() = default;

		bucket(uint8_t *begin_, uint8_t *end_, void *container)
			: initialized(1), begin(begin_), end(end_), container(container) {
			const auto [begin_aligned, end_aligned] = align_to({begin, end}, ALIGNMENT);

			if constexpr (ic == INVARIANT_CHECKING::CONSTANT || ic == INVARIANT_CHECKING::FULL) {
				validate_range({begin_aligned, end_aligned});
			}

			uint64_t size = end_aligned - begin_aligned;
			size          = round_down_to_multiple_plus_one(size, ALIGNMENT * 8);

			uint64_t size_of_memory = size * (ALIGNMENT * 8) / (ALIGNMENT * 8 + 1);

			begin_of_memory    = begin_aligned;
			begin_of_free_list = begin_aligned + size_of_memory;
			end_of_free_list   = begin_aligned + size;

			memset(begin_aligned, 0, size);

			free_elements = size / ALIGNMENT;
		}

		void destroy() { initialized = 0; }

		[[nodiscard]] uint64_t get_total_elements() const {
			uint64_t size = begin_of_free_list - begin_of_memory;
			return size / ALIGNMENT;
		}

		std::optional<allocation> try_alloc(uint64_t size) {
			if constexpr (ic == INVARIANT_CHECKING::CONSTANT || ic == INVARIANT_CHECKING::FULL) {
				if (corrupted()) { throw std::runtime_error("corrupt"); }
			}
			size = round_up_to_multiple(size, ALIGNMENT);

			if (size > free_elements * ALIGNMENT) { return std::nullopt; }


			auto res = get_first_fit<ALIGNMENT>({begin_of_free_list, end_of_free_list}, free_elements, size / ALIGNMENT,
												begin_of_memory);

			if (res.begin == nullptr || res.end == nullptr) { return std::nullopt; }

			size = res.end - res.begin;

			flag_range_in_free_list<ALIGNMENT>(begin_of_free_list, begin_of_memory, res.begin, res.end, true);
			free_elements -= size / ALIGNMENT;

			return allocation{res.begin, res.end};
		}

		enum class DEALLOC_ERROR {
			SUCCESS,
			NOT_IN_RANGE,
			NOT_ALIGNED,
			SUCCESS_NOW_EMPTY,
			CORRUPTED,
		};

		DEALLOC_ERROR dealloc(allocation alloc) {
			// check alignment
			if constexpr (ic == INVARIANT_CHECKING::CONSTANT || ic == INVARIANT_CHECKING::FULL) {
				if (corrupted()) { return DEALLOC_ERROR::CORRUPTED; }
				if (!check_alignment({alloc.begin, alloc.end}, ALIGNMENT)) { return DEALLOC_ERROR::NOT_ALIGNED; }
				if (alloc.begin < begin_of_memory || alloc.end > end) { return DEALLOC_ERROR::NOT_IN_RANGE; }
			}
			flag_range_in_free_list<ALIGNMENT>(begin_of_free_list, begin_of_memory, alloc.begin, alloc.end, false);
			free_elements += (alloc.end - alloc.begin) / ALIGNMENT;
			if (free_elements == get_total_elements()) { return DEALLOC_ERROR::SUCCESS_NOW_EMPTY; }
			return DEALLOC_ERROR::SUCCESS;
		}
	};
} // namespace cau::sab

#endif //CUSTOM_ALLOCATOR_SMALL_ALLOCATION_BUCKET_H
