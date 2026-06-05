#!/bin/bash

# 确保脚本在出错时退出
set -e

# 【终极修正】直接使用官方最标准的单体文件 build 映射，由 ns-3 核心在 scratch 树中自动搜寻 fat-tree-sim
echo "正在针对 fat-tree-sim 进行单体无冲突编译..."
../../ns3 build fat_tree_sim

# 清空或初始化 CSV 文件
CSV_FILE="fattree_all_scenarios_result.csv"
if [ -f "$CSV_FILE" ]; then
    rm "$CSV_FILE"
fi

# 定义需要测试的论文流量模式
PATTERNS=("clique" "hubs" "matching")

# 定义论文 Figure 13 的 x 轴活跃比例采样点
F_VALUES=(0.05 0.1 0.2 0.3 0.4 0.5 0.6 0.7 0.8 0.9 1.0)

echo "=================================================="
echo "开始自动化重新测试：复现 Figure 13 过订阅比曲线"
echo "=================================================="

for pattern in "${PATTERNS[@]}"; do
    for f in "${F_VALUES[@]}"; do
        echo "正在运行场景: scenario=$pattern, f=$f ..."
        
        # 运行对应的仿真二进制（使用你的原文件名，去掉.cc）
        ../../ns3 run "fat_tree_sim --scenario=$pattern --f=$f --pktsPerFlow=300"
        
        echo "--------------------------------------------------"
    done
done

echo "=================================================="
echo "测试全部完成！结果已追加保存至: $CSV_FILE"
echo "=================================================="