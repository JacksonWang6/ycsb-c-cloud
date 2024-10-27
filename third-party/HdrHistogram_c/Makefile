CC=gcc
CFLAGS+=-Wall -Wno-unknown-pragmas -Wextra -Wshadow -Winit-self -Wmissing-prototypes -Wpedantic -D_GNU_SOURCE -std=gnu89 -fPIC -g
INCLUDES=-I./src -I./test
HEADERS=$(wildcard src/*.h)
SUBSRCS=$(wildcard src/*.c)
OBJECTS=$(SUBSRCS:.c=.o)
LDFLAGS=-lz -lm

.PHONY: all $(OBJECTS)

all: $(OBJECTS) hdr_lib

$(OBJECTS): src/%.o : src/%.c
	$(CC) $(CFLAGS) -c $< $(LDFLAGS) -o $@

hdr_lib: $(OBJECTS)
	ar rvs hdr_lib.a $^

hdr_histogram_test: test/hdr_histogram_test.c test/minunit.c $(OBJECTS)
	$(CC) $(CFLAGS) $^ $(LDFLAGS) $(INCLUDES) -o $@

clean:
	rm $(OBJECTS)