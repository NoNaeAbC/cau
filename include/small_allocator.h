//
// Created by af on 21/09/23.
//

#ifndef CUSTOM_ALLOCATOR_SMALL_ALLOCATOR_H
#define CUSTOM_ALLOCATOR_SMALL_ALLOCATOR_H

#include "small_allocation_bucket.h"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <optional>
#include <unordered_set>


namespace cau {
	template<uint64_t ALIGNMENT = 64>
	struct SAB_Header {
		uint64_t                size;
		sab::bucket<ALIGNMENT> *bucket;
	};

	template<uint64_t ALIGNMENT = 64>
	inline std::optional<allocation> small_allocator_adapter(sab::bucket<ALIGNMENT> *bucket, size_t size) {
		auto alloc_try = bucket->try_alloc(size + ALIGNMENT);
		if (alloc_try) {
			allocation alloc = *alloc_try;
			new (alloc.begin) SAB_Header<ALIGNMENT>{uint64_t(alloc.end - alloc.begin), bucket};
			alloc.begin += ALIGNMENT;
			return alloc;
		}
		return std::nullopt;
	}


	template<uint64_t ALIGNMENT = 64>
	inline std::pair<typename sab::bucket<ALIGNMENT>::DEALLOC_ERROR, sab::bucket<ALIGNMENT> *>
	deallocate_small_allocation_adapter(allocation alloc) {
		auto *header = (SAB_Header<ALIGNMENT> *) (alloc.begin - 64);
		if (!header->bucket->is_initialized()) {
			return std::make_pair(sab::bucket<ALIGNMENT>::DEALLOC_ERROR::CORRUPTED, nullptr);
		}
		return std::make_pair(header->bucket->dealloc({(uint8_t *) header, (uint8_t *) header + header->size}),
							  header->bucket);
	}


	template<uint64_t ALIGNMENT = 64, INVARIANT_CHECKING IC = INVARIANT_CHECKING::NONE>
	struct small_allocator_node {
		static constexpr uint64_t  BUCKET_COUNT = 64;
		sab::bucket<ALIGNMENT, IC> buckets[BUCKET_COUNT]{};

		sab::bucket<ALIGNMENT, IC> special_bucket_for_allocation_of_nodes{};

		small_allocator_node *next         = nullptr;
		small_allocator_node *prev         = nullptr;
		uint64_t              free_buckets = BUCKET_COUNT;


		void debug_print() {
			std::cout << "Node: " << this << std::endl;
			for (uint64_t i = 0; i < small_allocator_node<ALIGNMENT, IC>::BUCKET_COUNT; i++) {
				if (!buckets[i].is_initialized()) {
					// color green
					std::cout << "\033[32m";
				}
				std::cout << "Bucket: " << i << " free: " << buckets[i].free_elements << " of "
						  << buckets[i].get_total_elements() << std::endl;
				if (!buckets[i].is_initialized()) {
					// reset color
					std::cout << "\033[0m";
				}
			}
			std::cout << /*id <<*/ " : " << free_buckets << " free buckets" << std::endl;
		}

		bool is_bucket_in_range(sab::bucket<ALIGNMENT, IC> *bucket) {
			return bucket >= buckets && bucket < buckets + BUCKET_COUNT;
		}

		void validate_free_bucket_count() {
			// linear error checking
			uint64_t true_free_buckets = 0;
			for (uint64_t i = 0; i < BUCKET_COUNT; i++) {
				if (!buckets[i].is_initialized()) { true_free_buckets++; }
			}
			if (free_buckets != true_free_buckets) {
				debug_print();
				throw std::runtime_error("free_buckets is not correct");
			}
		}
	};

	template<uint64_t ALIGNMENT = 64, INVARIANT_CHECKING IC = INVARIANT_CHECKING::NONE>
	struct Small_Allocator {
		small_allocator_node<ALIGNMENT, IC> head{};
		i_allocator                         allocator;
		// due to the assumption that allocations are short-lived, we try to allocate from the last bucket first.

		struct NodeIterator {
			small_allocator_node<ALIGNMENT, IC> *current_node;
			uint64_t                             index_into_current_node;

