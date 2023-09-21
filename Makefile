
bench: bench/bench.cpp Makefile include/generic_unsync_alloc.h
	g++ -Ofast -Wall -Wextra -Werror -march=native -I. -std=c++20 bench/bench.cpp -flto -lbenchmark -pthread
	./a.out --benchmark_out_format=csv --benchmark_out=bench_gcc.csv --benchmark_repetitions=10
	clang++ -Ofast -Wall -Wextra -Werror -march=native -I. -std=c++20 bench/bench.cpp -flto -lbenchmark -pthread
	./a.out --benchmark_out_format=csv --benchmark_out=bench_clang.csv --benchmark_repetitions=10

test: test/test.cpp Makefile include/generic_unsync_alloc.h
	g++ -Ofast -Wall -Wextra -Werror -march=native -I. -std=c++20 test/test.cpp -g -flto -fsanitize=address,undefined
	./a.out
	clang++ -Ofast -fsyntax-only -Wall -Wextra -Werror -march=native -I. -std=c++20 test/test.cpp -g -flto -fsanitize=address,undefined

run:
	./a.out

clean:
	rm a.out
