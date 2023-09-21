//
// Created by af on 21/09/23.
//

#include "include/generic_unsync_alloc.h"
#include <benchmark/benchmark.h>
#include <iostream>
#include <list>
#include <unordered_map>
#include <vector>


using string_type = std::basic_string<char, std::char_traits<char>, cau::STD_allocator<char>>;

using Map = std::unordered_map<string_type, void *, std::hash<string_type>, std::equal_to<string_type>,
							   cau::STD_allocator<std::pair<const string_type, void *>>>;


using Vector = std::vector<int, cau::STD_allocator<int>>;
using List   = std::list<int, cau::STD_allocator<int>>;
using Set    = std::unordered_set<int, std::hash<int>, std::equal_to<>, cau::STD_allocator<int>>;


cau::generic_allocator<cau::default_allocator> *cau::global_file_allocator;

static void BM_custom_allocator(benchmark::State &s) {

	cau::generic_allocator<cau::default_allocator> alloc;
	cau::global_file_allocator = &alloc;

	for (auto _: s) {
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

		for (int i = 0; i < 1000; i++) {
			list->push_back(i);
			vec->push_back(i);
			set->insert(i);
		}
		(*map)["test3_avoid_sso_avoid_sso_avoid_sso_avoid_sso_avoid_sso"] = set;
		(*map)["hey_3"]                                                   = nullptr;

		benchmark::DoNotOptimize(std::move(*map));

		alloc.dealloc(vec, 1);
		alloc.dealloc(set, 1);
		alloc.dealloc(list, 1);
		alloc.dealloc(map, 1);
	}
	cau::global_file_allocator = nullptr;
}

static void BM_std_allocator(benchmark::State &s) {


	for (auto _: s) {

		auto *list = new std::list<int>{1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
		auto *vec  = new std::vector<int>{1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

		auto *map = new std::unordered_map<std::string, void *>{};

		auto *set = new std::unordered_set<int>{1, 43, 56, 87, 39, 203};
		(*map)["test_avoid_sso_avoid_sso_avoid_sso_avoid_sso_avoid_sso"]  = list;
		(*map)["test2_avoid_sso_avoid_sso_avoid_sso_avoid_sso_avoid_sso"] = vec;
		(*map)["hey_1"]                                                   = nullptr;
		(*map)["hey_2"]                                                   = nullptr;

		for (int i = 0; i < 1000; i++) {
			list->push_back(i);
			vec->push_back(i);
			set->insert(i);
		}
		(*map)["test3_avoid_sso_avoid_sso_avoid_sso_avoid_sso_avoid_sso"] = set;
		(*map)["hey_3"]                                                   = nullptr;

		benchmark::DoNotOptimize(std::move(*map));
		delete vec;
		delete set;
		delete list;
		delete map;
	}
}

BENCHMARK(BM_custom_allocator)->UseRealTime();
BENCHMARK(BM_std_allocator)->UseRealTime();

BENCHMARK_MAIN();
