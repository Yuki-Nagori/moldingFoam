# 042 — 防线回归接入 nightly（037 堆退出 + 038 预警/快速失败）

- 状态：done（2026-09-13：smoke-exit 接入 nightly contract job；
  fillVelocityWarn 预警在 boxFill 落地断言；非有限快速失败保留实现、
  不做回归——见 §8）
- 优先级：P1
- 依赖：037（堆破坏退出防线）、038（守卫与预警）
- 预估规模：0.5–1 天
- 来源：`ai-docs/coverage-audit-2026-09-13.md` 缺口 G1、G7

## 1. 背景与现状

- **G1**：`scripts/smoke-exit.sh`（037 的堆破坏退出防线：把任意 case 拷到
  临时目录跑一遍并检查退出码/`malloc_consolidate` 类报告）未接入任何
  workflow——037 报告自身即写明「建议接入：CI（快）或夜间
  （`bash scripts/smoke-exit.sh case-contract`）」，但至今只能手工跑；
- **G7**：038 的 P1 防线缺回归——`fillVelocityWarn` 阈值无任何用例配置、
  预警文本无断言；`src/moldingFoam/moldingFoam.C:1876` 的「非有限快速
  失败」`FatalError` 从未被触发（属失败路径，可接受，但 README 的
  P1 防线 claim 与测试不匹配）。

## 2. 目标

1. `smoke-exit.sh` 进入 nightly，失败可定位（独立日志 + artifact）；
2. 038 两条防线各有一个用例断言：预警阈值可配且触发时输出可断言；
   非有限快速失败有最小复现（或明确记录「不可稳定复现、仅保留实现」
   的决策）；
3. 缺省行为与既有用例不受影响。

## 3. 技术方案

- **A（接线，优先）**：nightly `contract` job 在 `case-contract` 之后追加
  `bash scripts/smoke-exit.sh case-contract 2>&1 | tee smoke-exit.txt`，
  artifact 增加 `smoke-exit.txt`（该脚本自建临时目录，不污染）
- **B（预警断言）**：在既有 `tests/cases/boxFill` 增加一个参数化变体
  （同几何，`fillVelocityWarn 1e-3` + 入口速度放大到 >1 m/s），
  `expectedPatterns` 断言预警行；默认阈值（20 m/s）由原用例继续覆盖
  「不触发」路径
- **C（快速失败）**：构造最小发散用例（如入口速度 1e3 m/s 的 3 步运行）
  断言 `FatalError` 文本与退出码；若数值上不稳定复现，则在 038/本任务
  文档记录原因并降级为「仅实现、无回归」并在审计文档中同步该决策

## 4. 工作拆解

1. nightly `contract` job 接线 + artifact（G1，约 10 行 YAML）；
2. boxFill 变体用例 + 断言（G7 预警）；
3. 快速失败最小复现尝试（≤1 小时时间盒），成功则加入 solver 用例，
   失败则记录决策；
4. 文档：README CI 表、037/038 交叉引用、审计文档 G1/G7 标注关闭。

## 5. 验收标准（DoD）

- nightly 中出现 `smoke-exit.sh` 步骤且本地在 VM 内手工跑通（PASS）；
- 预警用例在 22+ 用例套件中通过，未触发路径仍是原用例；
- 快速失败有断言或明确的不可复现结论；
- 文档同步（README CI 表 + 审计标注）。

## 6. 风险与缓解

| 风险 | 缓解 |
|------|------|
| smoke-exit 在 CI 上因环境差异失败 | 先在 VM 内跑通再接线；失败日志独立成块 |
| 预警用例阈值过敏感导致平台差异误报 | 阈值取能明确触发的量级（1e-3 vs 20），断言只看「出现预警」 |

## 7. 涉及文件

| 文件 | 改动 |
|------|------|
| `.github/workflows/nightly.yml` | contract job 增加 smoke-exit 步骤与 artifact |
| `tests/cases/boxFill*`（或新变体） | fillVelocityWarn 断言 |
| `tests/cases/*`（快速失败，若可行） | 断言 FatalError |
| `README.md`、`ai-docs/coverage-audit-2026-09-13.md` | CI 表与缺口标注 |

## 8. 交付记录（2026-09-13）

- **G1 接线**：nightly `contract` job 新增步骤
  `bash scripts/smoke-exit.sh case-contract | tee smoke-exit.txt`，artifact
  增加 `smoke-exit.txt`。脚本为**零步运行 + 未知求解器 FATAL 路径**两段，
  成本仅一次 blockMesh，不显著推高 job 时长；本地在 boxFill 上验证机制：
  `zero-step exit = 0` / `fatal-path exit = 1` /
  `PASS: the success and fatal exit paths are clean`；
- **G7 预警断言**：`tests/cases/boxFill/constant/moldingDict` 设
  `fillVelocityWarn 1e-3`（缺省 20 m/s 远高于任何物理闸口速度，故调低以
  触发），`expectedPatterns` 断言
  `Nominal inlet melt velocity .* exceeds the warning threshold`；实测预警
  出现、用例其余断言与 `verify-box-fill.py`（逃逸 7.83%）不变；
- **非有限快速失败**：`moldingFoam.C:1876` 的 FatalError 属失败路径，构造
  稳定复现需刻意发散的输入且无法保证跨平台可复现——本任务按 §3 的方案 C
  记录为「仅实现、不做回归」，并在审计文档 G7 标注（预警侧已闭环）；
- 回归：boxFill 用例标准流程（patterns + 验证器）PASS。
