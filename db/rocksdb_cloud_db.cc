//
// Created by wujy on 1/23/19.
//
#include <cstdint>
#include <cstdio>
#include <iostream>

#include "rocksdb_cloud_db.h"
#include "core/db.h"
#include "lib/coding.h"
#include "rocksdb/status.h"
#include "rocksdb/filter_policy.h"
#include "rocksdb/table.h"
#include "rocksdb/statistics.h"
#include <hdr/hdr_histogram.h>
#include "rocksdb/persistent_cache.h"

#include <aws/core/Aws.h>
#include <aws/s3/S3Client.h>
#include <aws/core/auth/AWSCredentialsProviderChain.h>
#include <iostream>
#include <thread>
#include <chrono>
#include <unistd.h>

#include "lib/perf_disk_net_bw.h"


using namespace std;
using namespace ROCKSDB_NAMESPACE;

namespace ycsbc
{
    std::thread my_stat_thr;
    std::thread rocksdb_stat_thr;
    std::atomic<bool> done = false;
    std::atomic<bool> finish = false;
    std::string my_stat_output_file = "./a_my_log/zipfian_read/run_h2_my_stat.log";
    std::string rocksdb_stat_output_file = "./a_my_log/zipfian_read/run_h2_rocks_stat.log";
    uint64_t user_write_bytes_snap = 0;
    uint64_t user_read_bytes_snap = 0;
    uint64_t number_keys_write_snap = 0;
    uint64_t number_keys_read_snap = 0;

    uint64_t number_db_seek_snap = 0;
    uint64_t number_db_next_snap = 0;
    uint64_t number_db_prev_snap = 0;
    uint64_t iter_bytes_read_snap = 0;
    uint64_t writer_stall_snap = 0;
    uint64_t total_time = 0;

    constexpr uint64_t kMB = 1024 * 1024;

    void print_summary(rocksdb::DB* db, std::ofstream& outFile) {
        uint64_t bytes_read = db->GetOptions().statistics->getTickerCount(BYTES_READ);
        uint64_t bytes_write = db->GetOptions().statistics->getTickerCount(BYTES_WRITTEN);
        uint64_t number_keys_write = db->GetOptions().statistics->getTickerCount(NUMBER_KEYS_WRITTEN);
        uint64_t number_keys_read = db->GetOptions().statistics->getTickerCount(NUMBER_KEYS_READ);
        uint64_t number_db_seek = db->GetOptions().statistics->getTickerCount(NUMBER_DB_SEEK_FOUND);
        uint64_t number_db_next = db->GetOptions().statistics->getTickerCount(NUMBER_DB_NEXT_FOUND);
        uint64_t number_db_prev = db->GetOptions().statistics->getTickerCount(NUMBER_DB_PREV_FOUND);
        uint64_t iter_bytes_read = db->GetOptions().statistics->getTickerCount(ITER_BYTES_READ);
        auto writer_stall = db->GetOptions().statistics->getTickerCount(STALL_MICROS);

        outFile << "***my summary***" << endl;
        outFile << "total run time: " << total_time << "s" << endl;
        outFile << "user read size: " << bytes_read*1.0/kMB << "MB, avg user read throughput: " << bytes_read*1.0/kMB/total_time << "MB/s" << endl;
        outFile << "user write size: " << bytes_write*1.0/kMB << "MB, avg user write throughput: " << bytes_write*1.0/kMB/total_time << "MB/s" << endl;
        outFile << "key write op: " << number_keys_write*1.0/1000/1000 << "Mop, avg key write ops: " << number_keys_write*1.0/total_time/1000 << "kop/s" << endl;
        outFile << "key read op: " << number_keys_read*1.0/1000/1000 << "Mop, avg key read ops: " << number_keys_read*1.0/total_time/1000 << "kop/s" << endl;
        outFile << "seek op: " << number_db_seek*1.0/1000 << "Kop, avg seek ops: " << number_db_seek*1.0/total_time/1000 << "kop/s" << endl;
        outFile << "next op: " << number_db_next*1.0/1000 << "Kop, avg next ops: " << number_db_next*1.0/total_time/1000 << "kop/s" << endl;
        outFile << "prev op: " << number_db_prev*1.0/1000 << "Kop, avg prev ops: " << number_db_prev*1.0/total_time/1000 << "kop/s" << endl;

        // level hit status
        auto memhit = db->GetOptions().statistics->getTickerCount(MEMTABLE_HIT);
        auto memmiss = db->GetOptions().statistics->getTickerCount(MEMTABLE_MISS);
        auto l0hit = db->GetOptions().statistics->getTickerCount(GET_HIT_L0);
        auto l1hit = db->GetOptions().statistics->getTickerCount(GET_HIT_L1);
        auto l2hit = db->GetOptions().statistics->getTickerCount(GET_HIT_L2);
        auto l3hit = db->GetOptions().statistics->getTickerCount(GET_HIT_L3);
        auto l4anduphit = db->GetOptions().statistics->getTickerCount(GET_HIT_L4_AND_UP);
        outFile << "Level hit status: memtable hit " << memhit << ", memtable miss " << memmiss << ", L0 " << l0hit << ", L1 " << l1hit << ", L2 " << l2hit << ", L3 " << l3hit << ", L4andup" << l4anduphit << endl;

        // read his
        auto read_his = db->GetOptions().statistics->getHistogramString(DB_GET);
        outFile << "Read Histogram: " << read_his << endl;

        // write his
        auto write_his = db->GetOptions().statistics->getHistogramString(DB_WRITE);
        outFile << "Write Histogram: " << write_his << endl;

        // seek his
        auto seek_his = db->GetOptions().statistics->getHistogramString(DB_SEEK);
        outFile << "Seek Histogram: " << seek_his << endl;

        // sst read his
        auto sst_read_his = db->GetOptions().statistics->getHistogramString(SST_READ_MICROS);
        outFile << "SST Read Histogram: " << sst_read_his << endl;

        // compaction his
        auto compact_his = db->GetOptions().statistics->getHistogramString(COMPACTION_TIME);
        outFile << "Compaction Histogram: " << compact_his << endl;

        outFile << db->GetOptions().statistics->ToString() << endl;
        outFile.flush();
    }

