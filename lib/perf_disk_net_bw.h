#pragma once

#include <chrono>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

struct NetStats {
  long long rxBytes; // 接收字节数
  long long txBytes; // 发送字节数
};

inline std::map<std::string, NetStats> getNetStats() {
  std::ifstream file("/proc/net/dev");
  std::map<std::string, NetStats> stats;
  std::string line;

  // 跳过前两行的标题
  std::getline(file, line);
  std::getline(file, line);

  while (std::getline(file, line)) {
    std::istringstream ss(line);
    std::string iface;
    long long rxBytes, txBytes;

    // 读取接口名称
    ss >> iface;
    if (!iface.empty() && iface.back() == ':') {
      iface.pop_back(); // 去除接口名称中的冒号
    }

    // 使用 std::vector 存储分割后的字段
    std::vector<std::string> fields;
    std::string field;
    while (ss >> field) {
      fields.push_back(field);
    }

    // 获取接收字节数和发送字节数
    rxBytes = std::stoll(fields[0]); // 接收字节数
    txBytes = std::stoll(
        fields[8]); // 发送字节数（索引从0开始，第9个字段是发送字节数）

    stats[iface] = {rxBytes, txBytes};
  }

  return stats;
}

const std::string DISK_NAME = "nvme1n1"; // 指定磁盘名称
const int SECTOR_SIZE = 512;             // 扇区大小，单位字节
// 用于存储磁盘的读写扇区数
struct DiskStats {
  long sectors_read = 0;
  long sectors_written = 0;
};

// 从 /proc/diskstats 中获取指定磁盘的读写扇区数
inline DiskStats get_disk_stats() {
  DiskStats stats;
  std::ifstream diskstats("/proc/diskstats");
  if (!diskstats.is_open()) {
    std::cerr << "Failed to open /proc/diskstats" << std::endl;
    return stats;
  }

  std::string line;
  while (std::getline(diskstats, line)) {
    std::istringstream iss(line);
    int major, minor;
    std::string disk_name;
    long reads_completed, reads_merged, sectors_read_tmp, time_reading;
    long writes_completed, writes_merged, sectors_written_tmp, time_writing;

    // 解析行格式：major minor name reads_completed reads_merged sectors_read
    // time_reading writes_completed writes_merged sectors_written time_writing
    if (!(iss >> major >> minor >> disk_name >> reads_completed >>
          reads_merged >> sectors_read_tmp >> time_reading >>
          writes_completed >> writes_merged >> sectors_written_tmp >>
          time_writing)) {
      continue;
    }

    if (disk_name == DISK_NAME) {
      stats.sectors_read = sectors_read_tmp;
      stats.sectors_written = sectors_written_tmp;
      return stats;
    }
  }

  std::cout << "Disk " << DISK_NAME << " not found in /proc/diskstats"
            << std::endl;
  return stats;
}

inline void calculateBandwidth(int intervalSeconds = 1) {
  const std::string &output_file = "bandwidth_output.log";
  std::ofstream outFile(output_file, std::ios_base::app); // 以追加模式打开文件
  if (!outFile.is_open()) {
    std::cerr << "Failed to open output file" << std::endl;
    return;
  }

  while (true) {
    auto net_stats1 = getNetStats();
    auto disk_stats1 = get_disk_stats();

    std::this_thread::sleep_for(std::chrono::seconds(intervalSeconds));
    auto net_stats2 = getNetStats();
    auto disk_stats2 = get_disk_stats();

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

      outFile << "Interface: " << iface << ",";
      outFile << "Read Bandwidth: " << rxBandwidth / 1024.0 / 1024
              << " MB/s,";
      outFile << "Send Bandwidth: " << txBandwidth / 1024.0 / 1024
              << " MB/s\n";
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

    outFile << "Device: " << DISK_NAME << ",";
    outFile << "Read Throughput: " << readThroughput / 1024.0 / 1024.0
            << " MB/s"; // 转换为 MB/s
    outFile << "Write Throughput: " << writeThroughput / 1024.0 / 1024.0
            << " MB/s\n"; // 转换为 MB/s

    outFile.flush();
  }
}
