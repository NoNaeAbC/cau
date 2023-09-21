//
// Created by af on 21/09/23.
//


#include <list>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "include/generic_unsync_alloc.h"

using string_type = std::basic_string<char, std::char_traits<char>, cau::STD_allocator<char>>;

using Map = std::unordered_map<string_type, void *, std::hash<string_type>, std::equal_to<string_type>,
							   cau::STD_allocator<std::pair<const string_type, void *>>>;


using Vector = std::vector<int, cau::STD_allocator<int>>;
using List   = std::list<int, cau::STD_allocator<int>>;
using Set    = std::unordered_set<int, std::hash<int>, std::equal_to<>, cau::STD_allocator<int>>;


cau::generic_allocator<cau::default_allocator> *cau::global_file_allocator;

int main() {

	cau::generic_allocator<cau::default_allocator> alloc;

	cau::global_file_allocator = &alloc;


	for (int i = 0; i < 100; i++) {
		auto *list = alloc.alloc<List>(1);
		auto *vec  = alloc.alloc<Vector>(1);

		new (list) List{1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
		new (vec) Vector{1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

		auto *map = alloc.alloc<Map>(1);
		new (map) Map{};

		auto *set = alloc.alloc<Set>(1);
		new (set) Set{1, 43, 56, 87, 39, 203};

		(*map)["test_avoid_sso_avoid_sso_avoid_sso_avoid_sso_avoid_sso"]  = list;
		(*map)["test2_avoid_sso_avoid_sso_avoid_sso_avoid_sso_avoid_sso"] = vec;
		(*map)["hey_1"]                                                   = nullptr;
		(*map)["hey_2"]                                                   = nullptr;

		for (int j = 11; j < 1000; j++) {
			list->push_back(j);
			vec->push_back(j);
			set->insert(j);
		}
		(*map)["hey_3"] = nullptr;
		List *l         = (List *) (*map)["test_avoid_sso_avoid_sso_avoid_sso_avoid_sso_avoid_sso"];
		// check, that l contains the numbers 1 to 999 in ascending order
		{
			int  j  = 1;
			auto it = l->begin();
			while (it != l->end()) {
				if (*it != j) {
					std::cout << "ERROR: " << *it << " != " << j << std::endl;
					return 1;
				}
				j++;
				it++;
			}
		}
		Vector *v = (Vector *) (*map)["test2_avoid_sso_avoid_sso_avoid_sso_avoid_sso_avoid_sso"];
		// same with vector
		{
			int  j  = 1;
			auto it = v->begin();
			while (it != v->end()) {
				if (*it != j) {
					std::cout << "ERROR: " << *it << " != " << j << std::endl;
					return 1;
				}
				j++;
				it++;
			}
		}

		alloc.dealloc(vec, 1);
		alloc.dealloc(set, 1);
		alloc.dealloc(list, 1);
		alloc.dealloc(map, 1);
	}

	cau::global_file_allocator = nullptr;
	return 0;
}