    void print_my_status(rocksdb::DB* db) {
        uint64_t intervalSeconds = 1;
        const std::string &output_file = my_stat_output_file;
        std::ofstream outFile = std::ofstream(output_file, std::ios_base::app); // 以追加模式打开文件
        if (!outFile.is_open()) {
            std::cerr << "Failed to open output file" << std::endl;
            return;
        }

        while (true) {
            auto net_stats1 = getNetStats();
            auto disk_stats1 = get_disk_stats();

            // 每隔intervalSeconds输出一次
            std::this_thread::sleep_for(std::chrono::seconds(intervalSeconds));
            total_time += intervalSeconds;
            if (done) {
                print_summary(db, outFile);
                break;
            }
            auto net_stats2 = getNetStats();
            auto disk_stats2 = get_disk_stats();

            outFile << "-------------------->\n";
            // 计算网络带宽
            for (const auto &stat : net_stats1) {
            const auto &iface = stat.first;
            std::string str = iface;
            if (str.find("ens5") == std::string::npos) {
                continue;
            }
            long long rxDiff = net_stats2.at(iface).rxBytes - stat.second.rxBytes;
            long long txDiff = net_stats2.at(iface).txBytes - stat.second.txBytes;

            // 计算带宽 (字节/秒)
            double rxBandwidth =
                rxDiff / static_cast<double>(intervalSeconds); // 接收带宽
            double txBandwidth =
                txDiff / static_cast<double>(intervalSeconds); // 发送带宽

            outFile << "NetDevice: " << iface << ",";
            outFile << "Read Bandwidth: " << rxBandwidth / 1024.0 / 1024 << " MB/s,";
            outFile << "Send Bandwidth: " << txBandwidth / 1024.0 / 1024 << " MB/s\n";
            }

            // 计算磁盘带宽
            long long readDiff = disk_stats2.sectors_read - disk_stats1.sectors_read;
            long long writeDiff =
                disk_stats2.sectors_written - disk_stats1.sectors_written;

            // 每个 sector 一般为 512 字节
            double readThroughput =
                (readDiff * 512) /
                static_cast<double>(intervalSeconds); // 读取吞吐量 (字节/秒)
            double writeThroughput =
                (writeDiff * 512) /
                static_cast<double>(intervalSeconds); // 写入吞吐量 (字节/秒)

            outFile << "Disk Device: " << DISK_NAME << ",";
            outFile << "Read Throughput: " << readThroughput / 1024.0 / 1024.0
                    << " MB/s"; // 转换为 MB/s
            outFile << "Write Throughput: " << writeThroughput / 1024.0 / 1024.0
                    << " MB/s\n"; // 转换为 MB/s


            // 计算rocksdb这一秒的ops，吞吐量，时延信息
            uint64_t bytes_read = db->GetOptions().statistics->getTickerCount(BYTES_READ);
            uint64_t bytes_write = db->GetOptions().statistics->getTickerCount(BYTES_WRITTEN);
            uint64_t number_keys_write = db->GetOptions().statistics->getTickerCount(NUMBER_KEYS_WRITTEN);
            uint64_t number_keys_read = db->GetOptions().statistics->getTickerCount(NUMBER_KEYS_READ);
            uint64_t number_db_seek = db->GetOptions().statistics->getTickerCount(NUMBER_DB_SEEK_FOUND);
            uint64_t number_db_next = db->GetOptions().statistics->getTickerCount(NUMBER_DB_NEXT_FOUND);
            uint64_t number_db_prev = db->GetOptions().statistics->getTickerCount(NUMBER_DB_PREV_FOUND);
            uint64_t iter_bytes_read = db->GetOptions().statistics->getTickerCount(ITER_BYTES_READ);

            uint64_t bytes_read_delta = bytes_read - user_read_bytes_snap;
            uint64_t bytes_write_delta = bytes_write - user_write_bytes_snap;
            uint64_t number_keys_write_delta = number_keys_write - number_keys_write_snap;
            uint64_t number_keys_read_delta = number_keys_read - number_keys_read_snap;
            uint64_t number_db_seek_delta = number_db_seek - number_db_seek_snap;
            uint64_t number_db_next_delta = number_db_next - number_db_next_snap;
            uint64_t number_db_prev_delta = number_db_prev - number_db_prev_snap;
            uint64_t iter_bytes_read_delta = iter_bytes_read - iter_bytes_read_snap;

            outFile << "Rocksdb R/W status: \n";
            outFile << "write status:> total write op " << number_keys_write*1.0/1000/1000 << "Mops, interval write data size: " << bytes_write_delta*1.0/kMB << "MB, write throughput: " << bytes_write_delta*1.0/kMB/intervalSeconds << "MB/s, write ops: " << number_keys_write_delta*1.0/intervalSeconds << "op/s" << endl;

            outFile << "read status:> total read op " << number_keys_read*1.0/1000/1000 << "Mops, interval read data size: " << bytes_read_delta*1.0/kMB << "MB, read throughput: " << bytes_read_delta*1.0/kMB/intervalSeconds << "MB/s, read ops: " << number_keys_read_delta*1.0/intervalSeconds << "op/s" << endl;

            outFile << "iter status:> iter read size: " << iter_bytes_read_delta*1.0/kMB << "MB, iter read throughput: " << iter_bytes_read_delta*1.0/kMB/intervalSeconds << "MB/s, iter seek ops: " << number_db_seek_delta*1.0/intervalSeconds << "op/s, iter next ops: " << number_db_next_delta*1.0/intervalSeconds << "op/s, iter prev ops: " << number_db_prev_delta << "op/s" << endl;

            user_write_bytes_snap = bytes_write;
            user_read_bytes_snap = bytes_read;
            number_keys_write_snap = number_keys_write;
            number_keys_read_snap = number_keys_read;
            number_db_seek_snap = number_db_seek;
            number_db_next_snap = number_db_next;
            number_db_prev_snap = number_db_prev;
            iter_bytes_read_snap = iter_bytes_read;

            // add more statistics
            auto data_block_hit = db->GetOptions().statistics->getTickerCount(BLOCK_CACHE_DATA_HIT);
            auto data_block_miss = db->GetOptions().statistics->getTickerCount(BLOCK_CACHE_DATA_MISS);

            outFile << "BlockCache status: hit " << data_block_hit << ", miss " << data_block_miss << ", hit ritio " << data_block_hit*1.0/(data_block_hit+data_block_miss) << endl;

            // bloom filter
            auto useful = db->GetOptions().statistics->getTickerCount(BLOOM_FILTER_USEFUL);
            auto positive = db->GetOptions().statistics->getTickerCount(BLOOM_FILTER_FULL_POSITIVE);
            auto true_positive = db->GetOptions().statistics->getTickerCount(BLOOM_FILTER_FULL_TRUE_POSITIVE);
            auto false_positive = positive - true_positive;
            outFile << "BloomFilter status: useful " << useful << ", positive " << positive << ", true_positive " << true_positive << ", false_positive " << false_positive << " false positive ratio " << false_positive*1.0/(useful+positive) << endl;

            // S3 status
            std::string res;
            db->GetProperty("rocksdb.blob-stats", &res); // 修改了rocksdb内部rocksdb.blob-stats的打印信息
            outFile << res;

            // writer stall status
            // auto writer_stall = db->GetOptions().statistics->getTickerCount(STALL_MICROS);
            // auto writer_stall_delta = writer_stall - writer_stall_snap;
            // outFile << "Writer stall status: stall " << writer_stall_delta*1.0 / 1000 << "ms" << endl;
            // writer_stall_snap = writer_stall;

            outFile << "<--------------------\n";
            outFile.flush();
        }
    }

