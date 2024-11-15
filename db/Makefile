MY_LOCAL_BASE_DIR=/home/wjp
MY_CLOUD_BASE_DIR=/home/ubuntu
MY_BASE_DIR=${MY_CLOUD_BASE_DIR}#不同的环境需要修改这个变量
CC=g++
CFLAGS=-std=c++17 -c -g -Wall -fgnu-tm -I.. -I. -I${MY_BASE_DIR}/rocksdb-cloud/include
INCLUDES=-I../ -I${ROCKSDB_INCLUDE} -L${ROCKSDB_LIBRARY}
SOURCES=$(wildcard *.cc)
OBJECTS=$(SOURCES:.cc=.o)

all: $(SOURCES) $(OBJECTS)

.cc.o:
	$(CC) $(CFLAGS) $(INCLUDES) $< -o $@

clean:
	$(RM) $(OBJECTS)

