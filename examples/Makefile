STD=-std=c++20
WARNINGS=-Wall -Wextra -pedantic
INCLUDE=-I..
CFLAGS=$(STD) $(WARNINGS) $(INCLUDE) -O2
LDFLAGS=`pkg-config --libs --cflags libzip`

all: example

example: example.cpp ../ncr/ncr_numpy_impl_libzip.cpp
	$(CXX) $(CFLAGS) $(LDFLAGS) -o $@ $^

clean:
	rm -f example