    void printRocksDBStats(rocksdb::DB* db) {
        const std::string &output_file = rocksdb_stat_output_file;
        std::ofstream outFile = std::ofstream(output_file, std::ios_base::app); // 以追加模式打开文件
        if (!outFile.is_open()) {
            std::cerr << "Failed to open output file" << std::endl;
            return;
        }
        while (true) {
            uint64_t interval = 10;
            std::this_thread::sleep_for(std::chrono::seconds(interval));
            if (done) {
                break;
            }
            if (finish) {
                outFile << "write finished, wait compaction finished" << endl;
                finish = false;
            }
            outFile << ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>" << endl;
            std::string res;
            db->GetProperty("rocksdb.stats", &res);
            outFile << res << endl;
            
            res.clear();
            db->GetProperty("rocksdb.block-cache-entry-stats", &res);
            outFile << res << endl;
            outFile << "<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<" << endl;

            outFile.flush();
        }
    }

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
        aws_options.httpOptions.installSigPipeHandler = true; // fix bug(加入这一行可能够了，也可能和openssl的版本也有关)
        Aws::InitAPI(aws_options); // Should only be called once.

        CloudFileSystemOptions cloud_fs_options;
        // Store a reference to a cloud file system. A new cloud env object should be
        // associated with every new cloud-db.
        std::shared_ptr<FileSystem> cloud_fs;

