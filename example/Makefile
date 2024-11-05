STD      := -std=c++20
WARNINGS := -Wall -Wextra -pedantic

INCS     := -I.. `pkg-config --cflags libzip`
LIBS     := `pkg-config --libs libzip`

VERSION          := $(strip $(shell cat ../VERSION))
VERSION_MAJOR    := $(word 1,$(subst ., ,$(VERSION)))
VERSION_MINOR    := $(word 2,$(subst ., ,$(VERSION)))
VERSION_REVISION := $(word 3,$(subst ., ,$(VERSION)))


CPPFLAGS := -DVERSION=\"$(VERSION)\" -DVERSION_MAJOR=$(VERSION_MAJOR) -DVERSION_MINOR=$(VERSION_MINOR) -DVERSION_REVISION=$(VERSION_REVISION)

CFLAGS  := $(STD) $(WARNINGS) -O2 $(INCS) $(CPPFLAGS)
LDFLAGS := $(LIBS)


all: example

example: example.cpp
	$(CXX) $(CFLAGS) $(LDFLAGS) -o $@ $^

clean:
	rm -f example
