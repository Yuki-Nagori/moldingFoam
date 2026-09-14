# 057 — 性能收口：I/O 与诊断开销的量化，以及剩余旋钮盘点

- 状态：planned
- 优先级：P2
- 依赖：050（墙钟拆分）、041（内存流量）、048/052/053（已落地的杠杆）
- 预估规模：1 天
- 来源：050 的结论——通信 7–10%、界面/场运算 ~88%、字典层旋钮已测尽；
  剩下**从未量化**的两块开销：I/O 与诊断

## 1. 背景与现状

已测并已落地（勿重复探索）：

| 项 | 结论 | 出处 |
|----|------|------|
| MPI 通信 | 7–10%，不做上游通信改造 | 050 |
| 能量方程 | 占线性迭代 94%；容差 1e-6→1e-5 = −24% | 041 |
| 子循环/修正遍 | `nSubCycles 8` + `nCorrectors 1` → −30%（v1.29） | 048/052 |
| `maxAlphaCo` | 守恒 ∝ dt 一阶；dt 减半 = 余量 ×2、步数 ×2 | 047 |
| 自适应子循环表 | 阴性（−2%） | 053 |
| `MULESCorr yes` | 阴性（dt 塌缩 3×） | 052 |
| 界面机制（上游级） | 唯一剩下的结构性杠杆，收益未验证 | 050 |

**未量化**（本任务）：

1. **I/O**：契约 case `writeFormat ascii`、`writeInterval 0.1`、全字段写入；
   长周期/大件下写盘与解析开销占比（此前文档写"未测到显著占比"，但无数字）；
2. **`reportTrappedAir()`**：全局连通域洪水填充 + 每轮 halo 交换，每
   `trapAirInterval` 步一次——小 interval（1–10）时可能是可观开销；
   `trapAirInterval 0` 关闭时的差值即为该诊断的价格；
3. **`writeFillTime` 每步的 O(nCells) 局部循环**（预期便宜，一并量掉）。

## 2. 目标

1. 在契约 case（4 子域）上做**开关对照**，各 ≥2 样本、同会话：
   - 关闭 I/O：`writeControl timeStep; writeInterval 1e6`（或 `none`）；
   - 关闭/加大 trapAir：`trapAirInterval 0` 与 `200`→`2000`；
   - 关闭 `writeFillTime`；
   给出各自墙钟占比（%）与可落地的建议；
2. 若 I/O >2%：给出方案（`writeFormat binary`、按需写场、
   `purgeWrite`/`writeInterval` 建议），并在 `readme §7` 记录**写 I/O 的
   代价口径**（供 Kairos 侧决定写盘策略）；
3. 若 trapAir >2%：给方案（增量连通：只对变化单元重做 flood fill，
   或把诊断默认间隔调大并在文档写明代价）；
4. 产出**一页成本收口表**（已测/已落地/阴性/剩余），并入 README 第 7 节
   与 041 §4y，避免后续重复探索。

## 3. 技术方案

- 对照脚本复用 `scripts/lever-experiment.sh` 的"scratch 副本 + 同会话多
  样本"模式（新增 `--no-io`/`--trapAir N` 变体）；
- 墙钟用 `ExecutionTime` 末值（排除 blockMesh/decompose/reconstruct）；
- 守恒/验收照跑（I/O 关闭不影响解；trapAir 关闭不影响解，但会少写
  `airTrap` 场——在报告中标明诊断输出的差异）。

## 4. 验收标准（DoD）

- 三组开关对照表（墙钟/步数/守恒/验收）；
- 每项给出"是否值得做"的结论与方案（或阴性记录）；
- README §7 的"成本收口表"落地。

## 5. 风险与缓解

- VM 方差 ±11% → 同会话、多副本、结论以差值为主；
- trapAir 关闭会改变输出（缺 `airTrap` 场）→ 只用于测量，不改变缺省。

## 6. 涉及文件

- `scripts/lever-experiment.sh`（变体扩展）
- `README.md` §7、`ai-docs/tasks/041-memory-traffic-longterm.md`（收口表）

> 相关：050（剖面）、047/048/052/053（已测杠杆）。