        // This is the local directory where the db is stored.
        std::string MY_LOCAL_DBPATH = "/tmp/rocksdb_cloud_ycsb";
        std::string MY_CLOUD_DBPATH = "/home/ubuntu/gp3/hyper_8M_1000w";
        std::string kDBPath = MY_CLOUD_DBPATH;
        std::string kBucketSuffix = "hyper.8m.1000w";
        std::string kRegion = "ap-northeast-2";
        const std::string bucketPrefix = "rockset.";
        cloud_fs_options.src_bucket.SetBucketName(kBucketSuffix, bucketPrefix);
        cloud_fs_options.dest_bucket.SetBucketName(kBucketSuffix, bucketPrefix);
        cloud_fs_options.keep_local_sst_files = false;

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
        options.create_if_missing = true;
        options.statistics = rocksdb::CreateDBStatistics();
        SetOptions(&options, props);
        // SetLocalOptions(&options, props);
        // SetLargeOptions(&options, props);
        SetSpecialOptions(&options, kDBPath, Layout::Hyper);

        // No persistent read-cache
        std::string persistent_cache = "";
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

        // 启动后台线程，定期打印statistic
        my_stat_thr = std::thread(print_my_status, db_);
        rocksdb_stat_thr = std::thread(printRocksDBStats, db_);

    }

    void RocksDBCloud::SetLocalOptions(rocksdb::Options *options, utils::Properties &props)
    {
        options->create_if_missing = true;
        options->compression = rocksdb::kNoCompression;
        options->compaction_style = rocksdb::kCompactionStyleLevel;
        options->enable_pipelined_write = true;
        options->use_direct_reads = true;
        options->use_direct_io_for_flush_and_compaction = true;

        // L0和L1 10MB
        options->max_bytes_for_level_base = 10ul * 1024 * 1024;
        // memtable 1MB
        options->write_buffer_size = 1 * 1024 * 1024;
        // 单个文件的大小1MB
        options->target_file_size_base = 1 * 1024 * 1024;
        options->max_background_compactions = 8;
        options->max_background_flushes = 2;
        // options->level0_file_num_compaction_trigger = 10;
        // options->level0_slowdown_writes_trigger = 40;
        // options->level0_stop_writes_trigger = 60;

        rocksdb::BlockBasedTableOptions block_based_options;
        block_based_options.cache_index_and_filter_blocks = 0;
        std::shared_ptr<const rocksdb::FilterPolicy> filter_policy(rocksdb::NewBloomFilterPolicy(10, 0));
        block_based_options.filter_policy = filter_policy;
        block_based_options.block_cache = rocksdb::NewLRUCache(4 * 1024 * 1024);
        block_based_options.block_size = 16 * 1024;

        options->table_factory.reset(
            rocksdb::NewBlockBasedTableFactory(block_based_options));
    }

    void RocksDBCloud::SetOptions(rocksdb::Options *options, utils::Properties &props)
    {
        options->create_if_missing = true;
        options->compression = rocksdb::kNoCompression;
        options->compaction_style = rocksdb::kCompactionStyleLevel;
        options->enable_pipelined_write = true;
        options->use_direct_reads = true;
        options->use_direct_io_for_flush_and_compaction = true;

        // L0和L1 64MB
        options->max_bytes_for_level_base = 64ul * 1024 * 1024;
        // memtable 8MB
        options->write_buffer_size = 8 * 1024 * 1024;
        // 单个文件的大小8MB
        options->target_file_size_base = 8 * 1024 * 1024;
        // 8个后台Compaction线程，2个Flush线程
        options->max_background_compactions = 8;
        options->max_background_flushes = 2;
        options->level0_file_num_compaction_trigger = 8;
        options->level0_slowdown_writes_trigger = 20;
        options->level0_stop_writes_trigger = 36;
        options->compaction_readahead_size = 4 * 1024 * 1024;


        rocksdb::BlockBasedTableOptions block_based_options;
        block_based_options.initial_auto_readahead_size = 256 * 1024;
        block_based_options.max_auto_readahead_size = 4 * 1024 * 1024;
        block_based_options.cache_index_and_filter_blocks = 0;
        std::shared_ptr<const rocksdb::FilterPolicy> filter_policy(rocksdb::NewBloomFilterPolicy(10, 0));
        block_based_options.filter_policy = filter_policy;
        // 256MB的块缓存
        block_based_options.block_cache = rocksdb::NewLRUCache(256 * 1024 * 1024);
        block_based_options.block_size = 16 * 1024;
        options->table_factory.reset(
            rocksdb::NewBlockBasedTableFactory(block_based_options));
    }

    void RocksDBCloud::SetLargeOptions(rocksdb::Options *options, utils::Properties &props)
    {
        options->create_if_missing = true;
        options->compression = rocksdb::kNoCompression;
        options->compaction_style = rocksdb::kCompactionStyleLevel;
        options->enable_pipelined_write = true;
        options->use_direct_reads = true;
        options->use_direct_io_for_flush_and_compaction = true;

        // L0和L1 256MB
        options->max_write_buffer_number = 4;
        options->max_bytes_for_level_base = 256ul * 1024 * 1024;
        // memtable 64MB
        options->write_buffer_size = 64 * 1024 * 1024;
        // 单个文件的大小64MB
        options->target_file_size_base = 64 * 1024 * 1024;
        // 8个后台Compaction线程，2个Flush线程
        options->max_background_compactions = 8;
        options->max_background_flushes = 2;
        // options->level0_file_num_compaction_trigger = 4;
        // options->level0_slowdown_writes_trigger = 20;
        // options->level0_stop_writes_trigger = 36;
        options->compaction_readahead_size = 4 * 1024 * 1024;


        rocksdb::BlockBasedTableOptions block_based_options;
        block_based_options.initial_auto_readahead_size = 256 * 1024;
        block_based_options.max_auto_readahead_size = 4 * 1024 * 1024;
        block_based_options.cache_index_and_filter_blocks = 0;
        std::shared_ptr<const rocksdb::FilterPolicy> filter_policy(rocksdb::NewBloomFilterPolicy(10, 0));
        block_based_options.filter_policy = filter_policy;
        // 256MB的块缓存
        block_based_options.block_cache = rocksdb::NewLRUCache(256 * 1024 * 1024);
        block_based_options.block_size = 16 * 1024;
        options->table_factory.reset(
            rocksdb::NewBlockBasedTableFactory(block_based_options));
    }


    void RocksDBCloud::SetSpecialOptions(rocksdb::Options *options, std::string& kDBPath, enum Layout layout) {
        if (layout == Layout::ALL_S3) {
            // 全部放在S3上
            options->hyper_level = 7;
            options->db_paths = {
                {kDBPath + "/s3", 1024l * 1024 * 1024 * 1024},
                {kDBPath + "/ebs", 1024l * 1024 * 1024 * 1024},
            };
        } else if (layout  == Layout::Hyper) {
            // L0-L2在EBS，其余的在S3
            options->hyper_level = 2;
            options->db_paths = {
                {kDBPath + "/ebs", 1024l * 1024 * 1024 * 1024},
                {kDBPath + "/s3", 1024l * 1024 * 1024 * 1024},
            };
        } else if (layout == Layout::ALL_EBS) {
            // 全部在EBS
            options->hyper_level = 7;
            options->db_paths = {
                {kDBPath + "/ebs", 1024l * 1024 * 1024 * 1024},
                {kDBPath + "/s3", 1024l * 1024 * 1024 * 1024},
            };
        }
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

        // db_->Flush(FlushOptions());
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
