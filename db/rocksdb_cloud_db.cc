//
// Created by wujy on 1/23/19.
//
#include <iostream>

#include "rocksdb_cloud_db.h"
#include "lib/coding.h"
#include "rocksdb/status.h"
#include "rocksdb/filter_policy.h"
#include "rocksdb/table.h"
#include <hdr/hdr_histogram.h>
#include "rocksdb/persistent_cache.h"

#include <aws/core/Aws.h>
#include <aws/s3/S3Client.h>
#include <aws/core/auth/AWSCredentialsProviderChain.h>



using namespace std;
using namespace ROCKSDB_NAMESPACE;

namespace ycsbc
{

    void RocksDBCloud::latency_hiccup(uint64_t iops)
    {
        // fprintf(f_hdr_hiccup_output_, "mean     95th     99th     99.99th   IOPS");
        fprintf(f_hdr_hiccup_output_, "%-11.2lf %-8ld %-8ld %-8ld %-8lld\n",
                hdr_mean(hdr_last_1s_),
                hdr_value_at_percentile(hdr_last_1s_, 95),
                hdr_value_at_percentile(hdr_last_1s_, 99),
                hdr_value_at_percentile(hdr_last_1s_, 99.99),
                qps.load());
        hdr_reset(hdr_last_1s_);
        qps.store(0);
        fflush(f_hdr_hiccup_output_);
    }

    RocksDBCloud::RocksDBCloud(const char *dbfilename, utils::Properties &props) : noResult(0)
    {

        int r = hdr_init(1, INT64_C(3600000000), 3, &hdr_);
        r |= hdr_init(1, INT64_C(3600000000), 3, &hdr_last_1s_);
        r |= hdr_init(1, INT64_C(3600000000), 3, &hdr_get_);
        r |= hdr_init(1, INT64_C(3600000000), 3, &hdr_put_);
        r |= hdr_init(1, INT64_C(3600000000), 3, &hdr_update_);
        r |= hdr_init(1, INT64_C(3600000000), 3, &hdr_scan_);
        r |= hdr_init(1, INT64_C(3600000000), 3, &hdr_rmw_);

        if ((0 != r) || (NULL == hdr_) || (NULL == hdr_last_1s_) || (NULL == hdr_get_) || (NULL == hdr_put_) || (NULL == hdr_scan_) || (NULL == hdr_rmw_) || (NULL == hdr_update_) || (23552 < hdr_->counts_len))
        {
            cout << "DEBUG- init hdrhistogram failed." << endl;
            cout << "DEBUG- r=" << r << endl;
            cout << "DEBUG- histogram=" << &hdr_ << endl;
            cout << "DEBUG- counts_len=" << hdr_->counts_len << endl;
            cout << "DEBUG- counts:" << hdr_->counts << ", total_c:" << hdr_->total_count << endl;
            cout << "DEBUG- lowest:" << hdr_->lowest_discernible_value << ", max:" << hdr_->highest_trackable_value << endl;
            free(hdr_);
            exit(0);
        }

        f_hdr_output_ = std::fopen("./rocksdb-lat.hgrm", "w+");
        if (!f_hdr_output_)
        {
            std::perror("hdr output file opening failed");
            exit(0);
        }

        f_hdr_hiccup_output_ = std::fopen("./rocksdb-lat.hiccup", "w+");
        if (!f_hdr_hiccup_output_)
        {
            std::perror("hdr hiccup output file opening failed");
            exit(0);
        }
        fprintf(f_hdr_hiccup_output_, "#mean       95th    99th    99.99th    IOPS\n");

        // set option
        // rocksdb::Options options;
        // SetOptions(&options, props);

        // aws c++ sdk
        Aws::SDKOptions aws_options;
        Aws::InitAPI(aws_options); // Should only be called once.

        CloudFileSystemOptions cloud_fs_options;
        // Store a reference to a cloud file system. A new cloud env object should be
        // associated with every new cloud-db.
        std::shared_ptr<FileSystem> cloud_fs;

        // This is the local directory where the db is stored.
        std::string kDBPath = "/tmp/YCSB-C_rocksdb-cloud";
        std::string kBucketSuffix = "cloud-db-examples.wjp";
        std::string kRegion = "ap-northeast-2";
        // "rockset." is the default bucket prefix
        const std::string bucketPrefix = "rockset.";
        cloud_fs_options.src_bucket.SetBucketName(kBucketSuffix, bucketPrefix);
        cloud_fs_options.dest_bucket.SetBucketName(kBucketSuffix, bucketPrefix);

        // create a bucket name for debugging purposes
        const std::string bucketName = bucketPrefix + kBucketSuffix;
        std::cout << "kBucketSuffix: " << kBucketSuffix << " kDBPath: " << kDBPath << " kRegion: " << kRegion << " bucketName: " << bucketName << std::endl;

        // Create a new AWS cloud env Status
        CloudFileSystem* cfs;
        Status s = CloudFileSystemEnv::NewAwsFileSystem(
            FileSystem::Default(), kBucketSuffix, kDBPath, kRegion, kBucketSuffix,
            kDBPath, kRegion, cloud_fs_options, nullptr, &cfs);
        if (!s.ok()) {
            fprintf(stderr, "Unable to create cloud env in bucket %s. %s\n",
                    bucketName.c_str(), s.ToString().c_str());
        }
        cloud_fs.reset(cfs);

        // Create options and use the AWS file system that we created earlier
        auto cloud_env = NewCompositeEnv(cloud_fs);
        Options options;
        options.env = cloud_env.release();
        // cloud_env.release();
        options.create_if_missing = true;
        options.compaction_style = rocksdb::kCompactionStyleLevel;
        options.write_buffer_size = 110 << 10;  // 110KB
        options.arena_block_size = 4 << 10;
        options.level0_file_num_compaction_trigger = 2;
        options.max_bytes_for_level_base = 100 << 10; // 100KB

        // No persistent read-cache
        std::string persistent_cache = "";
        // options for each write
        WriteOptions wopt;
        wopt.disableWAL = false;
        // open DB
        DBCloud* db;
        s = DBCloud::Open(options, kDBPath, persistent_cache, 0, &db);
        if (!s.ok()) {
            fprintf(stderr, "Unable to open db at path %s with bucket %s. %s\n",
                    kDBPath.c_str(), bucketName.c_str(), s.ToString().c_str());
        }

        fprintf(stdout, "Successfully used db at path %s in bucket %s.\n",
                kDBPath.c_str(), bucketName.c_str());
        db_ = db;
        // end aws env init
    }

