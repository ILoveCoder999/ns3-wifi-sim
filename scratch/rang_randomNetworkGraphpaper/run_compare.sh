#!/bin/bash
# ─────────────────────────────────────────────────────────────────────────
#  run_compare.sh  —  数据中心网络拓扑对比执行脚本
#  用途：一键运行 topo-compare.cc，复现 RNG 论文 Figure 13 对比
#
#  使用前置条件：
#    1. 将 topo-compare.cc 放入 ns3 的 scratch/ 目录
#    2. 在 ns3 根目录下执行本脚本：  bash run_compare.sh
#
#  选项（可通过环境变量覆盖）：
#    NS3_DIR    ns3 根目录路径（默认 ./）
#    LINK_RATE  链路速率（默认 100Gbps）
#    QUEUE      队列深度（默认 8）
# ─────────────────────────────────────────────────────────────────────────
set -e

NS3_DIR="${NS3_DIR:-.}"
LINK_RATE="${LINK_RATE:-100Gbps}"
LINK_DELAY="${LINK_DELAY:-200ns}"
QUEUE="${QUEUE:-8}"
PKT_BYTES="${PKT_BYTES:-1024}"
BURST="${BURST:-64}"

RUN_CMD="$NS3_DIR/ns3 run"
SCRATCH_FILE="topo-compare"

# 颜色
CYAN='\033[0;36m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'; RESET='\033[0m'

banner() { echo -e "${CYAN}━━━  $1  ━━━${RESET}"; }

# ─── 参数解析 ──────────────────────────────────────────────────────────
MODE="quick"     # quick | full | figure13 | custom
TOPO="all"
PATTERN="all"
F="0.3"
SCAN_F=0

usage() {
cat <<EOF
用法: $0 [mode]

  mode:
    quick       快速对比（默认）：4 拓扑 × clique/matching × f=0.3
    full        完整对比：4 拓扑 × 3 模式 × f=0.1~1.0（约 15 分钟）
    figure13    复现论文 Figure 13：RNG vs Fat-Tree × 3 模式 × 全 f 扫描
    custom      自定义（见脚本末尾 CUSTOM_* 变量）

  环境变量：
    NS3_DIR     ns3 根目录（默认 ./）
    LINK_RATE   链路速率（默认 100Gbps）
    QUEUE       队列深度（默认 8）

示例：
    bash run_compare.sh quick
    bash run_compare.sh figure13
    NS3_DIR=/opt/ns3 bash run_compare.sh full
EOF
exit 0
}

[[ "$1" == "-h" || "$1" == "--help" ]] && usage
[[ -n "$1" ]] && MODE="$1"

# ─── 构建 ──────────────────────────────────────────────────────────────
banner "Step 1: Build topo-compare"
echo -e "${YELLOW}→ 检查 scratch/${SCRATCH_FILE}.cc 是否存在...${RESET}"

if [[ ! -f "$NS3_DIR/scratch/${SCRATCH_FILE}.cc" ]]; then
    echo -e "${YELLOW}⚠  未找到 scratch/${SCRATCH_FILE}.cc！"
    echo -e "   请将 topo-compare.cc 复制到 $NS3_DIR/scratch/ 后重试。${RESET}"
    exit 1
fi

echo "→ 编译..."
cd "$NS3_DIR"
./ns3 build "$SCRATCH_FILE" 2>&1 | tail -5
echo -e "${GREEN}✓ 构建成功${RESET}"

# ─── 定义运行函数 ──────────────────────────────────────────────────────
run_sim() {
    local topo="$1" pat="$2" f="$3" scan="$4"
    local args="--topo=$topo --pattern=$pat --f=$f --scanF=$scan"
    args="$args --linkRate=$LINK_RATE --linkDelay=$LINK_DELAY"
    args="$args --queuePkts=$QUEUE --pktBytes=$PKT_BYTES --burstPkts=$BURST"
    echo -e "${YELLOW}▶ $SCRATCH_FILE $args${RESET}"
    $RUN_CMD "$SCRATCH_FILE $args" 2>/dev/null
}

# 输出收集
TIMESTAMP=$(date +%Y%m%d_%H%M%S)
RESULT_DIR="compare_results_${TIMESTAMP}"
mkdir -p "$RESULT_DIR"

