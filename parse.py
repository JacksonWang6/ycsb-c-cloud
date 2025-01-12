import re
import matplotlib.pyplot as plt
import seaborn as sns
import numpy as np
import matplotlib.ticker as ticker

def parse_yichu_xiangxingtu():
    # 定义一个函数来解析日志文件
    def parse_log_file(file_path):
        # 用于存储每个级别的 Size 数据
        levels = {'L0': [], 'L1': [], 'L2': [], 'L3': [], 'L4': []}
        
        # 打开日志文件
        with open(file_path, 'r') as file:
            for line in file:
                # 识别 L0 到 L4 行
                match = re.match(r"^  (L[0-4])\s+.*\s+(\d+\.\d+)\s+(GB|MB)", line)
                if match:
                    level = match.group(1)
                    size = float(match.group(2))
                    unit = match.group(3)
                    
                    # 如果单位是 MB，则将其转换为 GB
                    if unit == "MB":
                        size /= 1024  # 转换 MB 为 GB
                    
                    levels[level].append(size)
        
        return levels

    # 提取日志数据
    log_file = "./yichu/write_compaction_status_finish.log"  # 请替换为实际日志文件的路径
    levels_data = parse_log_file(log_file)

    # 准备绘制箱型图
    data = [levels_data['L0'], levels_data['L1'], levels_data['L2'], levels_data['L3'], levels_data['L4']]
    labels = ['L0', 'L1', 'L2', 'L3', 'L4']

    # 绘制箱型图
    plt.figure(figsize=(12, 8), dpi=200)
    sns.set_style('whitegrid')
    # 设置箱子颜色为白色，箱子线为黑色
    sns.boxplot(data=data, palette="binary", 
                boxprops=dict(facecolor='white', edgecolor='black'),  # 箱子内部为白色，线条为黑色
                whiskerprops=dict(color='black'),  # 箱线为黑色
                capprops=dict(color='black'),  # 顶部和底部线为黑色
                medianprops=dict(color='black'),
                showfliers=True)  # 中位线为黑色
    # sns.boxplot(data=data, palette="binary", 
    #             boxprops=dict(facecolor='white', color='black'),  # 箱体内部为白色，线条为黑色
    #             whiskerprops=dict(color='black'),  # 箱线为黑色
    #             capprops=dict(color='black'),  # 顶部和底部线为黑色
    #             medianprops=dict(color='black'))  # 中位线为黑色
    # 设置对数坐标轴
    plt.yscale('log')

    # 设置纵坐标的刻度为指定值 0.01, 0.1, 1, 10, 100
    plt.gca().yaxis.set_major_locator(ticker.LogLocator(base=10.0, subs=[], numticks=6))
    plt.gca().yaxis.set_major_formatter(ticker.FuncFormatter(lambda x, _: f'{x:g}'))

    # 设置纵坐标显示的具体值：0.01, 0.1, 1, 10, 100
    plt.yticks([0.01, 0.1, 1, 10, 100, 1000])
    plt.grid(False)

    # 标题和Y轴标签
    plt.title('')
    plt.xlabel('level', fontsize=14, fontweight='bold', family='serif')
    plt.ylabel('Size (GB)', fontsize=14, fontweight='bold', family='serif')
    plt.xticks(ticks=range(5), labels=labels)  # 使用 xticks 来设置 x 轴标签

    # 在箱型图上标出最大值和中位数，放在箱体的顶部
    for i, level_data in enumerate(data):
        # 计算中位数和最大值
        median_val = np.median(level_data)  # 中位数
        max_val = np.max(level_data)        # 最大值

        # 获取当前箱型图的 y 坐标范围，并增加一个更高的偏移量
        y_max = max(level_data) * 1.5  # 增加更多的空间，确保文本不会被遮挡
        
        # 在图上标注中位数和最大值，放置得更高
        # plt.text(i, median_val * 1.2, f'Median: {median_val:.2f} GB', 
        #          color='black', ha='center', va='bottom', fontsize=10)
        plt.text(i, max_val + 1, f'Max: {max_val:.2f} GB', 
                color='black', ha='center', va='bottom', fontsize=10)


    plt.gca().spines['top'].set_visible(False)  # 去掉上边坐标轴
    plt.gca().spines['right'].set_visible(False)  # 去掉右边坐标轴
    plt.gca().spines['left'].set_color('black')  # 设置左边坐标轴为黑色
    plt.gca().spines['bottom'].set_color('black')  # 设置下边坐标轴为黑色

    # 显示刻度线（但不显示坐标轴线）
    plt.tick_params(axis='x', direction='in', length=6, width=1, colors='black')
    plt.tick_params(axis='y', direction='in', length=6, width=1, colors='black')

    plt.savefig("yichu.jpg")


def parse_size_trend():
    # 定义一个函数来解析日志文件
    def parse_log_file(file_path):
        # 用于存储每个级别的 Size 数据
        levels = {'L0': [], 'L1': [], 'L2': [], 'L3': [], 'L4': []}
        timestamps = []  # 用于存储时间戳（即每次日志打印的时间点）
        
        timestamp = 0  # 初始时间点
        last_level = None

        # 打开日志文件
        with open(file_path, 'r') as file:
            for line in file:
                # 识别 L0 到 L4 行
                match = re.match(r"^  (L[0-4])\s+.*\s+(\d+\.\d+)\s+(GB|MB)", line)
                if match:
                    level = match.group(1)
                    size = float(match.group(2))
                    unit = match.group(3)

                    # 如果单位是 MB，则将其转换为 GB
                    if unit == "MB":
                        size /= 1024  # 转换 MB 为 GB

                    # 如果上一层级不是 None 并且当前层级不是期望的顺序，填充缺失的层级
                    if last_level is not None:
                        expected_level = f"L{(int(last_level[1]) + 1) % 5}"  # 循环L0-L4
                        while expected_level != level:  # 填充缺失的层级
                            levels[expected_level].append(0)
                            # timestamps.append(timestamp)
                            expected_level = f"L{(int(expected_level[1]) + 1) % 5}"

                    # 存储该层级的文件大小
                    levels[level].append(size)
                    last_level = level  # 更新当前层级

                # 每一批数据间的时间戳加10s
                if line.startswith(" Sum"):
                    timestamps.append(timestamp)
                    timestamp += 10  # 假设每隔10秒打印一批数据

        print(len(levels['L4']))
        print(len(timestamps))
        return levels, timestamps

    # 提取日志数据
    log_file = "./yichu/write_compaction_status.log"  # 请替换为实际日志文件的路径
    levels_data, timestamps = parse_log_file(log_file)

    # 绘制每个层级的文件大小随时间变化的趋势图
    plt.figure(figsize=(12, 8), dpi=200)
    plt.grid(False)

    # 设置不同层级的线条
    for level in levels_data:
        plt.plot(timestamps, levels_data[level], label=level)

    # 设置坐标轴标签和标题
    plt.xlabel('Time(s)')
    plt.ylabel('File Size (GB)')
    plt.title('')
    # 添加图例
    plt.legend(fontsize=12)
    plt.tick_params(axis='both', which='major', labelsize=12)

    # 显示图形
    # plt.grid(True)
    plt.show()
    plt.savefig("trend.jpg")

if __name__ == "__main__":
    parse_yichu_xiangxingtu()
    # parse_size_trend()
