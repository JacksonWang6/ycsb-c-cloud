#ifndef YCSB_C_ROCKSDB_CLOUD_H
#define YCSB_C_ROCKSDB_CLOUD_H

#include "core/db.h"
#include <iostream>
#include <string>
#include "core/properties.h"
#include <hdr/hdr_histogram.h>
#include <fstream>
#include <sys/time.h>
#include <atomic>


#include <iostream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "core/properties.h"
#include "rocksdb/cloud/db_cloud.h"
#include "rocksdb/db.h"
#include "rocksdb/options.h"


using std::cout;
using std::endl;

namespace ycsbc {
    class RocksDBCloud : public DB {
    public :
        enum Layout {
            ALL_S3,
            Hyper,
            ALL_EBS
        };
        std::atomic<long long> qps;
        struct hdr_histogram* hdr_ = NULL;
        struct hdr_histogram* hdr_last_1s_ = NULL;
        struct hdr_histogram* hdr_get_= NULL;
        struct hdr_histogram* hdr_put_= NULL;
        struct hdr_histogram* hdr_update_ = NULL;
        struct hdr_histogram* hdr_scan_ = NULL;
        struct hdr_histogram* hdr_delete_ = NULL;
	    struct hdr_histogram* hdr_rmw_ = NULL;

        void latency_hiccup(uint64_t iops);
        std::FILE* f_hdr_output_;
        std::FILE* f_hdr_hiccup_output_;


        RocksDBCloud(const char *dbfilename, utils::Properties &props);
        int Read(const std::string &table, const std::string &key,
                 const std::vector<std::string> *fields,
                 std::vector<KVPair> &result);

        int Scan(const std::string &table, const std::string &key,
                 int len, const std::vector<std::string> *fields,
                 std::vector<std::vector<KVPair>> &result);

        int Insert(const std::string &table, const std::string &key,
                   std::vector<KVPair> &values);

        int Update(const std::string &table, const std::string &key,
                   std::vector<KVPair> &values);


        int Delete(const std::string &table, const std::string &key);

	    void RecordTime(int op,uint64_t tx_xtime);

        void PrintStats();

        bool HaveBalancedDistribution();

        uint64_t get_now_micros(){
            struct timeval tv;
            gettimeofday(&tv, NULL);
            return (tv.tv_sec) * 1000000 + tv.tv_usec;
        }

        ~RocksDBCloud();

    public:
        rocksdb::DBCloud* db_;
        unsigned noResult;

        void SetLocalOptions(rocksdb::Options *options, utils::Properties &props);
        void SetRocksdbEBSOptions(rocksdb::Options *options, utils::Properties &props, std::string& kDBPath);
        void SetHetCloudDBOptions(rocksdb::Options *options, utils::Properties &props, std::string& kDBPath);
        void SetRocksMashOptions(rocksdb::Options *options, utils::Properties &props, std::string& kDBPath);
        void SerializeValues(std::vector<KVPair> &kvs, std::string &value);
        void DeSerializeValues(std::string &value, std::vector<KVPair> &kvs);

    };
}


#endif //YCSB_C_ROCKSDB_CLOUD_H
