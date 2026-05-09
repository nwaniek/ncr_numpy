STD      := -std=c++20
WARNINGS := -Wall -Wextra -pedantic

INCS     := -I.. `pkg-config --cflags libzip`
LIBS     := `pkg-config --libs libzip`

CFLAGS   := $(STD) $(WARNINGS) -O3 -march=native -DNDEBUG $(INCS)
LDFLAGS  := $(LIBS)

all: bench bench_no_mmap

bench: bench.cpp ../ncr_numpy.hpp
	$(CXX) $(CFLAGS) $(LDFLAGS) -o $@ bench.cpp

# bench_no_mmap forces the old vector-only load path so we can
# A/B-compare without rebuilding from scratch
bench_no_mmap: bench.cpp ../ncr_numpy.hpp
	$(CXX) $(CFLAGS) -DNCR_NUMPY_NO_MMAP_LOAD $(LDFLAGS) -o $@ bench.cpp

clean:
	rm -f bench bench_no_mmap

# don't accidentally clean the multi-GiB fixtures
clean-fixtures:
	rm -f assets/big_f32_1g.npy assets/big_f64_512m.npy assets/small_u64_256m.npy assets/many_arrays.npz

.PHONY: all clean clean-fixtures
