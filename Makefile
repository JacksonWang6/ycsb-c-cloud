
# ROCKSDB_INCLUDE=/home/wjp/rocksdb-cloud/include
# # RocksDB 的静态链接库
# ROCKSDB_LIBRARY=/home/wjp/rocksdb-cloud/librocksdb.a
# ROCKSDB_LIB=/home/ubuntu/yjs_design/AeroDB2/
# # HDR的静态链接库
# HDR_LIB=./third-party/HdrHistogram_c/hdr_lib.a
# HDR_INCLUDES=-I./third-party/HdrHistogram_c/src
# HDR_LDFLAGS=-lz -lm
# HDR_CFLAGS=-Wno-unknown-pragmas -Wextra -Wshadow -Winit-self -Wpedantic -D_GNU_SOURCE -fPIC


# CC=g++
# CFLAGS=-std=c++11 -g -Wall -pthread -I./ -I$(ROCKSDB_INCLUDE) -L$(ROCKSDB_LIB)
# #LDFLAGS= -lpthread -lrocksdb -lz -lbz2 -llz4 -ldl -lsnappy -lnuma -lzstd -lhdr_histogram -lboost_regex -lboost_iostreams -L$(HDR_LIB) 
# LDFLAGS= -lpthread -lrocksdb -lz -lbz2 -llz4 -ldl -lsnappy -L$(HDR_LIB) -lhdr_histogram -lzstd ${ROCKSDB_LIBRARY} 
# SUBDIRS= core db 
# SUBSRCS=$(wildcard core/*.cc) $(wildcard db/*.cc)
# OBJECTS=$(SUBSRCS:.cc=.o)
# EXEC=ycsbc

# all: $(SUBDIRS) $(EXEC)

# $(SUBDIRS):
# 	#$(MAKE) -C $@
# 	$(MAKE) -C $@ ROCKSDB_INCLUDE=${ROCKSDB_INCLUDE} ROCKSDB_LIBRARY=${ROCKSDB_LIBRARY}

# $(EXEC): $(wildcard *.cc) $(OBJECTS)
# 	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o $@

# clean:
# 	for dir in $(SUBDIRS); do \
# 		$(MAKE) -C $$dir $@; \
# 	done
# 	$(RM) $(EXEC)

# .PHONY: $(SUBDIRS) $(EXEC)

#######

CC=g++
CFLAGS=-std=c++17 -g -pthread
CXXFLAGS=-std=c++17 -g -pthread -I. -I..
INCLUDES=-I./
LDFLAGS= -lpthread -ltbb
SUBDIRS=core db third-party/HdrHistogram_c
SUBSRCS=$(wildcard core/*.cc) $(wildcard db/*.cc)
OBJECTS=$(SUBSRCS:.cc=.o)

TMPVAR := $(OBJECTS)
OBJECTS = $(filter-out db/rocksdb_db.o db/rocksdb_cloud_db.o db/db_factory.o, $(TMPVAR))

HDR_LIB=./third-party/HdrHistogram_c/hdr_lib.a
HDR_INCLUDES=-I./third-party/HdrHistogram_c/src
HDR_LDFLAGS=-lz -lm
HDR_CFLAGS=-Wno-unknown-pragmas -Wextra -Wshadow -Winit-self -Wpedantic -D_GNU_SOURCE -fPIC

ROCKSDB_LIB=/home/wjp/rocksdb-cloud/librocksdb.a
ROCKSDB_PLATFORM_CXXFLAGS=-DROCKSDB_USE_RTTI -g -W -Wextra -Wsign-compare -Wshadow -Werror -I. -I./include -std=c++17 -faligned-new -DHAVE_ALIGNED_NEW -DROCKSDB_PLATFORM_POSIX -DROCKSDB_LIB_IO_POSIX -DOS_LINUX -fno-builtin-memcmp -DROCKSDB_FALLOCATE_PRESENT -DSNAPPY -DGFLAGS=1 -DZLIB -DBZIP2 -DLZ4 -DZSTD -DTBB -DROCKSDB_MALLOC_USABLE_SIZE -DROCKSDB_PTHREAD_ADAPTIVE_MUTEX -DROCKSDB_BACKTRACE -DROCKSDB_RANGESYNC_PRESENT -DROCKSDB_SCHED_GETCPU_PRESENT -march=native -DHAVE_SSE42 -DHAVE_PCLMUL -DROCKSDB_SUPPORT_THREAD_LOCAL
ROCKSDB_PLATFORM_LDFLAGS=-lpthread -lrt -lz -ltbb
ROCKSDB_EXEC_LDFLAGS=-ldl
ROCKSDB_INCLUDES=-I/home/wjp/rocksdb-cloud/include

ROCKSDB_CLOUD_PLATFORM_CXXFLAGS=-faligned-new -DHAVE_ALIGNED_NEW -DROCKSDB_PLATFORM_POSIX -DROCKSDB_LIB_IO_POSIX -DOS_LINUX -fno-builtin-memcmp -DROCKSDB_FALLOCATE_PRESENT -DSNAPPY -DGFLAGS=1 -DZLIB -DBZIP2 -DLZ4 -DZSTD -DTBB -DROCKSDB_MALLOC_USABLE_SIZE -DROCKSDB_PTHREAD_ADAPTIVE_MUTEX -DROCKSDB_BACKTRACE -DROCKSDB_RANGESYNC_PRESENT -DROCKSDB_SCHED_GETCPU_PRESENT -march=native -DHAVE_SSE42 -DHAVE_PCLMUL -DROCKSDB_SUPPORT_THREAD_LOCAL -luring
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

