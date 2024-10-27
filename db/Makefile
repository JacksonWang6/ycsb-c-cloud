
CC=g++
CFLAGS=-std=c++17 -c -g -Wall -fgnu-tm -I.. -I. -I/home/wjp/rocksdb-cloud/include
INCLUDES=-I../ -I${ROCKSDB_INCLUDE} -L${ROCKSDB_LIBRARY}
SOURCES=$(wildcard *.cc)
OBJECTS=$(SOURCES:.cc=.o)

all: $(SOURCES) $(OBJECTS)

.cc.o:
	$(CC) $(CFLAGS) $(INCLUDES) $< -o $@

clean:
	$(RM) $(OBJECTS)