    void RocksDBCloud::SetOptions(rocksdb::Options *options, utils::Properties &props)
    {

        //// 默认的Rocksdb配置
        options->create_if_missing = true;
        options->compression = rocksdb::kNoCompression;
        options->enable_pipelined_write = true;

        rocksdb::BlockBasedTableOptions block_based_options;
        options->max_bytes_for_level_base = 256ul * 1024 * 1024;
        options->write_buffer_size = 64 * 1024 * 1024;
        options->target_file_size_base = 8 * 1024 * 1024;
        options->max_background_compactions = 4;
        options->max_background_flushes = 2;
        options->level0_file_num_compaction_trigger = 12;
        options->level0_slowdown_writes_trigger = 30;
        options->level0_stop_writes_trigger = 40;

        options->use_direct_reads = true;
        options->use_direct_io_for_flush_and_compaction = true;

        //// set block based cache 8k

        block_based_options.cache_index_and_filter_blocks = 0;
        std::shared_ptr<const rocksdb::FilterPolicy> filter_policy(rocksdb::NewBloomFilterPolicy(10, 0));
        block_based_options.filter_policy = filter_policy;
        // block_based_options.block_cache = rocksdb::NewLRUCache(1024 * 1024 * 1024);
        block_based_options.block_cache = rocksdb::NewLRUCache(8 * 1024 * 1024);
        // block_based_options.block_cache = rocksdb::NewLRUCache(8 * 1024);

        //
        int dboption = stoi(props["dboption"]);
        if (dboption == 0)
        {
            options->db_paths = {
                {"/home/ubuntu/ssd_150g/wp", 200l * 1024 * 1024 * 1024},
            };
        }
        else if (dboption == 1)
        { // RocksDBCloud
            // options->db_paths = {
            //     {"/home/ubuntu/gp2_50g_1", 60l * 1024 * 1024 * 1024},
            //     {"/home/ubuntu/gp2_50g_2", 60l * 1024 * 1024 * 1024},
            //     {"/home/ubuntu/gp2_50g_1_bak", 60l * 1024 * 1024 * 1024},
            //     {"/home/ubuntu/gp2_50g_2_bak", 60l * 1024 * 1024 * 1024},
            //     {"/home/ubuntu/gp3_100g_1", 60l * 1024 * 1024 * 1024},
            //     {"/home/ubuntu/gp3_100g_2", 60l * 1024 * 1024 * 1024},
            //     {"/home/ubuntu/gp3_100g_3", 60l * 1024 * 1024 * 1024},
            // };
            // options->db_paths = {
            //     {"/home/ubuntu/gp2_50g_1", 35l * 1024 * 1024 * 1024},
            //     {"/home/ubuntu/gp2_50g_2", 35l * 1024 * 1024 * 1024},
            //     {"/home/ubuntu/gp3_100g_1", 45l * 1024 * 1024 * 1024},
            //     {"/home/ubuntu/gp3_100g_2", 45l * 1024 * 1024 * 1024},
            //     {"/home/ubuntu/gp3_100g_3", 45l * 1024 * 1024 * 1024},
            // };
        }
        else if (dboption == 2)
        {
            // options->db_paths={
            //     {"/home/ubuntu/ssd_150g/db1", 100l * 1024 * 1024 * 1024},
            //     {"/home/ubuntu/ssd_150g/db2", 100l * 1024 * 1024 * 1024},
            //     {"/home/ubuntu/ssd_150g/db1_bak", 100l * 1024 * 1024 * 1024},
            //     {"/home/ubuntu/ssd_150g/db2_bak", 100l * 1024 * 1024 * 1024},
            //     {"/home/ubuntu/ssd_150g/db3", 100l * 1024 * 1024 * 1024},
            //     {"/home/ubuntu/ssd_150g/db4", 100l * 1024 * 1024 * 1024},
            //     {"/home/ubuntu/ssd_150g/db5", 100l * 1024 * 1024 * 1024},
            // };
            // options->db_paths = {
            //     {"/home/ubuntu/raid0", 100l * 1024 * 1024 * 1024},
            //     {"/home/ubuntu/raid1", 120l * 1024 * 1024 * 1024},
            // };
            options->db_paths = {
                {"/home/ubuntu/gp2_50g_1", 50l * 1024 * 1024 * 1024},
                {"/home/ubuntu/gp2_50g_2", 50l * 1024 * 1024 * 1024},
                {"/home/ubuntu/gp2_50g_1_bak", 50l * 1024 * 1024 * 1024},
                {"/home/ubuntu/gp2_50g_2_bak", 50l * 1024 * 1024 * 1024},
                {"/home/ubuntu/gp3_100g_1", 60l * 1024 * 1024 * 1024},
                {"/home/ubuntu/gp3_100g_2", 60l * 1024 * 1024 * 1024},
                {"/home/ubuntu/gp3_100g_3", 60l * 1024 * 1024 * 1024},
            };
            // options->db_paths = {
            //     {"/home/ubuntu/gp2_50g_1", 35l * 1024 * 1024 * 1024},
            //     {"/home/ubuntu/gp2_50g_2", 35l * 1024 * 1024 * 1024},
            //     {"/home/ubuntu/gp3_100g_1", 45l * 1024 * 1024 * 1024},
            //     {"/home/ubuntu/gp3_100g_2", 45l * 1024 * 1024 * 1024},
            //     {"/home/ubuntu/gp3_100g_3", 45l * 1024 * 1024 * 1024},
            // };
            // options->db_paths = {
            //     {"/home/ubuntu/gp2_50g_1", 200l * 1024 * 1024 * 1024},
            // };
            rocksdb::Status status;

            rocksdb::Env *env = rocksdb::Env::Default();
            // status = env->CreateDirIfMissing("/home/ubuntu/gp2_pcache");
            // status = env->CreateDirIfMissing("/home/ubuntu/gp2_50g_1/pcahce");
            assert(status.ok());
            std::shared_ptr<rocksdb::Logger> read_cache_logger;
            uint64_t pcache_size = 12 * 1024 * 1024 * 1024ul;
            // status = rocksdb::NewPersistentCache(env, "/home/ubuntu/gp2_50g_1/pcahce", pcache_size, read_cache_logger,
            //                                      false, &block_based_options.persistent_cache);
            status = rocksdb::NewPersistentCache(env, "/home/ubuntu/gp2_pcache", pcache_size, read_cache_logger,
                                                   false, &block_based_options.persistent_cache);
        }
        else if (dboption == 3)
        { // mutant
            options->db_paths = {
                {"/home/ubuntu/gp2_50g_1", 40l * 1024 * 1024 * 1024},
                {"/home/ubuntu/gp3_100g_1", 200l * 1024 * 1024 * 1024},
            };
        }
        else if (dboption == 4)
        { // two path and has cache
            printf("error not supported\n");

            //             options->db_paths = {{"/home/ubuntu/ssd/data/data1", 200L*1024*1024*1024},                                    {"/home/ubuntu/zyh/data/data2", 800L*1024*1024*1024}};

            // #ifdef PCACHE
            // 	    // set pcache
            // 	    printf("set pcache\n");
            //             rocksdb::Status status;
            //             rocksdb::Env* env = rocksdb::Env::Default();
            //             status = env->CreateDirIfMissing("/home/ubuntu/ssd/data/pcache");
            //             assert(status.ok());
            //             std::shared_ptr<rocksdb::Logger> read_cache_logger;
            // 	    uint64_t pcache_size = 6.25*1024*1024*1024ul;
            //             status = rocksdb::NewPersistentmyCache(env,"/home/ubuntu/ssd/data/pcache",pcache_size, read_cache_logger,
            //                             true, &block_based_options.persistent_cache);
            //             assert(status.ok());
            // #endif
        }

        options->table_factory.reset(
            rocksdb::NewBlockBasedTableFactory(block_based_options));
    }