			void next(small_allocator_node<ALIGNMENT, IC> *base_node) {
				if (index_into_current_node < small_allocator_node<ALIGNMENT, IC>::BUCKET_COUNT - 1) {
					index_into_current_node++;
					return;
				}
				if (current_node->next == nullptr) {
					current_node            = base_node;
					index_into_current_node = 0;
					return;
				}
				current_node            = current_node->next;
				index_into_current_node = 0;
			}

			void try_to_go_back() {
				if (index_into_current_node > 0) {
					index_into_current_node--;
					return;
				}
			}

			sab::bucket<ALIGNMENT, IC> *get_current_bucket() { return &current_node->buckets[index_into_current_node]; }

			bool operator==(const NodeIterator &other) const {
				return current_node == other.current_node && index_into_current_node == other.index_into_current_node;
			}
		};

		NodeIterator current_node = {&head, 0};

		void destroy_unused_bucket(sab::bucket<ALIGNMENT, IC> *bucket) {
			small_allocator_node<ALIGNMENT, IC> *container = (small_allocator_node<ALIGNMENT, IC> *) bucket->container;


			if constexpr (IC == INVARIANT_CHECKING::CONSTANT || IC == INVARIANT_CHECKING::FULL) {
				if (!container->is_bucket_in_range(bucket)) { throw std::runtime_error("Bucket is not in range"); }
			}
			allocator.dealloc({bucket->begin, bucket->end});
			bucket->destroy();

			container->free_buckets++;
			if constexpr (IC == INVARIANT_CHECKING::FULL) { container->validate_free_bucket_count(); }
			if (container->free_buckets < small_allocator_node<ALIGNMENT, IC>::BUCKET_COUNT) { return; }
			if (container == &head) { return; }

			// reset the current iterator
			if (current_node.current_node == container) {
				current_node = {container->prev, small_allocator_node<ALIGNMENT, IC>::BUCKET_COUNT - 1};
			}
			// check if all buckets are really free
			if constexpr (IC == INVARIANT_CHECKING::FULL) {
				for (uint64_t i = 0; i < small_allocator_node<ALIGNMENT, IC>::BUCKET_COUNT; i++) {
					if (container->buckets[i].is_initialized()) {

						print_stats();
						throw std::runtime_error("Not all buckets are free");
					}
				}
			}

			if (container->prev != nullptr) { container->prev->next = container->next; }
			if (container->next != nullptr) { container->next->prev = container->prev; }


			allocator.dealloc(
					{(uint8_t *) container, (uint8_t *) container + sizeof(small_allocator_node<ALIGNMENT, IC>)});
		}

		void dealloc(allocation alloc) {
			auto [res, bucket] = deallocate_small_allocation_adapter(alloc);

			if constexpr (IC == INVARIANT_CHECKING::FULL) {
				if (sab::free_list_is_empty({bucket->begin_of_free_list, bucket->end_of_free_list}) &&
					res != sab::bucket<ALIGNMENT>::DEALLOC_ERROR::SUCCESS_NOW_EMPTY) {
					throw std::runtime_error("Free list is empty but dealloc was not successful");
				}
			}
			if constexpr (IC == INVARIANT_CHECKING::CONSTANT || IC == INVARIANT_CHECKING::FULL) {
				if (res == sab::bucket<ALIGNMENT, IC>::DEALLOC_ERROR::CORRUPTED) {
					throw std::runtime_error("Corrupted bucket");
				}
				if (res == sab::bucket<ALIGNMENT, IC>::DEALLOC_ERROR::NOT_IN_RANGE) {
					throw std::runtime_error("Not in range");
				}
				if (res == sab::bucket<ALIGNMENT, IC>::DEALLOC_ERROR::NOT_ALIGNED) {
					throw std::runtime_error("Not aligned");
				}
			}
			if (res == sab::bucket<ALIGNMENT, IC>::DEALLOC_ERROR::SUCCESS_NOW_EMPTY) { destroy_unused_bucket(bucket); }
		}

		small_allocator_node<ALIGNMENT, IC> *allocate_new_node() {
			// Evaluate, to use this allocator itself to allocate new nodes.
			auto alloc = allocator.alloc(sizeof(small_allocator_node<ALIGNMENT, IC>));
			if (alloc.begin == nullptr) { throw std::bad_alloc(); }
			new (alloc.begin) small_allocator_node<ALIGNMENT, IC>{};
			return (small_allocator_node<ALIGNMENT, IC> *) alloc.begin;
		}