# ─── 根据模式执行 ──────────────────────────────────────────────────────
case "$MODE" in

  quick)
    banner "Step 2: Quick Comparison (4 topologies × 2 patterns × f=0.3)"
    echo "预计耗时：3~5 分钟"
    run_sim "all" "clique" "0.3" "0"
    cp topo_compare_results.csv "$RESULT_DIR/quick_clique_f0.3.csv" 2>/dev/null || true
    run_sim "all" "matching" "0.3" "0"
    cp topo_compare_results.csv "$RESULT_DIR/quick_matching_f0.3.csv" 2>/dev/null || true
    ;;

  full)
    banner "Step 2: Full Comparison (4 topologies × 3 patterns × f scan)"
    echo "预计耗时：15~30 分钟"
    for pat in clique hubs matching; do
        run_sim "all" "$pat" "0.3" "1"
        cp topo_compare_results.csv "$RESULT_DIR/full_${pat}.csv" 2>/dev/null || true
    done
    ;;

  figure13)
    banner "Step 2: Figure 13 Reproduction (RNG vs Fat-Tree × 3 patterns × f=0.1~1.0)"
    echo "预计耗时：10~20 分钟"
    echo ""
    echo -e "${CYAN}─── RNG topology ───${RESET}"
    run_sim "rng" "all" "0.3" "1"
    cp topo_compare_results.csv "$RESULT_DIR/fig13_rng.csv" 2>/dev/null || true

    echo ""
    echo -e "${CYAN}─── Fat-Tree topology ───${RESET}"
    run_sim "fattree" "all" "0.3" "1"
    cp topo_compare_results.csv "$RESULT_DIR/fig13_fattree.csv" 2>/dev/null || true

    # 合并并打印对比
    echo ""
    banner "Figure 13 Side-by-Side Comparison"
    echo ""
    echo -e "${YELLOW}论文预期结果（对应 Figure 13）：${RESET}"
    echo "  ✓ clique/hubs 大 f：RNG 过订阅比 < Fat-Tree（路径多样性优势）"
    echo "  ✓ matching  小 f：Fat-Tree 略优（最短路利用率高）"
    echo ""
    if command -v python3 >/dev/null 2>&1; then
        python3 - "$RESULT_DIR" <<'PYEOF'
import sys, csv, os
from collections import defaultdict

result_dir = sys.argv[1]
data = defaultdict(dict)

for fname in ['fig13_rng.csv', 'fig13_fattree.csv']:
    fpath = os.path.join(result_dir, fname)
    if not os.path.exists(fpath): continue
    with open(fpath) as f:
        for row in csv.DictReader(f):
            key = (row['topology'], row['pattern'], float(row['f']))
            data[key] = row

print(f"{'Pattern':<10} {'f':>5}  {'RNG oversub':>12}  {'FatTree oversub':>15}  {'Winner'}")
print('-'*65)
for pat in ['clique','hubs','matching']:
    for fv in [0.1,0.2,0.3,0.5,0.8,1.0]:
        rng_k = ('rng',pat,fv)
        ft_k  = ('fattree',pat,fv)
        if rng_k not in data or ft_k not in data: continue
        r_os = float(data[rng_k]['oversub_approx'])
        f_os = float(data[ft_k]['oversub_approx'])
        winner = 'RNG ✓' if r_os <= f_os else 'FatTree ✓'
        print(f"{pat:<10} {fv:>5.2f}  {r_os:>12.3f}  {f_os:>15.3f}  {winner}")
    print()
PYEOF
    fi
    ;;

  custom)
    banner "Step 2: Custom Run"
    # ── 自定义参数（在此修改）──
    CUSTOM_TOPO="${CUSTOM_TOPO:-rng}"
    CUSTOM_PAT="${CUSTOM_PAT:-clique}"
    CUSTOM_F="${CUSTOM_F:-0.5}"
    CUSTOM_SCAN="${CUSTOM_SCAN:-0}"
    run_sim "$CUSTOM_TOPO" "$CUSTOM_PAT" "$CUSTOM_F" "$CUSTOM_SCAN"
    ;;

  *)
    echo "未知模式: $MODE"
    usage
    ;;
esac

# ─── 收尾 ──────────────────────────────────────────────────────────────
cp topo_compare_results.csv "$RESULT_DIR/latest.csv" 2>/dev/null || true

echo ""
banner "Done"
echo -e "${GREEN}✓ 结果保存在：$RESULT_DIR/${RESET}"
echo ""
echo "快速查看命令："
echo "  cat topo_compare_results.csv"
if command -v column >/dev/null 2>&1; then
    echo "  column -t -s, topo_compare_results.csv"
fi
echo ""
echo "下次运行建议："
echo "  bash run_compare.sh quick    # 快速对比"
echo "  bash run_compare.sh figure13 # 完整论文对比"