    int RocksDBCloud::Read(const std::string &table, const std::string &key, const std::vector<std::string> *fields,
                      std::vector<KVPair> &result)
    {
        string value;
        rocksdb::Status s = db_->Get(rocksdb::ReadOptions(), key, &value);

        if (s.ok())
        {
            DeSerializeValues(value, result);
            return DB::kOK;
        }
        if (s.IsNotFound())
        {
            noResult++;
            // cerr<<"read not found:"<<noResult<<endl;
            return DB::kOK;
        }
        else
        {
            cerr << "read error" << endl;
            exit(0);
        }
    }

    int RocksDBCloud::Scan(const std::string &table, const std::string &key, int len, const std::vector<std::string> *fields,
                      std::vector<std::vector<KVPair>> &result)
    {
        auto it = db_->NewIterator(rocksdb::ReadOptions());
        it->Seek(key);
        std::string val;
        std::string k;
        for (int i = 0; i < len && it->Valid(); i++)
        {
            k = it->key().ToString();
            val = it->value().ToString();
            it->Next();
        }
        delete it;
        return DB::kOK;
    }

    int RocksDBCloud::Insert(const std::string &table, const std::string &key,
                        std::vector<KVPair> &values)
    {
        // fprintf(stderr,"Insert\n");
        rocksdb::Status s;
        string value;
        SerializeValues(values, value);
        s = db_->Put(rocksdb::WriteOptions(), key, value);

        if (!s.ok())
        {
            cerr << "insert error\n"
                 << endl;
            exit(0);
        }
        // fprintf(stderr,"Insert End\n");
        return DB::kOK;
    }

