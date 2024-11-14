HDR_LIB=./third-party/HdrHistogram_c/build/src/libhdr_histogram_static.a
HDR_INCLUDES=-I./third-party/HdrHistogram_c/include
HDR_LDFLAGS=-lz -lm
HDR_CFLAGS=-Wno-unknown-pragmas -Wextra -Wshadow -Winit-self -Wpedantic -D_GNU_SOURCE -fPIC

CC=g++
CFLAGS=-std=c++17 -O2 -pthread
CXXFLAGS=-std=c++17 -O2 -pthread -I. -I..
INCLUDES=-I./
LDFLAGS= -lpthread -ltbb -L$(HDR_LIB)
SUBDIRS=core db third-party/HdrHistogram_c
SUBSRCS=$(wildcard core/*.cc) $(wildcard db/*.cc)
OBJECTS=$(SUBSRCS:.cc=.o)

TMPVAR := $(OBJECTS)
OBJECTS = $(filter-out db/rocksdb_db.o db/rocksdb_cloud_db.o db/db_factory.o, $(TMPVAR))

MY_LOCAL_BASE_DIR=/home/wjp
MY_CLOUD_BASE_DIR=/home/ubuntu
MY_BASE_DIR=${MY_LOCAL_BASE_DIR}#不同的环境需要修改这个变量
ROCKSDB_LIB=${MY_BASE_DIR}/HyperCloudDB/build/librocksdb.a
# ROCKSDB_LIB=/home/ubuntu/HyperCloudDB/librocksdb_debug.a
ROCKSDB_PLATFORM_CXXFLAGS=-DROCKSDB_USE_RTTI -O2 -W -Wextra -Wsign-compare -Wshadow -Werror -I. -I./include -std=c++17 -faligned-new -DHAVE_ALIGNED_NEW -DROCKSDB_PLATFORM_POSIX -DROCKSDB_LIB_IO_POSIX -DOS_LINUX -fno-builtin-memcmp -DROCKSDB_FALLOCATE_PRESENT -DSNAPPY -DGFLAGS=1 -DZLIB -DBZIP2 -DLZ4 -DZSTD -DTBB -DROCKSDB_MALLOC_USABLE_SIZE -DROCKSDB_PTHREAD_ADAPTIVE_MUTEX -DROCKSDB_BACKTRACE -DROCKSDB_RANGESYNC_PRESENT -DROCKSDB_SCHED_GETCPU_PRESENT -march=native -DHAVE_SSE42 -DHAVE_PCLMUL -DROCKSDB_SUPPORT_THREAD_LOCAL
ROCKSDB_PLATFORM_LDFLAGS=-lpthread -lrt -lz -ltbb
ROCKSDB_EXEC_LDFLAGS=-ldl
ROCKSDB_INCLUDES=-I"${MY_BASE_DIR}/HyperCloudDB/include"

ROCKSDB_CLOUD_PLATFORM_CXXFLAGS=-faligned-new -DHAVE_ALIGNED_NEW -DROCKSDB_PLATFORM_POSIX -DROCKSDB_LIB_IO_POSIX -DOS_LINUX -fno-builtin-memcmp -DROCKSDB_FALLOCATE_PRESENT -DSNAPPY -DGFLAGS=1 -DZLIB -DBZIP2 -DLZ4 -DZSTD -DTBB -DROCKSDB_MALLOC_USABLE_SIZE -DROCKSDB_PTHREAD_ADAPTIVE_MUTEX -DROCKSDB_BACKTRACE -DROCKSDB_RANGESYNC_PRESENT -DROCKSDB_SCHED_GETCPU_PRESENT -march=native -DHAVE_SSE42 -DHAVE_PCLMUL -DROCKSDB_SUPPORT_THREAD_LOCAL
ROCKSDB_CLOUD_LDFLAGS=-laws-cpp-sdk-s3 -laws-cpp-sdk-kinesis -laws-cpp-sdk-core -laws-cpp-sdk-transfer -lsnappy -lgflags -lbz2 -llz4 -lzstd

CXXFLAGS+=$(HDR_INCLUDES)
CXXFLAGS+=$(ROCKSDB_INCLUDES)

all: dependency ycsbc

dependency: $(SUBDIRS)

$(SUBDIRS):
	@for dir in $(SUBDIRS); do \
		echo "------------- Make in "$$dir" -------------";\
        $(MAKE) -C $$dir; \
        echo "-----------------------------------------------";\
    done

# $(ROCKSDB_LIB):
# 	$(MAKE) -C rocksdb-cloud -j8 static_lib

ycsbc: ycsbc.cc db/rocksdb_cloud_db.cc db/db_factory.cc $(OBJECTS) $(HDR_LIB) $(ROCKSDB_LIB)
	$(CC) $(CFLAGS) $^ -O2 $(LDFLAGS) $(HDR_LDFLAGS) $(ROCKSDB_PLATFORM_LDFLAGS) $(INCLUDES) $(HDR_INCLUDES) $(ROCKSDB_INCLUDES) $(ROCKSDB_PLATFORM_CXXFLAGS) $(ROCKSDB_CLOUD_PLATFORM_CXXFLAGS) $(ROCKSDB_EXEC_LDFLAGS) $(ROCKSDB_CLOUD_LDFLAGS) -o $@

# measurements_test: measurements_test.cc $(OBJECTS) $(HDR_LIB)
# 	$(CC) $(CFLAGS) $^ $(LDFLAGS) $(HDR_LDFLAGS) $(INCLUDES) $(HDR_INCLUDES) -o $@

clean:
	for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir $@; \
	done
	$(RM) ycsbc

.PHONY: $(ROCKSDB_LIB) ycsbc