		sab::bucket<ALIGNMENT, IC> *construct_new_bucket(uint64_t minimal_size) {
			// correct minimal size to account of overhead of bucket
			minimal_size = max(minimal_size * 12 / 10 /*add 20 %*/, ALIGNMENT * 50) +
						   3 * ALIGNMENT /* correct possibility of incorrect alignment and add allocation header*/;
			// Step 1: Find corrupted bucket and if found allocate a new bucket.
			NodeIterator it      = current_node;
			NodeIterator it_copy = it;
			while (true) {
				sab::bucket<ALIGNMENT, IC> *bucket = it.get_current_bucket();
				if (!bucket->is_initialized()) {
					// If a non-initialized bucket is found, try to allocate a new bucket.
					auto alloc = allocator.alloc(minimal_size * 12 / 10 /*add 20 %*/);
					if (alloc.begin == nullptr) { throw std::bad_alloc(); }
					// If allocation was successful, construct a new bucket.
					new (bucket) sab::bucket(alloc.begin, alloc.end, it.current_node);
					it.current_node->free_buckets--;
					it.current_node->validate_free_bucket_count();

					// Return the new bucket.
					return bucket;
				}
				it.next(&head);
				if (it == it_copy) { break; }
			}
			// If no corrupted bucket is found, create a new node.
			small_allocator_node<ALIGNMENT, IC> *new_node = allocate_new_node();
			if constexpr (IC == INVARIANT_CHECKING::CONSTANT || IC == INVARIANT_CHECKING::FULL) {
				if (new_node->free_buckets != 64) { throw std::runtime_error("New node is not initialized correctly"); }
			}
			// append to list
			small_allocator_node<ALIGNMENT, IC> *node = &head;
			while (node->next != nullptr) { node = node->next; }
			node->next     = new_node;
			new_node->prev = node;
			// construct new bucket
			auto alloc = allocator.alloc(minimal_size * 12 / 10 /*add 20 %*/);
			if (alloc.begin == nullptr) { throw std::bad_alloc(); }
			new (new_node->buckets) sab::bucket(alloc.begin, alloc.end, new_node);
			new_node->free_buckets--;
			return new_node->buckets;
		}

		allocation allocate(uint64_t size) {
			NodeIterator it = current_node;

			int iterations_before_allocating_new_bucket = 6;

			while (true) {
				sab::bucket<ALIGNMENT, IC> *bucket = it.get_current_bucket();
				if (bucket->is_initialized()) {
					auto alloc = small_allocator_adapter(bucket, size);
					if (alloc) {
						//std::cout << "hey!" << std::endl;

						current_node = it;
						return *alloc;
					}
				}
				it.next(&head);
				iterations_before_allocating_new_bucket--;
				if (iterations_before_allocating_new_bucket == 0) {
					sab::bucket<ALIGNMENT, IC> *new_bucket = construct_new_bucket(size);
					if constexpr (IC == INVARIANT_CHECKING::CONSTANT || IC == INVARIANT_CHECKING::FULL) {
						if (!new_bucket->is_initialized()) { throw std::runtime_error("Bucket is not initialized"); }
					}
					auto alloc = small_allocator_adapter(new_bucket, size);
					if (alloc) {
						return *alloc;
					} else {
						throw std::bad_alloc();
					}
				}
			}
		}

		void print_stats() {
			// print used buckets for each node
			small_allocator_node<ALIGNMENT, IC> *node = &head;
			while (node != nullptr) {
				std::cout << "Node: " << node << std::endl;
				for (uint64_t i = 0; i < small_allocator_node<ALIGNMENT, IC>::BUCKET_COUNT; i++) {
					if (!node->buckets[i].is_initialized()) {
						// color green
						std::cout << "\033[32m";
					}
					std::cout << "Bucket: " << i << " free: " << node->buckets[i].free_elements << " of "
							  << node->buckets[i].get_total_elements() << std::endl;
					if (!node->buckets[i].is_initialized()) {
						// reset color
						std::cout << "\033[0m";
					}
				}
				std::cout << /*node->id <<*/ " : " << node->free_buckets << " free buckets" << std::endl;
				node = node->next;
			}
		}
	};
} // namespace cau
#endif //CUSTOM_ALLOCATOR_SMALL_ALLOCATOR_H