    int RocksDBCloud::Update(const std::string &table, const std::string &key, std::vector<KVPair> &values)
    {
        return Insert(table, key, values);
    }

    int RocksDBCloud::Delete(const std::string &table, const std::string &key)
    {
        rocksdb::Status s;
        s = db_->Delete(rocksdb::WriteOptions(), key);
        if (!s.ok())
        {
            cerr << "Delete error\n"
                 << endl;
            exit(0);
        }
        return DB::kOK;
    }

    void RocksDBCloud::PrintStats()
    {
        cout << "read not found:" << noResult << endl;
        string stats;
        db_->GetProperty("rocksdb.stats", &stats);
        cout << stats << endl;

        cout << "-----------------------------------------------------" << endl;
        cout << "SUMMARY latency (us) of this run with HDR measurement" << endl;
        cout << "         ALL      GET      PUT      UPD      SCAN    RMW" << endl;
        fprintf(stdout, "mean     %-8.3lf %-8.3lf %-8.3lf %-8.3lf %8.3lf %8.3lf\n",
                hdr_mean(hdr_),
                hdr_mean(hdr_get_),
                hdr_mean(hdr_put_),
                hdr_mean(hdr_update_),
                hdr_mean(hdr_scan_),
                hdr_mean(hdr_rmw_));

        fprintf(stdout, "95th     %-8ld %-8ld %-8ld %-8ld %-8ld %-8ld\n",
                hdr_value_at_percentile(hdr_, 95),
                hdr_value_at_percentile(hdr_get_, 95),
                hdr_value_at_percentile(hdr_put_, 95),
                hdr_value_at_percentile(hdr_update_, 95),
                hdr_value_at_percentile(hdr_scan_, 95),
                hdr_value_at_percentile(hdr_rmw_, 95));
        fprintf(stdout, "99th     %-8ld %-8ld %-8ld %-8ld %-8ld %-8ld\n",
                hdr_value_at_percentile(hdr_, 99),
                hdr_value_at_percentile(hdr_get_, 99),
                hdr_value_at_percentile(hdr_put_, 99),
                hdr_value_at_percentile(hdr_update_, 99),
                hdr_value_at_percentile(hdr_scan_, 99),
                hdr_value_at_percentile(hdr_rmw_, 99));
        fprintf(stdout, "99.99th  %-8ld %-8ld %-8ld %-8ld %-8ld %-8ld\n",
                hdr_value_at_percentile(hdr_, 99.99),
                hdr_value_at_percentile(hdr_get_, 99.99),
                hdr_value_at_percentile(hdr_put_, 99.99),
                hdr_value_at_percentile(hdr_update_, 99.99),
                hdr_value_at_percentile(hdr_scan_, 99.99),
                hdr_value_at_percentile(hdr_rmw_, 99.99));

        int ret = hdr_percentiles_print(hdr_, f_hdr_output_, 5, 1.0, CLASSIC);
        if (0 != ret)
        {
            cout << "hdr percentile output print file error!" << endl;
        }
        cout << "-------------------------------" << endl;
    }

