//
// Created by af on 18/09/23.
//

#ifndef CUSTOM_ALLOCATOR_UTILS_H
#define CUSTOM_ALLOCATOR_UTILS_H

#include <cstdint>
namespace cau {
	uint64_t round_down_to_multiple(uint64_t value, uint64_t multiple) { return value - (value % multiple); }

	uint64_t round_up_to_multiple(uint64_t value, uint64_t multiple) {
		return (value + multiple - 1) / multiple * multiple;
	}

	/*
	 * find largest y < x such that y * M is a multiple of M + 1
     */
	uint64_t round_down_to_multiple_plus_one(uint64_t value, uint64_t multiple) {
		// M and M + 1 are coprime
		return round_down_to_multiple(value, multiple + 1);
	}

	struct allocation {
		uint8_t *begin;
		uint8_t *end;
	};

	using alloc_func_t   = allocation (*)(std::size_t);
	using dealloc_func_t = void (*)(allocation);

	struct i_allocator {
		const alloc_func_t   alloc;
		const dealloc_func_t dealloc;
	};

	uint64_t max(uint64_t a, uint64_t b) { return a > b ? a : b; }

	enum class INVARIANT_CHECKING { NONE, CONSTANT, FULL };

} // namespace cau
#endif //CUSTOM_ALLOCATOR_UTILS_H