    bool RocksDBCloud::HaveBalancedDistribution()
    {
        return true;
        // return db_->HaveBalancedDistribution();
    }

    RocksDBCloud::~RocksDBCloud()
    {
        printf("wait delete db\n");

        free(hdr_);
        free(hdr_last_1s_);
        free(hdr_get_);
        free(hdr_put_);
        free(hdr_update_);
        free(hdr_scan_);
        free(hdr_rmw_);

        delete db_;
        printf("delete\n");
    }

    void RocksDBCloud::RecordTime(int op, uint64_t tx_xtime)
    {
        if (tx_xtime > 3600000000)
        {
            cout << "too large tx_xtime" << endl;
        }
        qps++;
        hdr_record_value(hdr_, tx_xtime);
        hdr_record_value(hdr_last_1s_, tx_xtime);

        if (op == 1)
        {
            hdr_record_value(hdr_put_, tx_xtime);
        }
        else if (op == 2)
        {
            hdr_record_value(hdr_get_, tx_xtime);
        }
        else if (op == 3)
        {
            hdr_record_value(hdr_update_, tx_xtime);
        }
        else if (op == 4)
        {
            hdr_record_value(hdr_scan_, tx_xtime);
        }
        else if (op == 5)
        {
            hdr_record_value(hdr_rmw_, tx_xtime);
        }
        else
        {
            cout << "record time err with op error" << endl;
        }
    }

    void RocksDBCloud::SerializeValues(std::vector<KVPair> &kvs, std::string &value)
    {
        value.clear();
        PutFixed64(&value, kvs.size());
        for (unsigned int i = 0; i < kvs.size(); i++)
        {
            PutFixed64(&value, kvs[i].first.size());
            value.append(kvs[i].first);
            PutFixed64(&value, kvs[i].second.size());
            value.append(kvs[i].second);
        }
    }

    void RocksDBCloud::DeSerializeValues(std::string &value, std::vector<KVPair> &kvs)
    {
        uint64_t offset = 0;
        uint64_t kv_num = 0;
        uint64_t key_size = 0;
        uint64_t value_size = 0;

        kv_num = DecodeFixed64(value.c_str());
        offset += 8;
        for (unsigned int i = 0; i < kv_num; i++)
        {
            ycsbc::DB::KVPair pair;
            key_size = DecodeFixed64(value.c_str() + offset);
            offset += 8;

            pair.first.assign(value.c_str() + offset, key_size);
            offset += key_size;

            value_size = DecodeFixed64(value.c_str() + offset);
            offset += 8;

            pair.second.assign(value.c_str() + offset, value_size);
            offset += value_size;
            kvs.push_back(pair);
        }
    }
}
